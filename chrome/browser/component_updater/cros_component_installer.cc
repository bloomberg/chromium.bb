// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_installer_errors.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

namespace component_updater {

#if defined(OS_CHROMEOS)
void LogRegistrationResult(const std::string& name,
                           chromeos::DBusMethodCallStatus call_status,
                           bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
    DVLOG(1) << "Call to imageloader service failed.";
    return;
  }
  if (!result) {
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
                              base::Bind(&LogRegistrationResult, name));
  } else {
    DVLOG(1) << "Failed to get ImageLoaderClient object.";
  }
}

ComponentConfig::ComponentConfig(const std::string& name,
                                 const std::string& dir,
                                 const std::string& sha2hashstr)
    : name(name), dir(dir), sha2hashstr(sha2hashstr) {}
ComponentConfig::~ComponentConfig() {}

CrOSComponentInstallerTraits::CrOSComponentInstallerTraits(
    const ComponentConfig& config)
    : dir_name(config.dir), name(config.name) {
  if (config.sha2hashstr.length() != 64)
    return;
  auto strstream = config.sha2hashstr;
  for (auto& cell : kSha2Hash_) {
    cell = stoul(strstream.substr(0, 2), nullptr, 16);
    strstream.erase(0, 2);
  }
}

bool CrOSComponentInstallerTraits::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool CrOSComponentInstallerTraits::RequiresNetworkEncryption() const {
  return true;
}

update_client::CrxInstaller::Result
CrOSComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  DVLOG(1) << "[CrOSComponentInstallerTraits::OnCustomInstall]";
  std::string version;
  if (!manifest.GetString("version", &version)) {
    return ToInstallerResult(update_client::InstallError::GENERIC_ERROR);
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ImageLoaderRegistration, version, install_dir, name));
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void CrOSComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    std::unique_ptr<base::DictionaryValue> manifest) {}

bool CrOSComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath CrOSComponentInstallerTraits::GetRelativeInstallDir() const {
  return base::FilePath(dir_name);
}

void CrOSComponentInstallerTraits::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kSha2Hash_, kSha2Hash_ + arraysize(kSha2Hash_));
}

std::string CrOSComponentInstallerTraits::GetName() const {
  return name;
}

update_client::InstallerAttributes
CrOSComponentInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> CrOSComponentInstallerTraits::GetMimeTypes() const {
  std::vector<std::string> mime_types;
  return mime_types;
}

void CrOSComponent::RegisterCrOSComponentInternal(
    ComponentUpdateService* cus,
    const ComponentConfig& config,
    const base::Closure& installcallback) {
  std::unique_ptr<ComponentInstallerTraits> traits(
      new CrOSComponentInstallerTraits(config));
  // |cus| will take ownership of |installer| during
  // installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, installcallback);
}

void CrOSComponent::InstallChromeOSComponent(
    ComponentUpdateService* cus,
    const std::string& id,
    const update_client::Callback& install_callback) {
  cus->GetOnDemandUpdater().OnDemandUpdate(id, install_callback);
}

bool CrOSComponent::InstallCrOSComponent(
    const std::string& name,
    const update_client::Callback& install_callback) {
  auto* const cus = g_browser_process->component_updater();
  const ConfigMap components = {
      {"escpr",
       {{"dir", "epson-inkjet-printer-escpr"},
        {"sha2hashstr",
         "1913a5e0a6cad30b6f03e176177e0d7ed62c5d6700a9c66da556d7c3f5d6a47e"}}}};
  if (name.empty()) {
    base::PostTask(
        FROM_HERE,
        base::Bind(install_callback, update_client::Error::INVALID_ARGUMENT));
    return false;
  }
  const auto it = components.find(name);
  if (it == components.end()) {
    DVLOG(1) << "[RegisterCrOSComponents] component " << name
             << " is not in configuration.";
    base::PostTask(
        FROM_HERE,
        base::Bind(install_callback, update_client::Error::INVALID_ARGUMENT));
    return false;
  }
  ComponentConfig config(it->first, it->second.find("dir")->second,
                         it->second.find("sha2hashstr")->second);
  RegisterCrOSComponentInternal(
      cus, config,
      base::Bind(InstallChromeOSComponent, cus,
                 crx_file::id_util::GenerateIdFromHex(
                     it->second.find("sha2hashstr")->second)
                     .substr(0, 32),
                 install_callback));
  return true;
}

void MountResult(const base::Callback<void(const std::string&)>& mount_callback,
                 chromeos::DBusMethodCallStatus call_status,
                 const std::string& result) {
  if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS) {
    DVLOG(1) << "Call to imageloader service failed.";
    base::PostTask(FROM_HERE, base::Bind(mount_callback, ""));
    return;
  }
  if (result.empty()) {
    DVLOG(1) << "Component load failed";
    base::PostTask(FROM_HERE, base::Bind(mount_callback, ""));
    return;
  }
  base::PostTask(FROM_HERE, base::Bind(mount_callback, result));
}

void CrOSComponent::LoadCrOSComponent(
    const std::string& name,
    const base::Callback<void(const std::string&)>& mount_callback) {
  chromeos::ImageLoaderClient* loader =
      chromeos::DBusThreadManager::Get()->GetImageLoaderClient();
  if (loader) {
    loader->LoadComponent(name, base::Bind(&MountResult, mount_callback));
  } else {
    DVLOG(1) << "Failed to get ImageLoaderClient object.";
  }
}
#endif  // defined(OS_CHROMEOS

}  // namespace component_updater
