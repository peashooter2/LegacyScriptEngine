#include "PluginMigration.h"

#include "Entry.h"

#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <ll/api/plugin/Manifest.h>
#include <ll/api/plugin/Plugin.h>
#include <ll/api/reflection/Serialization.h>
#include <unordered_set>

#if LEGACY_SCRIPT_ENGINE_BACKEND_LUA

constexpr auto PluginExtName = ".lua";

#endif

#ifdef LEGACY_SCRIPT_ENGINE_BACKEND_QUICKJS

constexpr auto PluginExtName = ".js";

#endif

namespace lse {

auto migratePlugin(const std::filesystem::path& path) -> void {
    auto& self = getSelfPluginInstance();

    auto& logger = self.getLogger();

    logger.info("migrating legacy plugin at {}", path.string());

    auto& pluginManager = getPluginManager();

    const auto& pluginType = pluginManager.getType();

    const auto& pluginName     = path.filename();
    const auto& pluginBaseName = path.stem();
    const auto& pluginDir      = ll::plugin::getPluginsRoot() / pluginBaseName;

    if (std::filesystem::exists(pluginDir / path.filename())) {
        throw std::runtime_error(
            fmt::format("failed to migrate legacy plugin at {}: {} already exists", path.string(), pluginDir.string())
        );
    }

    if (!std::filesystem::create_directory(pluginDir) && !std::filesystem::exists(pluginDir)) {
        throw std::runtime_error(fmt::format("failed to create directory {}", pluginDir.string()));
    }

    // Move plugin file.
    std::filesystem::rename(path, pluginDir / pluginName);

    ll::plugin::Manifest manifest{
        .entry = pluginName.string(),
        .name  = pluginBaseName.string(),
        .type  = pluginType,
        .dependencies =
            std::unordered_set<ll::plugin::Dependency>{
                                                       ll::plugin::Dependency{
                    .name = self.getManifest().name,
                }, },
    };

    auto manifestJson = ll::reflection::serialize<nlohmann::ordered_json>(manifest);

    std::ofstream manifestFile{pluginDir / "manifest.json"};
    manifestFile << manifestJson.dump(4);
}

auto migratePlugins() -> void {
    auto& self = getSelfPluginInstance();

    auto& logger = self.getLogger();

    std::unordered_set<std::filesystem::path> pluginPaths;

    // Discover plugins.
    logger.info("discovering legacy plugins...");

    const auto& pluginBaseDir = ll::plugin::getPluginsRoot();

    for (const auto& entry : std::filesystem::directory_iterator(pluginBaseDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != PluginExtName) {
            continue;
        }

        pluginPaths.insert(entry.path());
    }

    if (pluginPaths.empty()) {
        logger.info("no legacy plugin found, skipping migration");

        return;
    }

    // Migrate plugins.
    logger.info("migrating legacy plugins...");

    for (const auto& path : pluginPaths) {
        migratePlugin(path);
    }

    logger.warn("legacy plugins have been migrated, please restart the server to load them!");
}

} // namespace lse
