// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/third_party_conflicts_manager_win.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/conflicts/installed_programs_win.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_list_filter_win.h"
#include "chrome/browser/conflicts/problematic_programs_updater_win.h"

namespace {

std::unique_ptr<ModuleListFilter> CreateModuleListFilter(
    const base::FilePath& module_list_path) {
  auto module_list_filter = std::make_unique<ModuleListFilter>();

  if (!module_list_filter->Initialize(module_list_path))
    return nullptr;

  return module_list_filter;
}

}  // namespace

ThirdPartyConflictsManager::ThirdPartyConflictsManager(
    ModuleDatabase* module_database)
    : module_database_(module_database),
      module_list_received_(false),
      on_module_database_idle_called_(false),
      weak_ptr_factory_(this) {}

ThirdPartyConflictsManager::~ThirdPartyConflictsManager() = default;

void ThirdPartyConflictsManager::OnModuleDatabaseIdle() {
  if (on_module_database_idle_called_)
    return;

  on_module_database_idle_called_ = true;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce([]() { return std::make_unique<InstalledPrograms>(); }),
      base::BindOnce(&ThirdPartyConflictsManager::OnInstalledProgramsCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ThirdPartyConflictsManager::LoadModuleList(const base::FilePath& path) {
  if (module_list_received_)
    return;

  module_list_received_ = true;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CreateModuleListFilter, path),
      base::BindOnce(&ThirdPartyConflictsManager::OnModuleListFilterCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ThirdPartyConflictsManager::OnModuleListFilterCreated(
    std::unique_ptr<ModuleListFilter> module_list_filter) {
  module_list_filter_ = std::move(module_list_filter);

  // A valid |module_list_filter_| is critical to the blocking of third-party
  // modules. By returning early here, the |problematic_programs_updater_|
  // instance never gets created, thus disabling the identification of
  // incompatible applications.
  if (!module_list_filter_) {
    // Mark the module list as not received so that a new one may trigger the
    // creation of a valid filter.
    module_list_received_ = false;
    return;
  }

  if (installed_programs_)
    InitializeProblematicProgramsUpdater();
}

void ThirdPartyConflictsManager::OnInstalledProgramsCreated(
    std::unique_ptr<InstalledPrograms> installed_programs) {
  installed_programs_ = std::move(installed_programs);

  if (module_list_filter_)
    InitializeProblematicProgramsUpdater();
}

void ThirdPartyConflictsManager::InitializeProblematicProgramsUpdater() {
  DCHECK(module_list_filter_);
  DCHECK(installed_programs_);
  problematic_programs_updater_ = ProblematicProgramsUpdater::MaybeCreate(
      *module_list_filter_, *installed_programs_);
  if (problematic_programs_updater_)
    module_database_->AddObserver(problematic_programs_updater_.get());
}
