// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
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
   {"star-cups-driver",                                                      \
    {{"env_version", "1.1"},                                                 \
     {"sha2hashstr",                                                         \
      "6d24de30f671da5aee6d463d9e446cafe9ddac672800a9defe86877dcde6c466"}}}};

using content::BrowserThread;

namespace component_updater {

using ConfigMap = std::map<std::string, std::map<std::string, std::string>>;

void LogRegistrationResult(base::Optional<bool> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!result.has_value()) {
    DVLOG(1) << "Call to imageloader service failed.";
    return;
  }
  if (!result.value()) {
    DVLOG(1) << "Component registration failed";
    return;
  }
}

void ImageLoaderRegistration(const std::string& version,
                             const base::FilePath& install_dir,
                             const std::string& name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::ImageLoaderClient* loader =
      chromeos::DBusThreadManager::Get()->GetImageLoaderClient();
  if (loader) {
    loader->RegisterComponent(name, version, install_dir.value(),
                              base::BindOnce(&LogRegistrationResult));
  } else {
    DVLOG(1) << "Failed to get ImageLoaderClient object.";
  }
}

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
  std::string version;
  if (!manifest.GetString("version", &version)) {
    return ToInstallerResult(update_client::InstallError::GENERIC_ERROR);
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ImageLoaderRegistration, version, install_dir, name));
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void CrOSComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    std::unique_ptr<base::DictionaryValue> manifest) {
  std::string min_env_version;
  if (manifest && manifest->GetString("min_env_version", &min_env_version)) {
    if (IsCompatible(env_version, min_env_version)) {
      g_browser_process->platform_part()->AddCompatibleCrOSComponent(GetName());
    }
  }
}

bool CrOSComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath CrOSComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(name);
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

// It returns load result passing the following as parameters in
// load_callback: call_status - dbus call status, result - component mount
// point.
static void LoadResult(
    base::OnceCallback<void(const std::string&)> load_callback,
    base::Optional<std::string> result) {
  PostTask(FROM_HERE, base::BindOnce(std::move(load_callback),
                                     result.value_or(std::string())));
}

// Internal function to load a component.
static void LoadComponentInternal(
    const std::string& name,
    base::OnceCallback<void(const std::string&)> load_callback) {
  DCHECK(g_browser_process->platform_part()->IsCompatibleCrOSComponent(name));
  chromeos::ImageLoaderClient* loader =
      chromeos::DBusThreadManager::Get()->GetImageLoaderClient();
  if (loader) {
    loader->LoadComponent(
        name, base::BindOnce(&LoadResult, std::move(load_callback)));
  } else {
    base::PostTask(FROM_HERE, base::BindOnce(std::move(load_callback), ""));
  }
}

// It calls LoadComponentInternal to load the installed component.
static void InstallResult(
    const std::string& name,
    base::OnceCallback<void(const std::string&)> load_callback,
    update_client::Error error) {
  LoadComponentInternal(name, std::move(load_callback));
}

// It calls OnDemandUpdate to install the component right after being
// registered.
void CrOSComponent::RegisterResult(ComponentUpdateService* cus,
                                   const std::string& id,
                                   update_client::Callback install_callback) {
  cus->GetOnDemandUpdater().OnDemandUpdate(id, std::move(install_callback));
}

// Register a component with a dedicated ComponentUpdateService instance.
static void RegisterComponent(ComponentUpdateService* cus,
                              const ComponentConfig& config,
                              base::OnceClosure register_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<ComponentInstallerPolicy> policy(
      new CrOSComponentInstallerPolicy(config));
  // |cus| will take ownership of |installer| during
  // installer->Register(cus).
  ComponentInstaller* installer = new ComponentInstaller(std::move(policy));
  installer->Register(cus, std::move(register_callback));
}

// Install a component with a dedicated ComponentUpdateService instance.
void CrOSComponent::InstallComponent(
    ComponentUpdateService* cus,
    const std::string& name,
    base::OnceCallback<void(const std::string&)> load_callback) {
  const ConfigMap components = CONFIG_MAP_CONTENT;
  const auto it = components.find(name);
  if (name.empty() || it == components.end()) {
    base::PostTask(FROM_HERE, base::BindOnce(std::move(load_callback), ""));
    return;
  }
  ComponentConfig config(it->first, it->second.find("env_version")->second,
                         it->second.find("sha2hashstr")->second);
  RegisterComponent(cus, config,
                    base::BindOnce(RegisterResult, cus,
                                   crx_file::id_util::GenerateIdFromHex(
                                       it->second.find("sha2hashstr")->second)
                                       .substr(0, 32),
                                   base::BindOnce(InstallResult, name,
                                                  std::move(load_callback))));
}

void CrOSComponent::LoadComponent(
    const std::string& name,
    base::OnceCallback<void(const std::string&)> load_callback) {
  if (!g_browser_process->platform_part()->IsCompatibleCrOSComponent(name)) {
    // A compatible component is not installed, start installation process.
    auto* const cus = g_browser_process->component_updater();
    InstallComponent(cus, name, std::move(load_callback));
  } else {
    // A compatible component is intalled, load it directly.
    LoadComponentInternal(name, std::move(load_callback));
  }
}

std::vector<ComponentConfig> CrOSComponent::GetInstalledComponents() {
  std::vector<ComponentConfig> configs;
  base::FilePath root;
  if (!PathService::Get(DIR_COMPONENT_USER, &root))
    return configs;

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

void CrOSComponent::RegisterComponents(
    const std::vector<ComponentConfig>& configs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  component_updater::ComponentUpdateService* updater =
      g_browser_process->component_updater();
  for (const auto& config : configs) {
    RegisterComponent(updater, config, base::Closure());
  }
}

}  // namespace component_updater
