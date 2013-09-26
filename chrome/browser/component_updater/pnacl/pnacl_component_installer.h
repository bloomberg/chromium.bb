// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_COMPONENT_INSTALLER_H_

#include <list>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/version.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/component_updater/pnacl/pnacl_profile_observer.h"
#include "chrome/browser/component_updater/pnacl/pnacl_updater_observer.h"

class CommandLine;

namespace base {
class DictionaryValue;
}

// Component installer responsible for Portable Native Client files.
// Files can be installed to a shared location, or be installed to
// a per-user location.
class PnaclComponentInstaller : public ComponentInstaller {
 public:
  PnaclComponentInstaller();

  virtual ~PnaclComponentInstaller();

  virtual void OnUpdateError(int error) OVERRIDE;

  virtual bool Install(const base::DictionaryValue& manifest,
                       const base::FilePath& unpack_path) OVERRIDE;

  virtual bool GetInstalledFile(const std::string& file,
                                base::FilePath* installed_file) OVERRIDE;

  // Register a PNaCl component for the first time.
  void RegisterPnaclComponent(ComponentUpdateService* cus,
                              const CommandLine& command_line);

  // Check the PNaCl version again and re-register with the component
  // updater service.
  void ReRegisterPnacl();

  CrxComponent GetCrxComponent();

  // Return true if PNaCl installs are separated by user.
  bool per_user() const { return per_user_; }

  // If per_user, function to call when profile is changed.
  void OnProfileChange();

  // Return true if PNaCl updates are disabled.
  bool updates_disabled() const { return updates_disabled_; }

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

  typedef base::Callback<void(bool)> InstallCallback;

  // Ask the component updater service to do a first-install for PNaCl.
  // The |installed| callback will be run with |true| on success,
  // or run with |false| on an error. The callback is called on the UI thread.
  void RequestFirstInstall(const InstallCallback& installed);

 private:
  friend class PnaclUpdaterObserver;

  // Called when a RequestFirstInstall completed successfully.
  void NotifyInstallSuccess();

  // Called when a RequestFirstInstall will not happen, or an error occurred.
  void NotifyInstallError();

  bool per_user_;
  bool updates_disabled_;
  scoped_ptr<PnaclProfileObserver> profile_observer_;
  base::FilePath current_profile_path_;
  base::Version current_version_;
  std::string current_fingerprint_;
  ComponentUpdateService* cus_;
  // The one callback to call when there is a RequestFirstInstall.
  InstallCallback install_callback_;
  // Component updater service observer, to determine when an on-demand
  // install request failed.
  scoped_ptr<PnaclUpdaterObserver> updater_observer_;
  DISALLOW_COPY_AND_ASSIGN(PnaclComponentInstaller);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_PNACL_PNACL_COMPONENT_INSTALLER_H_
