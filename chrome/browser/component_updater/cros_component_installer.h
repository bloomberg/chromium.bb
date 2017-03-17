// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_

#include <string>

#include "build/build_config.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/update_client/update_client.h"
#include "crypto/sha2.h"

namespace component_updater {

#if defined(OS_CHROMEOS)
struct ComponentConfig {
  std::string name;
  std::string dir;
  std::string sha2hashstr;
  ComponentConfig(const std::string& name,
                  const std::string& dir,
                  const std::string& sha2hashstr);
  ~ComponentConfig();
};

using ConfigMap = std::map<std::string, std::map<std::string, std::string>>;

class CrOSComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  explicit CrOSComponentInstallerTraits(const ComponentConfig& config);
  ~CrOSComponentInstallerTraits() override {}

 private:
  // The following methods override ComponentInstallerTraits.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;
  std::string dir_name;
  std::string name;
  uint8_t kSha2Hash_[crypto::kSHA256Length] = {};

  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstallerTraits);
};

// This class contains functions used to register and install a component.
class CrOSComponent {
 public:
  // Register and start installing a CrOS component.
  // |install_callback| is triggered after install finishes and returns error
  // code: update_client::Error::INVALID_ARGUMENT - component name is invalid.
  // update_client::Error::NONE
  // - successful install
  // other error returns are processed by OnDemandUpdate upon install finishes.
  static bool InstallCrOSComponent(
      const std::string& name,
      const update_client::Callback& install_callback);

 private:
  CrOSComponent() {}
  // Register a component.
  static void RegisterCrOSComponentInternal(ComponentUpdateService* cus,
                                            const ComponentConfig& config,
                                            const base::Closure& callback);
  // A helper function to pass into RegisterCrOSComonentInternal as a callback.
  // It calls OnDemandUpdate to install the component right after being
  // registered.
  static void InstallChromeOSComponent(
      ComponentUpdateService* cus,
      const std::string& id,
      const update_client::Callback& install_callback);
};
#endif  // defined(OS_CHROMEOS)

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_
