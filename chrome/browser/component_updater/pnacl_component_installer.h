// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PNACL_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PNACL_COMPONENT_INSTALLER_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/version.h"
#include "components/update_client/update_client.h"

namespace base {
class DictionaryValue;
}

namespace pnacl {
// Returns true if PNaCl actually needs an on-demand component update.
// E.g., if PNaCl is not yet installed and the user is loading a PNaCl app,
// or the current version is behind chrome's version, and is ABI incompatible
// with chrome. If not necessary, returns false.
// May conservatively return false before PNaCl is registered, but
// should return the right answer after it is registered.
bool NeedsOnDemandUpdate();
}

namespace component_updater {

class ComponentUpdateService;

// Component installer responsible for Portable Native Client files.
// Files can be installed to a shared location, or be installed to
// a per-user location.
class PnaclComponentInstaller : public update_client::CrxInstaller {
 public:
  PnaclComponentInstaller();

  // ComponentInstaller implementation:
  void OnUpdateError(int error) override;
  update_client::CrxInstaller::Result Install(
      const base::DictionaryValue& manifest,
      const base::FilePath& unpack_path) override;
  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;
  bool Uninstall() override;

  // Register a PNaCl component for the first time.
  void RegisterPnaclComponent(ComponentUpdateService* cus);

  update_client::CrxComponent GetCrxComponent();

  // Determine the base directory for storing each version of PNaCl.
  base::FilePath GetPnaclBaseDirectory();

  base::Version current_version() const { return current_version_; }

  void set_current_version(const base::Version& current_version) {
    current_version_ = current_version;
  }

  std::string current_fingerprint() const { return current_fingerprint_; }

  void set_current_fingerprint(const std::string& current_fingerprint) {
    current_fingerprint_ = current_fingerprint;
  }

  ComponentUpdateService* cus() const { return cus_; }

 private:
  ~PnaclComponentInstaller() override;

  bool DoInstall(const base::DictionaryValue& manifest,
                 const base::FilePath& unpack_path);

  base::Version current_version_;
  std::string current_fingerprint_;
  ComponentUpdateService* cus_;

  DISALLOW_COPY_AND_ASSIGN(PnaclComponentInstaller);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PNACL_COMPONENT_INSTALLER_H_
