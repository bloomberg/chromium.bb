// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_installer_errors.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

namespace component_updater {

#if defined(OS_CHROMEOS)
void LogRegistrationResult(chromeos::DBusMethodCallStatus call_status,
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
                              base::Bind(&LogRegistrationResult));
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

void RegisterCrOSComponentInternal(ComponentUpdateService* cus,
                                   const ComponentConfig& config) {
  std::unique_ptr<ComponentInstallerTraits> traits(
      new CrOSComponentInstallerTraits(config));
  // |cus| will take ownership of |installer| during
  // installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

bool RegisterCrOSComponentInternal(ComponentUpdateService* cus,
                                   const std::string& name) {
  if (name.empty()) {
    DVLOG(1) << "[RegisterCrOSComponents] name is empty.";
    return false;
  }
  const std::map<std::string, std::map<std::string, std::string>> components = {
      {"escpr",
       {{"dir", "epson-inkjet-printer-escpr"},
        {"sha2hashstr",
         "1913a5e0a6cad30b6f03e176177e0d7ed62c5d6700a9c66da556d7c3f5d6a47e"}}}};
  const auto it = components.find(name);
  if (it == components.end()) {
    DVLOG(1) << "[RegisterCrOSComponents] component " << name
             << " is not in configuration.";
    return false;
  }
  ComponentConfig config(it->first, it->second.find("dir")->second,
                         it->second.find("sha2hashstr")->second);
  RegisterCrOSComponentInternal(cus, config);
  return true;
}

#endif  // defined(OS_CHROMEOS)

bool RegisterCrOSComponent(ComponentUpdateService* cus,
                           const std::string& name) {
#if defined(OS_CHROMEOS)
  return RegisterCrOSComponentInternal(cus, name);
#else
  return false;
#endif  // defined(OS_CHROMEOS)
}

}  // namespace component_updater
