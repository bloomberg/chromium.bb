// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_MODULE_LIST_MANAGER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_MODULE_LIST_MANAGER_WIN_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/version.h"

namespace component_updater {
class ThirdPartyModuleListComponentInstallerPolicy;
}  // namespace component_updater

// Declares a class that is responsible for knowing the location of the most
// up-to-date module list (a whitelist of third party modules that are safe for
// injection).
//
// Since the module list can change at runtime (a new component version can be
// installed) this class provides an observer interface that can notify clients
// of a new whitelist.
//
// This class binds to the sequence on which it is created, and expects
// callbacks to arrive on that same sequence.
class ModuleListManager {
 public:
  // The root of the module list registry information. This is relative to the
  // root of the installation registry data, as returned by
  // install_static::InstallDetails::GetClientStateKeyPath.
  static const wchar_t kModuleListRegistryKeyPath[];

  // The name of the key below registry_key_root_ that stores the path to the
  // most recent module list, as a string.
  static const wchar_t kModuleListPathKeyName[];

  // The name of the key below registry_key_root_ that stores the version of the
  // most recent module list, as a string.
  static const wchar_t kModuleListVersionKeyName[];

  // An observer of changes to the ModuleListManager. The observer will only
  // be called if the module list installation location changes after the
  // observer has been registered.
  class Observer {
   public:
    Observer();
    virtual ~Observer();

    // This is invoked on the same sequence to which the ModuleListManager is
    // bound.
    virtual void OnNewModuleList(const base::Version& version,
                                 const base::FilePath& path) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Creates a ModuleListManager bound to the provided |task_runner|. All calls
  // to the manager are expected to be received on that task runner, and all
  // outgoing callbacks will be on the same task runner.
  explicit ModuleListManager(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~ModuleListManager();

  // Returns the path to the current module list. This is empty if no module
  // list is available.
  base::FilePath module_list_path();

  // Returns the version of the current module list. This is empty if no module
  // list is available.
  base::Version module_list_version();

  // For adding and removing an observer. It is expected that there only be a
  // single observer needed. The observer must outlive this class. Call
  // SetObserver(nullptr) to clear the observer.
  void SetObserver(Observer* observer);

  // Generates the registry key where Path and Version information will be
  // written. Exposed for testing.
  static HKEY GetRegistryHive();
  static std::wstring GetRegistryPath();

 protected:
  friend class component_updater::ThirdPartyModuleListComponentInstallerPolicy;

  // Called post-startup with information about the most recently available
  // module list installation. Can potentially be called again much later when
  // another (newer) version is installed.
  void LoadModuleList(const base::Version& version, const base::FilePath& path);

 private:
  friend class ModuleListManagerTest;

  // The hive and path to the registry key where the most recent module list
  // path information is cached.
  const std::wstring registry_key_root_;

  // The task runner on which this class does its work.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The path and version of the most recently installed module list. This is
  // retrieved from the registry at creation of ModuleListManager, and
  // potentially updated at runtime via calls to LoadModuleList.
  base::FilePath module_list_path_;
  base::Version module_list_version_;

  // The observer of this object.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ModuleListManager);
};

#endif  // CHROME_BROWSER_CONFLICTS_MODULE_LIST_MANAGER_WIN_H_
