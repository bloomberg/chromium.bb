// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_

#include <string>

#include "build/build_config.h"
#include "components/component_updater/default_component_installer.h"
#include "crypto/sha2.h"

namespace component_updater {

class ComponentUpdateService;

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
#endif  // defined(OS_CHROMEOS)

// Register a CrOS component.
// It must be called on UI thread.
bool RegisterCrOSComponent(ComponentUpdateService* cus,
                           const std::string& name);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_
