// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/component_updater/component_installer_errors.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"

// ConfigMap list-initialization expression for all downloadable
// Chrome OS components.
#define CONFIG_MAP_CONTENT                                                   \
  {{"epson-inkjet-printer-escpr",                                            \
    {{"env_version", "2.1"},                                                 \
     {"sha2hashstr",                                                         \
      "1913a5e0a6cad30b6f03e176177e0d7ed62c5d6700a9c66da556d7c3f5d6a47e"}}}, \
   {"cros-termina",                                                          \
    {{"env_version", "1.1"},                                                 \
     {"sha2hashstr",                                                         \
      "e9d960f84f628e1f42d05de4046bb5b3154b6f1f65c08412c6af57a29aecaffb"}}}, \
   {"rtanalytics-light",                                                     \
    {{"env_version", "1.0"},                                                 \
     {"sha2hashstr",                                                         \
      "69f09d33c439c2ab55bbbe24b47ab55cb3f6c0bd1f1ef46eefea3216ec925038"}}}, \
   {"rtanalytics-full",                                                      \
    {{"env_version", "1.0"},                                                 \
     {"sha2hashstr",                                                         \
      "c93c3e1013c52100a20038b405ac854d69fa889f6dc4fa6f188267051e05e444"}}}, \
   {"star-cups-driver",                                                      \
    {{"env_version", "1.1"},                                                 \
     {"sha2hashstr",                                                         \
      "6d24de30f671da5aee6d463d9e446cafe9ddac672800a9defe86877dcde6c466"}}}, \
   {"cros-cellular",                                                         \
    {{"env_version", "1.0"},                                                 \
     {"sha2hashstr",                                                         \
      "5714811c04f0a63aac96b39096faa759ace4c04e9b68291e7c9716128f5a2722"}}}};

#define COMPONENTS_ROOT_PATH "cros-components"

namespace component_updater {

namespace {
// TODO(xiaochu): add metrics for component usage (crbug.com/793052).
void LogCustomUninstall(base::Optional<bool> result) {}

std::string GenerateId(const std::string& sha2hashstr) {
  // kIdSize is the count of a pair of hex in the sha2hash array.
  // In string representation of sha2hash, size is doubled since each hex is
  // represented by a single char.
  return crx_file::id_util::GenerateIdFromHex(
      sha2hashstr.substr(0, crx_file::id_util::kIdSize * 2));
}

void CleanUpOldInstalls(const std::string& name) {
  // Clean up components installed at old path.
  base::FilePath path;
  if (!PathService::Get(DIR_COMPONENT_USER, &path))
    return;
  path = path.Append(name);
  if (base::PathExists(path))
    base::DeleteFile(path, true);
}
}  // namespace

using ConfigMap = std::map<std::string, std::map<std::string, std::string>>;

ComponentConfig::ComponentConfig(const std::string& name,
                                 const std::string& env_version,
                                 const std::string& sha2hashstr)
    : name(name), env_version(env_version), sha2hashstr(sha2hashstr) {}
ComponentConfig::~ComponentConfig() {}

CrOSComponentInstallerPolicy::CrOSComponentInstallerPolicy(
    const ComponentConfig& config)
    : name(config.name), env_version(config.env_version) {
  if (config.sha2hashstr.length() != 64)
    return;
  auto strstream = config.sha2hashstr;
  for (auto& cell : kSha2Hash_) {
    cell = stoul(strstream.substr(0, 2), nullptr, 16);
    strstream.erase(0, 2);
  }
}

bool CrOSComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool CrOSComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return true;
}

update_client::CrxInstaller::Result
CrOSComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  // TODO(xiaochu): remove this at M66 (crbug.com/792203).
  CleanUpOldInstalls(name);

  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void CrOSComponentInstallerPolicy::OnCustomUninstall() {
  g_browser_process->platform_part()
      ->cros_component_manager()
      ->UnregisterCompatiblePath(name);

  chromeos::DBusThreadManager::Get()->GetImageLoaderClient()->UnmountComponent(
      name, base::BindOnce(&LogCustomUninstall));
}

void CrOSComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    std::unique_ptr<base::DictionaryValue> manifest) {
  std::string min_env_version;
  if (manifest && manifest->GetString("min_env_version", &min_env_version)) {
    if (IsCompatible(env_version, min_env_version)) {
      g_browser_process->platform_part()
          ->cros_component_manager()
          ->RegisterCompatiblePath(GetName(), path);
    }
  }
}

bool CrOSComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath CrOSComponentInstallerPolicy::GetRelativeInstallDir() const {
  base::FilePath path = base::FilePath(COMPONENTS_ROOT_PATH);
  return path.Append(name);
}

void CrOSComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kSha2Hash_, kSha2Hash_ + arraysize(kSha2Hash_));
}

std::string CrOSComponentInstallerPolicy::GetName() const {
  return name;
}

update_client::InstallerAttributes
CrOSComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attrs;
  attrs["_env_version"] = env_version;
  return attrs;
}

std::vector<std::string> CrOSComponentInstallerPolicy::GetMimeTypes() const {
  std::vector<std::string> mime_types;
  return mime_types;
}

bool CrOSComponentInstallerPolicy::IsCompatible(
    const std::string& env_version_str,
    const std::string& min_env_version_str) {
  base::Version env_version(env_version_str);
  base::Version min_env_version(min_env_version_str);
  return env_version.IsValid() && min_env_version.IsValid() &&
         env_version.components()[0] == min_env_version.components()[0] &&
         env_version >= min_env_version;
}

CrOSComponentManager::CrOSComponentManager() {}

CrOSComponentManager::~CrOSComponentManager() {}

void CrOSComponentManager::Load(
    const std::string& name,
    base::OnceCallback<void(const base::FilePath&)> load_callback) {
  if (!IsCompatible(name)) {
    // A compatible component is not installed, start installation process.
    auto* const cus = g_browser_process->component_updater();
    Install(cus, name, std::move(load_callback));
  } else {
    // A compatible component is intalled, load it directly.
    LoadInternal(name, std::move(load_callback));
  }
}

bool CrOSComponentManager::Unload(const std::string& name) {
  const ConfigMap components = CONFIG_MAP_CONTENT;
  const auto it = components.find(name);
  if (it == components.end()) {
    // Component |name| does not exist.
    return false;
  }
  component_updater::ComponentUpdateService* updater =
      g_browser_process->component_updater();
  const std::string id = GenerateId(it->second.find("sha2hashstr")->second);
  return updater->UnregisterComponent(id);
}

void CrOSComponentManager::RegisterInstalled() {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&component_updater::CrOSComponentManager::GetInstalled,
                     base::Unretained(this)),
      base::BindOnce(&component_updater::CrOSComponentManager::RegisterN,
                     base::Unretained(this)));
}

void CrOSComponentManager::RegisterCompatiblePath(const std::string& name,
                                                  const base::FilePath& path) {
  compatible_components_[name] = path;
}

void CrOSComponentManager::UnregisterCompatiblePath(const std::string& name) {
  compatible_components_.erase(name);
}

bool CrOSComponentManager::IsCompatible(const std::string& name) const {
  return compatible_components_.count(name) > 0;
}

base::FilePath CrOSComponentManager::GetCompatiblePath(
    const std::string& name) const {
  const auto it = compatible_components_.find(name);
  return it == compatible_components_.end() ? base::FilePath() : it->second;
}

void CrOSComponentManager::Register(ComponentUpdateService* cus,
                                    const ComponentConfig& config,
                                    base::OnceClosure register_callback) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<CrOSComponentInstallerPolicy>(config));
  installer->Register(cus, std::move(register_callback));
}

void CrOSComponentManager::Install(
    ComponentUpdateService* cus,
    const std::string& name,
    base::OnceCallback<void(const base::FilePath&)> load_callback) {
  const ConfigMap components = CONFIG_MAP_CONTENT;
  const auto it = components.find(name);
  if (it == components.end()) {
    base::PostTask(FROM_HERE,
                   base::BindOnce(std::move(load_callback), base::FilePath()));
    return;
  }
  ComponentConfig config(it->first, it->second.find("env_version")->second,
                         it->second.find("sha2hashstr")->second);
  Register(cus, config,
           base::BindOnce(&CrOSComponentManager::StartInstall,
                          base::Unretained(this), cus,
                          GenerateId(it->second.find("sha2hashstr")->second),
                          base::BindOnce(&CrOSComponentManager::FinishInstall,
                                         base::Unretained(this), name,
                                         std::move(load_callback))));
}

void CrOSComponentManager::StartInstall(
    ComponentUpdateService* cus,
    const std::string& id,
    update_client::Callback install_callback) {
  cus->GetOnDemandUpdater().OnDemandUpdate(id, std::move(install_callback));
}

void CrOSComponentManager::FinishInstall(
    const std::string& name,
    base::OnceCallback<void(const base::FilePath&)> load_callback,
    update_client::Error error) {
  LoadInternal(name, std::move(load_callback));
}

void CrOSComponentManager::LoadInternal(
    const std::string& name,
    base::OnceCallback<void(const base::FilePath&)> load_callback) {
  DCHECK(IsCompatible(name));
  const base::FilePath path = GetCompatiblePath(name);
  // path is empty if no compatible component is available to load.
  if (!path.empty()) {
    chromeos::DBusThreadManager::Get()
        ->GetImageLoaderClient()
        ->LoadComponentAtPath(
            name, path,
            base::BindOnce(&CrOSComponentManager::FinishLoad,
                           base::Unretained(this), std::move(load_callback)));
  } else {
    base::PostTask(FROM_HERE,
                   base::BindOnce(std::move(load_callback), base::FilePath()));
  }
}

void CrOSComponentManager::FinishLoad(
    base::OnceCallback<void(const base::FilePath&)> load_callback,
    base::Optional<base::FilePath> result) {
  PostTask(FROM_HERE, base::BindOnce(std::move(load_callback),
                                     result.value_or(base::FilePath())));
}

std::vector<ComponentConfig> CrOSComponentManager::GetInstalled() {
  std::vector<ComponentConfig> configs;
  base::FilePath root;
  if (!PathService::Get(DIR_COMPONENT_USER, &root))
    return configs;

  root = root.Append(COMPONENTS_ROOT_PATH);
  const ConfigMap components = CONFIG_MAP_CONTENT;
  for (auto it : components) {
    const std::string& name = it.first;
    const std::map<std::string, std::string>& props = it.second;
    base::FilePath component_path = root.Append(name);
    if (base::PathExists(component_path)) {
      ComponentConfig config(name, props.find("env_version")->second,
                             props.find("sha2hashstr")->second);
      configs.push_back(config);
    }
  }
  return configs;
}

void CrOSComponentManager::RegisterN(
    const std::vector<ComponentConfig>& configs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  component_updater::ComponentUpdateService* updater =
      g_browser_process->component_updater();
  for (const auto& config : configs) {
    Register(updater, config, base::OnceClosure());
  }
}

}  // namespace component_updater
