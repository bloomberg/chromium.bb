// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_EV_WHITELIST_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_EV_WHITELIST_COMPONENT_INSTALLER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/values.h"
#include "components/component_updater/default_component_installer.h"

namespace component_updater {

class ComponentUpdateService;

class EVWhitelistComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  EVWhitelistComponentInstallerTraits();
  virtual ~EVWhitelistComponentInstallerTraits() {}

 private:
  // The following methods override ComponentInstallerTraits.
  virtual bool CanAutoUpdate() const OVERRIDE;
  virtual bool OnCustomInstall(const base::DictionaryValue& manifest,
                               const base::FilePath& install_dir) OVERRIDE;
  virtual bool VerifyInstallation(
      const base::FilePath& install_dir) const OVERRIDE;
  virtual void ComponentReady(
      const base::Version& version,
      const base::FilePath& path,
      scoped_ptr<base::DictionaryValue> manifest) OVERRIDE;
  virtual base::FilePath GetBaseDirectory() const OVERRIDE;
  virtual void GetHash(std::vector<uint8_t>* hash) const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  static base::FilePath GetInstalledPath(const base::FilePath& base);

  DISALLOW_COPY_AND_ASSIGN(EVWhitelistComponentInstallerTraits);
};

// Call once during startup to make the component update service aware of
// the EV whitelist.
void RegisterEVWhitelistComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_EV_WHITELIST_COMPONENT_INSTALLER_H_
