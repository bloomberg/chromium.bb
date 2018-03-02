// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/browser/conflicts/installed_programs_win.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"
#include "chrome/browser/conflicts/proto/module_list.pb.h"
#include "url/gurl.h"

class ModuleListFilter;
class PrefRegistrySimple;

// A feature that controls whether Chrome warns about incompatible applications.
extern const base::Feature kIncompatibleApplicationsWarning;

// Maintains a list of problematic programs that are installed on the machine.
// These programs cause unwanted DLLs to be loaded into Chrome.
//
// Because the list is expensive to build, it is cached into the Local State
// file so that it is available at startup.
//
// When kIncompatibleApplicationsWarning is disabled, this class always behaves
// as-if there are no problematic programs on the computer. This makes it safe
// to use all of the class' static functions unconditionally.
class ProblematicProgramsUpdater : public ModuleDatabaseObserver {
 public:
  struct ProblematicProgram {
    ProblematicProgram(
        InstalledPrograms::ProgramInfo info,
        std::unique_ptr<chrome::conflicts::BlacklistAction> blacklist_action);
    ~ProblematicProgram();

    // Needed for std::remove_if().
    ProblematicProgram(ProblematicProgram&& problematic_program);
    ProblematicProgram& operator=(ProblematicProgram&& problematic_program);

    InstalledPrograms::ProgramInfo info;
    std::unique_ptr<chrome::conflicts::BlacklistAction> blacklist_action;
  };

  ~ProblematicProgramsUpdater() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Creates an instance of the updater. Returns nullptr if the
  // kIncompatibleApplicationsWarning experiment is disabled.
  //
  // |installed_programs| must outlive the lifetime of this class.
  static std::unique_ptr<ProblematicProgramsUpdater> MaybeCreate(
      const ModuleListFilter& module_list_filter,
      const InstalledPrograms& installed_programs);

  // Returns true if the cache contains at least one problematic program.
  static bool HasCachedPrograms();

  // Returns all the cached problematic programs.
  static std::vector<ProblematicProgram> GetCachedPrograms();

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

 private:
  ProblematicProgramsUpdater(const ModuleListFilter& module_list_filter,
                             const InstalledPrograms& installed_programs);

  const ModuleListFilter& module_list_filter_;

  const InstalledPrograms& installed_programs_;

  // Temporarily holds problematic programs that were recently found.
  std::vector<ProblematicProgram> problematic_programs_;

  // Becomes false on the first call to OnModuleDatabaseIdle.
  bool before_first_idle_ = true;

  DISALLOW_COPY_AND_ASSIGN(ProblematicProgramsUpdater);
};

#endif  // CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_
