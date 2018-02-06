// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/conflicts/installed_programs_win.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"

class PrefRegistrySimple;

// A feature that controls whether Chrome keeps track of problematic programs.
extern const base::Feature kThirdPartyConflictsWarning;

// Maintains a list of problematic programs that are installed on the machine.
// These programs cause unwanted DLLs to be loaded into Chrome.
//
// Because the list is expensive to build, it is cached into the Local State
// file so that it is available at startup, albeit somewhat out-of-date. To
// remove stale elements from the list, use TrimCache().
//
// When kThirdPartyConflictsWarning is disabled, this class always behaves as-if
// there are no problematic programs on the computer. This makes it safe to use
// all of the class' static functions unconditionally.
class ProblematicProgramsUpdater : public ModuleDatabaseObserver {
 public:
  ~ProblematicProgramsUpdater() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Creates an instance of the updater. Returns nullptr if the
  // kThirdPartyConflictsWarning experiment is disabled.
  //
  // |installed_programs| must outlive the lifetime of this class.
  static std::unique_ptr<ProblematicProgramsUpdater> MaybeCreate(
      const InstalledPrograms& installed_programs);

  // Removes stale programs from the cache. This can happen if a program was
  // uninstalled between the time it was found and Chrome was relaunched.
  // Returns true if any problematic programs are installed after trimming
  // completes (i.e., HasCachedPrograms() would return true).
  static bool TrimCache();

  // Returns true if the cache contains at least one problematic program.
  static bool HasCachedPrograms();

  // Returns the names of all the cached problematic programs in a list Value.
  static base::Value GetCachedProgramNames();

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

 private:
  explicit ProblematicProgramsUpdater(
      const InstalledPrograms& installed_programs);

  const InstalledPrograms& installed_programs_;

  // Temporarily holds program names for modules that were recently found.
  std::vector<InstalledPrograms::ProgramInfo> programs_;

  // Becomes false on the first call to OnModuleDatabaseIdle.
  bool before_first_idle_ = true;

  DISALLOW_COPY_AND_ASSIGN(ProblematicProgramsUpdater);
};

#endif  // CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_
