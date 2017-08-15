// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/update_client/update_client.h"
#include "crypto/sha2.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_method_call_status.h"
#endif  // defined(OS_CHROMEOS)

//  Developer API usage:
//  ...
//  void LoadCallback(const std::string& mount_point){
//    if (mount_point.empty()) {
//      // component is not loaded.
//      return;
//    }
//    ...
//  }
//  ...
//  component_updater::CrOSComponent::LoadComponent(
//            name,
//            base::Bind(&LoadCallback));
//
namespace component_updater {

#if defined(OS_CHROMEOS)
struct ComponentConfig {
  std::string name;
  std::string env_version;
  std::string sha2hashstr;
  ComponentConfig(const std::string& name,
                  const std::string& env_version,
                  const std::string& sha2hashstr);
  ~ComponentConfig();
};

class CrOSComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  explicit CrOSComponentInstallerTraits(const ComponentConfig& config);
  ~CrOSComponentInstallerTraits() override {}

 private:
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, IsCompatibleOrNot);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest,
                           ComponentReadyCorrectManifest);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest,
                           ComponentReadyWrongManifest);
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

  virtual bool IsCompatible(const std::string& env_version_str,
                            const std::string& min_env_version_str);
  std::string name;
  std::string env_version;
  uint8_t kSha2Hash_[crypto::kSHA256Length] = {};

  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstallerTraits);
};

// This class contains functions used to register and install a component.
class CrOSComponent {
 public:
  static void LoadComponent(
      const std::string& name,
      const base::Callback<void(const std::string&)>& load_callback);

  // Returns all installed components.
  static std::vector<ComponentConfig> GetInstalledComponents();

  // Registers component |configs| to be updated.
  static void RegisterComponents(const std::vector<ComponentConfig>& configs);

 private:
  CrOSComponent() {}
  static void RegisterResult(ComponentUpdateService* cus,
                             const std::string& id,
                             const update_client::Callback& install_callback);
  static void InstallComponent(
      ComponentUpdateService* cus,
      const std::string& name,
      const base::Callback<void(const std::string&)>& load_callback);
};
#endif  // defined(OS_CHROMEOS)

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_
