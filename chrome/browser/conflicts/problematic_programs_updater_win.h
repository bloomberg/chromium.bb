// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/conflicts/installed_programs_win.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"
#include "chrome/browser/conflicts/proto/module_list.pb.h"

struct CertificateInfo;
class ModuleListFilter;
class PrefRegistrySimple;

// Maintains a list of problematic programs that are installed on the machine.
// These programs cause unwanted DLLs to be loaded into Chrome.
//
// Because the list is expensive to build, it is cached into the Local State
// file so that it is available at startup.
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

  // Creates an instance of the updater.
  // The parameters must outlive the lifetime of this class.
  ProblematicProgramsUpdater(const CertificateInfo& exe_certificate_info,
                             const ModuleListFilter& module_list_filter,
                             const InstalledPrograms& installed_programs);
  ~ProblematicProgramsUpdater() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Returns true if the tracking of incompatible applications is enabled. The
  // return value will not change throughout the lifetime of the process.
  static bool IsIncompatibleApplicationsWarningEnabled();

  // Returns true if the cache contains at least one problematic program.
  // Only call this if IsIncompatibleApplicationsWarningEnabled() returns true.
  static bool HasCachedPrograms();

  // Returns all the cached problematic programs.
  // Only call this if IsIncompatibleApplicationsWarningEnabled() returns true.
  static std::vector<ProblematicProgram> GetCachedPrograms();

  // ModuleDatabaseObserver:
  void OnNewModuleFound(const ModuleInfoKey& module_key,
                        const ModuleInfoData& module_data) override;
  void OnModuleDatabaseIdle() override;

 private:
  const CertificateInfo& exe_certificate_info_;

  const ModuleListFilter& module_list_filter_;

  const InstalledPrograms& installed_programs_;

  // Temporarily holds problematic programs that were recently found.
  std::vector<ProblematicProgram> problematic_programs_;

  // Becomes false on the first call to OnModuleDatabaseIdle.
  bool before_first_idle_ = true;

  DISALLOW_COPY_AND_ASSIGN(ProblematicProgramsUpdater);
};

#endif  // CHROME_BROWSER_CONFLICTS_PROBLEMATIC_PROGRAMS_UPDATER_WIN_H_
