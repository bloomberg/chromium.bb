// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/browser_process.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "crypto/sha2.h"

namespace component_updater {

struct ComponentConfig {
  std::string name;
  std::string env_version;
  std::string sha2hashstr;
  ComponentConfig(const std::string& name,
                  const std::string& env_version,
                  const std::string& sha2hashstr);
  ~ComponentConfig();
};

class CrOSComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit CrOSComponentInstallerPolicy(const ComponentConfig& config);
  ~CrOSComponentInstallerPolicy() override {}

 private:
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, IsCompatibleOrNot);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, CompatibilityOK);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest,
                           CompatibilityMissingManifest);
  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
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

  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstallerPolicy);
};

// This class contains functions used to register and install a component.
class CrOSComponentManager {
 public:
  using LoadCallback = base::OnceCallback<void(const base::FilePath&)>;
  enum class MountPolicy {
    kMount,
    kDontMount,
  };

  CrOSComponentManager();
  ~CrOSComponentManager();
  // Installs a component and keeps it up-to-date. |load_callback| returns the
  // mount point path.
  void Load(const std::string& name,
            MountPolicy mount_policy,
            LoadCallback load_callback);

  // Stops updating and removes a component.
  // Returns true if the component was successfully unloaded
  // or false if it couldn't be unloaded or already wasn't loaded.
  bool Unload(const std::string& name);

  // Register all installed components.
  void RegisterInstalled();

  // Saves the name and install path of a compatible component.
  void RegisterCompatiblePath(const std::string& name,
                              const base::FilePath& path);

  // Removes the name and install path entry of a component.
  void UnregisterCompatiblePath(const std::string& name);

  // Checks if the current installed component is compatible given a component
  // |name|. If compatible, sets |path| to be its installed path.
  bool IsCompatible(const std::string& name) const;

  // Returns installed path of a compatible component given |name|. Returns an
  // empty path if the component isn't compatible.
  base::FilePath GetCompatiblePath(const std::string& name) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerTest, RegisterComponent);

  // Registers a component with a dedicated ComponentUpdateService instance.
  void Register(ComponentUpdateService* cus,
                const ComponentConfig& config,
                base::OnceClosure register_callback);

  // Installs a component with a dedicated ComponentUpdateService instance.
  void Install(ComponentUpdateService* cus,
               const std::string& name,
               MountPolicy mount_policy,
               LoadCallback load_callback);

  // Calls OnDemandUpdate to install the component right after being registered.
  // |id| is the component id generated from its sha2 hash.
  void StartInstall(ComponentUpdateService* cus,
                    const std::string& id,
                    update_client::Callback install_callback);

  // Calls LoadInternal to load the installed component.
  void FinishInstall(const std::string& name,
                     MountPolicy mount_policy,
                     LoadCallback load_callback,
                     update_client::Error error);

  // Internal function to load a component.
  void LoadInternal(const std::string& name, LoadCallback load_callback);

  // Calls load_callback and pass in the parameter |result| (component mount
  // point).
  void FinishLoad(LoadCallback load_callback,
                  base::Optional<base::FilePath> result);

  // Returns all installed components.
  std::vector<ComponentConfig> GetInstalled();

  // Registers component |configs| to be updated.
  void RegisterN(const std::vector<ComponentConfig>& configs);

  // Maps from a compatible component name to its installed path.
  base::flat_map<std::string, base::FilePath> compatible_components_;

  DISALLOW_COPY_AND_ASSIGN(CrOSComponentManager);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CROS_COMPONENT_INSTALLER_H_
