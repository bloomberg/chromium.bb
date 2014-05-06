// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_DEFAULT_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_DEFAULT_COMPONENT_INSTALLER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/version.h"
#include "chrome/browser/component_updater/component_updater_service.h"

namespace base {
class DictionaryValue;
class FilePath;
}  // namespace base

namespace component_updater {

// Components should use a DefaultComponentInstaller by defining a class that
// implements the members of ComponentInstallerTraits, and then registering a
// DefaultComponentInstaller that has been constructed with an instance of that
// class.
class ComponentInstallerTraits {
 public:
  virtual ~ComponentInstallerTraits();

  // Verifies that a working installation resides within the directory specified
  // by |dir|. |dir| is of the form <base directory>/<version>.
  // Called only from a thread belonging to a blocking thread pool.
  // The implementation of this function must be efficient since the function
  // can be called when Chrome starts.
  virtual bool VerifyInstallation(const base::FilePath& dir) const = 0;

  // Returns true if the component can be automatically updated. Called once
  // during component registration from the UI thread.
  virtual bool CanAutoUpdate() const = 0;

  // OnCustomInstall is called during the installation process. Components that
  // require custom installation operations should implement them here.
  // Returns false if a custom operation failed, and true otherwise.
  // Called only from a thread belonging to a blocking thread pool.
  virtual bool OnCustomInstall(const base::DictionaryValue& manifest,
                               const base::FilePath& install_dir) = 0;

  // ComponentReady is called in two cases:
  //   1) After an installation is successfully completed.
  //   2) During component registration if the component is already installed.
  // In both cases the install is verified before this is called. This method
  // is guaranteed to be called before any observers of the component are
  // notified of a successful install, and is meant to support follow-on work
  // such as updating paths elsewhere in Chrome. Called on the UI thread.
  // |version| is the version of the component.
  // |install_dir| is the path to the install directory for this version.
  // |manifest| is the manifest for this version of the component.
  virtual void ComponentReady(const base::Version& version,
                              const base::FilePath& install_dir,
                              scoped_ptr<base::DictionaryValue> manifest) = 0;

  // Returns the directory that the installer will place versioned installs of
  // the component into.
  virtual base::FilePath GetBaseDirectory() const = 0;

  // Returns the component's SHA2 hash as raw bytes.
  virtual void GetHash(std::vector<uint8>* hash) const = 0;

  // Returns the human-readable name of the component.
  virtual std::string GetName() const = 0;
};

// A DefaultComponentInstaller is intended to be final, and not derived from.
// Customization must be provided by passing a ComponentInstallerTraits object
// to the constructor.
class DefaultComponentInstaller : public ComponentInstaller {
 public:
  explicit DefaultComponentInstaller(
      scoped_ptr<ComponentInstallerTraits> installer_traits);

  // Registers the component for update checks and installs.
  void Register(ComponentUpdateService* cus);

  // Overridden from ComponentInstaller:
  virtual void OnUpdateError(int error) OVERRIDE;
  virtual bool Install(const base::DictionaryValue& manifest,
                       const base::FilePath& unpack_path) OVERRIDE;
  virtual bool GetInstalledFile(const std::string& file,
                                base::FilePath* installed_file) OVERRIDE;

  virtual ~DefaultComponentInstaller();

 private:
  base::FilePath GetInstallDirectory();
  bool InstallHelper(const base::DictionaryValue& manifest,
                     const base::FilePath& unpack_path,
                     const base::FilePath& install_path);
  void StartRegistration(ComponentUpdateService* cus);
  void FinishRegistration(ComponentUpdateService* cus);

  base::Version current_version_;
  std::string current_fingerprint_;
  scoped_ptr<base::DictionaryValue> current_manifest_;
  scoped_ptr<ComponentInstallerTraits> installer_traits_;

  DISALLOW_COPY_AND_ASSIGN(DefaultComponentInstaller);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_DEFAULT_COMPONENT_INSTALLER_H_
