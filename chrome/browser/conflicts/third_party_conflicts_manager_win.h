// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"

class ModuleDatabase;
class ModuleListFilter;
class InstalledPrograms;
class ProblematicProgramsUpdater;

namespace base {
class FilePath;
}

// This class owns all the third-party conflicts-related classes and is
// responsible for their initialization.
class ThirdPartyConflictsManager : public ModuleDatabaseObserver {
 public:
  explicit ThirdPartyConflictsManager(ModuleDatabase* module_database);
  ~ThirdPartyConflictsManager() override;

  // ModuleDatabaseObserver:
  void OnModuleDatabaseIdle() override;

  // Loads the |module_list_filter_| using the Module List at |path|.
  void LoadModuleList(const base::FilePath& path);

 private:
  // Called when |module_list_filter_| finishes its initialization.
  void OnModuleListFilterCreated(std::unique_ptr<ModuleListFilter>);

  // Called when |installed_programs_| finishes its initialization.
  void OnInstalledProgramsCreated(
      std::unique_ptr<InstalledPrograms> installed_programs);

  // Initializes |problematic_programs_updater_| when both the ModuleListFilter
  // and the InstalledPrograms are available.
  void InitializeProblematicProgramsUpdater();

  ModuleDatabase* module_database_;

  // Indicates if the initial Module List has been received. Used to prevent the
  // creation of multiple ModuleListFilter instances.
  bool module_list_received_;

  // Indicates if the OnModuleDatabaseIdle() function has been called once
  // already. Used to prevent the creation of multiple InstalledPrograms
  // instances.
  bool on_module_database_idle_called_;

  // Filters third-party modules against a whitelist and a blacklist.
  std::unique_ptr<ModuleListFilter> module_list_filter_;

  // Retrieves the list of installed programs.
  std::unique_ptr<InstalledPrograms> installed_programs_;

  // Maintains the cache of problematic programs.
  std::unique_ptr<ProblematicProgramsUpdater> problematic_programs_updater_;

  base::WeakPtrFactory<ThirdPartyConflictsManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyConflictsManager);
};

#endif  // CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_
