// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/conflicts/module_blacklist_cache_updater_win.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"

class IncompatibleApplicationsUpdater;
class InstalledApplications;
class ModuleDatabase;
class ModuleListFilter;
class PrefRegistrySimple;
struct CertificateInfo;

namespace base {
class FilePath;
class SequencedTaskRunner;
class TaskRunner;
}

// This class owns all the third-party conflicts-related classes and is
// responsible for their initialization.
class ThirdPartyConflictsManager : public ModuleDatabaseObserver {
 public:
  explicit ThirdPartyConflictsManager(ModuleDatabase* module_database);
  ~ThirdPartyConflictsManager() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Explicitely disables the third-party module blocking feature. This is
  // needed because simply turning off the feature using either the Feature List
  // API or via group policy is not sufficient. Disabling the blocking requires
  // the deletion of the module blacklist cache. This task is executed on
  // |background_sequence|.
  static void DisableThirdPartyModuleBlocking(
      base::TaskRunner* background_sequence);

  // Explicitely disables the blocking of third-party modules for the next
  // browser launch and prevent |instance| from reenabling it. Basically calls
  // the above function in the background sequence of |instance| and then
  // deletes that instance.
  static void ShutdownAndDestroy(
      std::unique_ptr<ThirdPartyConflictsManager> instance);

  // ModuleDatabaseObserver:
  void OnModuleDatabaseIdle() override;

  // Loads the |module_list_filter_| using the Module List at |path|.
  void LoadModuleList(const base::FilePath& path);

 private:
  // Called when |exe_certificate_info_| finishes its initialization.
  void OnExeCertificateCreated(
      std::unique_ptr<CertificateInfo> exe_certificate_info);

  // Called when |module_list_filter_| finishes its initialization.
  void OnModuleListFilterCreated(
      std::unique_ptr<ModuleListFilter> module_list_filter);

  // Called when |installed_applications_| finishes its initialization.
  void OnInstalledApplicationsCreated(
      std::unique_ptr<InstalledApplications> installed_applications);

  // Initializes |incompatible_applications_updater_| when the
  // exe_certificate_info_, the module_list_filter_ and the
  // installed_applications_ are available.
  void InitializeIncompatibleApplicationsUpdater();

  // Initializes |module_blacklist_cache_updater_| if third-party module
  // blocking is enabled.
  void MaybeInitializeModuleBlacklistCacheUpdater();

  // Checks if the |old_md5_digest| matches the expected one from the Local
  // State file, and updates it to |new_md5_digest|.
  void OnModuleBlacklistCacheUpdated(
      const ModuleBlacklistCacheUpdater::CacheUpdateResult& result);

  ModuleDatabase* module_database_;

  scoped_refptr<base::SequencedTaskRunner> background_sequence_;

  // Indicates if the initial Module List has been received. Used to prevent the
  // creation of multiple ModuleListFilter instances.
  bool module_list_received_;

  // Indicates if the OnModuleDatabaseIdle() function has been called once
  // already. Used to prevent the creation of multiple InstalledApplications
  // instances.
  bool on_module_database_idle_called_;

  // The certificate info of the current executable.
  std::unique_ptr<CertificateInfo> exe_certificate_info_;

  // Filters third-party modules against a whitelist and a blacklist.
  std::unique_ptr<ModuleListFilter> module_list_filter_;

  // Retrieves the list of installed applications.
  std::unique_ptr<InstalledApplications> installed_applications_;

  // Maintains the cache of incompatible applications.
  std::unique_ptr<IncompatibleApplicationsUpdater>
      incompatible_applications_updater_;

  // Maintains the module blacklist cache. This member is only initialized when
  // the ThirdPartyModuleBlocking feature is enabled.
  std::unique_ptr<ModuleBlacklistCacheUpdater> module_blacklist_cache_updater_;

  base::WeakPtrFactory<ThirdPartyConflictsManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyConflictsManager);
};

#endif  // CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_
