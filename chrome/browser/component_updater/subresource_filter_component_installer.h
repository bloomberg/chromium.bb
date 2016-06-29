// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SUBRESOURCE_FILTER_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SUBRESOURCE_FILTER_COMPONENT_INSTALLER_H_

#include <string>
#include <vector>

#include "components/component_updater/default_component_installer.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// Component for receiving Safe Browsing Subresource filtering rules.
class SubresourceFilterComponentInstallerTraits
    : public ComponentInstallerTraits {
 public:
  SubresourceFilterComponentInstallerTraits();

  ~SubresourceFilterComponentInstallerTraits() override;

 private:
  friend class SubresourceFilterComponentInstallerTest;

  // ComponentInstallerTraits implementation.
  bool CanAutoUpdate() const override;
  bool RequiresNetworkEncryption() const override;
  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  // Reads and parses the on-disk ruleset file.
  void LoadSubresourceFilterRulesFromDisk(const base::FilePath& file_path,
                                          const base::Version& version);

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterComponentInstallerTraits);
};

void RegisterSubresourceFilterComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SUBRESOURCE_FILTER_COMPONENT_INSTALLER_H_
