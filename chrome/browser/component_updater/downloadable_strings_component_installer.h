// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_DOWNLOADABLE_STRINGS_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_DOWNLOADABLE_STRINGS_COMPONENT_INSTALLER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"

namespace update_client {
enum class Error;
}  // namespace update_client

namespace component_updater {

class ComponentUpdateService;

// Component installer policy for the DonwloadableStrings component.
// For more details, see http://crbug.com/757543
class DownloadableStringsComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  DownloadableStringsComponentInstallerPolicy() {}
  ~DownloadableStringsComponentInstallerPolicy() override {}

 private:
  // The following methods override ComponentInstallerTraits.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  static void TriggerComponentUpdate(OnDemandUpdater* updater);

  static void DoRegistration(ComponentUpdateService*);

  friend void RegisterDownloadableStringsComponent(ComponentUpdateService*);

  DISALLOW_COPY_AND_ASSIGN(DownloadableStringsComponentInstallerPolicy);
};

// Call once during startup to make the component update service aware of
// the Downloadable Strings component.
void RegisterDownloadableStringsComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_DOWNLOADABLE_STRINGS_COMPONENT_INSTALLER_H_
