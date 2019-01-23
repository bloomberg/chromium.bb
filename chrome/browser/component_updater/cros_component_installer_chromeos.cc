// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"

#include <map>
#include <utility>

#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_installer_errors.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

namespace component_updater {

namespace {

// Root path where all components are stored.
constexpr char kComponentsRootPath[] = "cros-components";

// All downloadable Chrome OS components.
const ComponentConfig kConfigs[] = {
    {"epson-inkjet-printer-escpr", "3.0",
     "1913a5e0a6cad30b6f03e176177e0d7ed62c5d6700a9c66da556d7c3f5d6a47e"},
    {"cros-termina", "730.1",
     "e9d960f84f628e1f42d05de4046bb5b3154b6f1f65c08412c6af57a29aecaffb"},
    {"rtanalytics-light", "9.0",
     "69f09d33c439c2ab55bbbe24b47ab55cb3f6c0bd1f1ef46eefea3216ec925038"},
    {"rtanalytics-full", "9.0",
     "c93c3e1013c52100a20038b405ac854d69fa889f6dc4fa6f188267051e05e444"},
    {"star-cups-driver", "1.1",
     "6d24de30f671da5aee6d463d9e446cafe9ddac672800a9defe86877dcde6c466"},
    {"cros-cellular", "1.0",
     "5714811c04f0a63aac96b39096faa759ace4c04e9b68291e7c9716128f5a2722"},
    {"demo-mode-resources", "1.0",
     "93c093ebac788581389015e9c59c5af111d2fa5174d206eb795042e6376cbd10"},
};

const ComponentConfig* FindConfig(const std::string& name) {
  const ComponentConfig* config = std::find_if(
      std::begin(kConfigs), std::end(kConfigs),
      [&name](const ComponentConfig& config) { return config.name == name; });
  if (config == std::end(kConfigs))
    return nullptr;
  return config;
}

// TODO(xiaochu): add metrics for component usage (https://crbug.com/793052).
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
  if (!base::PathService::Get(DIR_COMPONENT_USER, &path))
    return;
  path = path.Append(name);
  if (base::PathExists(path))
    base::DeleteFile(path, true);
}

// Returns all installed components.
std::vector<ComponentConfig> GetInstalled() {
  std::vector<ComponentConfig> configs;
  base::FilePath root;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &root))
    return configs;

  root = root.Append(kComponentsRootPath);
  for (const ComponentConfig& config : kConfigs) {
    base::FilePath component_path = root.Append(config.name);
    if (base::PathExists(component_path))
      configs.push_back(config);
  }
  return configs;
}

// Report Error code.
CrOSComponentManager::Error ReportError(CrOSComponentManager::Error error) {
  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.ChromeOS.InstallResult", error,
                            CrOSComponentManager::Error::ERROR_MAX);
  return error;
}

}  // namespace

CrOSComponentInstallerPolicy::CrOSComponentInstallerPolicy(
    const ComponentConfig& config,
    CrOSComponentInstaller* cros_component_installer)
    : cros_component_installer_(cros_component_installer),
      name_(config.name),
      env_version_(config.env_version) {
  if (strlen(config.sha2hash) != crypto::kSHA256Length * 2)
    return;

  bool converted = base::HexStringToBytes(config.sha2hash, &sha2_hash_);
  DCHECK(converted);
  DCHECK_EQ(crypto::kSHA256Length, sha2_hash_.size());
}

CrOSComponentInstallerPolicy::~CrOSComponentInstallerPolicy() = default;

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
  // TODO(xiaochu): remove after M66 ships to stable. https://crbug.com/792203
  CleanUpOldInstalls(name_);

  cros_component_installer_->EmitInstalledSignal(GetName());

  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void CrOSComponentInstallerPolicy::OnCustomUninstall() {
  cros_component_installer_->UnregisterCompatiblePath(name_);

  chromeos::DBusThreadManager::Get()->GetImageLoaderClient()->UnmountComponent(
      name_, base::BindOnce(&LogCustomUninstall));
}

void CrOSComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    std::unique_ptr<base::DictionaryValue> manifest) {
  std::string min_env_version;
  if (!manifest || !manifest->GetString("min_env_version", &min_env_version))
    return;

  if (!IsCompatible(env_version_, min_env_version))
    return;

  cros_component_installer_->RegisterCompatiblePath(GetName(), path);
}

bool CrOSComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath CrOSComponentInstallerPolicy::GetRelativeInstallDir() const {
  base::FilePath path = base::FilePath(kComponentsRootPath);
  return path.Append(name_);
}

void CrOSComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  *hash = sha2_hash_;
}

std::string CrOSComponentInstallerPolicy::GetName() const {
  return name_;
}

update_client::InstallerAttributes
CrOSComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attrs;
  attrs["_env_version"] = env_version_;
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

CrOSComponentInstaller::CrOSComponentInstaller(
    std::unique_ptr<MetadataTable> metadata_table,
    ComponentUpdateService* component_updater)
    : metadata_table_(std::move(metadata_table)),
      component_updater_(component_updater) {}

CrOSComponentInstaller::~CrOSComponentInstaller() {}

void CrOSComponentInstaller::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void CrOSComponentInstaller::Load(const std::string& name,
                                  MountPolicy mount_policy,
                                  UpdatePolicy update_policy,
                                  LoadCallback load_callback) {
  if (!IsCompatible(name) || update_policy == UpdatePolicy::kForce) {
    // A compatible component is not installed, or forced update is requested.
    // Start registration and installation/update process.
    Install(name, update_policy, mount_policy, std::move(load_callback));
  } else if (mount_policy == MountPolicy::kMount) {
    // A compatible component is installed, load it.
    LoadInternal(name, std::move(load_callback));
  } else {
    // A compatible component is installed, do not load it.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(load_callback),
                                  ReportError(Error::NONE), base::FilePath()));
  }
}

bool CrOSComponentInstaller::Unload(const std::string& name) {
  const ComponentConfig* config = FindConfig(name);
  if (!config) {
    // Component |name| does not exist.
    return false;
  }
  const std::string id = GenerateId(config->sha2hash);
  metadata_table_->DeleteComponentForCurrentUser(name);
  return metadata_table_->HasComponentForAnyUser(name) ||
         component_updater_->UnregisterComponent(id);
}

void CrOSComponentInstaller::RegisterInstalled() {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(GetInstalled),
      base::BindOnce(&CrOSComponentInstaller::RegisterN,
                     base::Unretained(this)));
}

void CrOSComponentInstaller::RegisterCompatiblePath(
    const std::string& name,
    const base::FilePath& path) {
  compatible_components_[name] = path;
}

void CrOSComponentInstaller::UnregisterCompatiblePath(const std::string& name) {
  compatible_components_.erase(name);
}

base::FilePath CrOSComponentInstaller::GetCompatiblePath(
    const std::string& name) const {
  const auto it = compatible_components_.find(name);
  return it == compatible_components_.end() ? base::FilePath() : it->second;
}

void CrOSComponentInstaller::EmitInstalledSignal(const std::string& component) {
  if (delegate_)
    delegate_->EmitInstalledSignal(component);
}

bool CrOSComponentInstaller::IsRegistered(const std::string& name) const {
  base::FilePath root;
  if (!base::PathService::Get(DIR_COMPONENT_USER, &root))
    return false;

  return base::PathExists(root.Append(kComponentsRootPath).Append(name));
}

void CrOSComponentInstaller::Register(const ComponentConfig& config,
                                      base::OnceClosure register_callback) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<CrOSComponentInstallerPolicy>(config, this));
  installer->Register(component_updater_, std::move(register_callback));
}

void CrOSComponentInstaller::Install(const std::string& name,
                                     UpdatePolicy update_policy,
                                     MountPolicy mount_policy,
                                     LoadCallback load_callback) {
  const ComponentConfig* config = FindConfig(name);
  if (!config) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(load_callback),
                                  ReportError(Error::UNKNOWN_COMPONENT),
                                  base::FilePath()));
    return;
  }

  Register(*config,
           base::BindOnce(
               &CrOSComponentInstaller::StartInstall, base::Unretained(this),
               name, GenerateId(config->sha2hash), update_policy,
               base::BindOnce(&CrOSComponentInstaller::FinishInstall,
                              base::Unretained(this), name, mount_policy,
                              update_policy, std::move(load_callback))));
}

void CrOSComponentInstaller::StartInstall(
    const std::string& name,
    const std::string& id,
    UpdatePolicy update_policy,
    update_client::Callback install_callback) {
  // Check whether an installed component was found during registration, and
  // determine whether OnDemandUpdater should be started accordingly.
  const bool is_compatible = IsCompatible(name);
  if (update_policy == UpdatePolicy::kSkip ||
      (is_compatible && update_policy != UpdatePolicy::kForce)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(install_callback),
                                  update_client::Error::NONE));
    return;
  }

  const component_updater::OnDemandUpdater::Priority priority =
      is_compatible ? component_updater::OnDemandUpdater::Priority::BACKGROUND
                    : component_updater::OnDemandUpdater::Priority::FOREGROUND;
  component_updater_->GetOnDemandUpdater().OnDemandUpdate(
      id, priority, std::move(install_callback));
}

void CrOSComponentInstaller::FinishInstall(const std::string& name,
                                           MountPolicy mount_policy,
                                           UpdatePolicy update_policy,
                                           LoadCallback load_callback,
                                           update_client::Error error) {
  if (error != update_client::Error::NONE) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(load_callback),
                       ReportError(Error::INSTALL_FAILURE), base::FilePath()));
  } else if (!IsCompatible(name)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(load_callback),
                       ReportError(update_policy == UpdatePolicy::kSkip
                                       ? Error::NOT_FOUND
                                       : Error::COMPATIBILITY_CHECK_FAILED),
                       base::FilePath()));
  } else if (mount_policy == MountPolicy::kMount) {
    LoadInternal(name, std::move(load_callback));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(load_callback),
                                  ReportError(Error::NONE), base::FilePath()));
  }
}

void CrOSComponentInstaller::LoadInternal(const std::string& name,
                                          LoadCallback load_callback) {
  const base::FilePath path = GetCompatiblePath(name);
  DCHECK(!path.empty());
  chromeos::DBusThreadManager::Get()
      ->GetImageLoaderClient()
      ->LoadComponentAtPath(
          name, path,
          base::BindOnce(&CrOSComponentInstaller::FinishLoad,
                         base::Unretained(this), std::move(load_callback),
                         base::TimeTicks::Now(), name));
}

void CrOSComponentInstaller::FinishLoad(LoadCallback load_callback,
                                        const base::TimeTicks start_time,
                                        const std::string& name,
                                        base::Optional<base::FilePath> result) {
  // Report component image mount time.
  UMA_HISTOGRAM_LONG_TIMES("ComponentUpdater.ChromeOS.MountTime",
                           base::TimeTicks::Now() - start_time);
  if (!result.has_value()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(load_callback),
                       ReportError(Error::MOUNT_FAILURE), base::FilePath()));
  } else {
    metadata_table_->AddComponentForCurrentUser(name);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(load_callback),
                                  ReportError(Error::NONE), result.value()));
  }
}

void CrOSComponentInstaller::RegisterN(
    const std::vector<ComponentConfig>& configs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (const auto& config : configs) {
    Register(config, base::OnceClosure());
  }
}

bool CrOSComponentInstaller::IsCompatible(const std::string& name) const {
  return compatible_components_.count(name) > 0;
}

}  // namespace component_updater
