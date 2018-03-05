// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/third_party_conflicts_manager_win.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/module_list_filter_win.h"
#include "chrome/browser/conflicts/problematic_programs_updater_win.h"

ThirdPartyConflictsManager::ThirdPartyConflictsManager(
    ModuleDatabase* module_database)
    : module_database_(module_database), module_list_received_(false) {}

ThirdPartyConflictsManager::~ThirdPartyConflictsManager() = default;

void ThirdPartyConflictsManager::OnModuleDatabaseIdle() {
  if (installed_programs_.initialized())
    return;

  // ThirdPartyConflictsManager owns |installed_programs_|, so it is safe to use
  // base::Unretained().
  installed_programs_.Initialize(base::BindOnce(
      &ThirdPartyConflictsManager::OnInstalledProgramsInitialized,
      base::Unretained(this)));
}

void ThirdPartyConflictsManager::LoadModuleList(const base::FilePath& path) {
  // No attempt is made to dynamically reconcile a new module list version. The
  // next Chrome launch will pick it up.
  if (module_list_received_)
    return;

  auto module_list_filter = std::make_unique<ModuleListFilter>();
  if (!module_list_filter->Initialize(path))
    return;

  module_list_filter_ = std::move(module_list_filter);

  // Mark the module list as received here so that if Initialize() fails,
  // another attempt will be made with a newer version.
  module_list_received_ = true;

  if (installed_programs_.initialized())
    InitializeProblematicProgramsUpdater();
}

void ThirdPartyConflictsManager::OnInstalledProgramsInitialized() {
  if (module_list_received_)
    InitializeProblematicProgramsUpdater();
}

void ThirdPartyConflictsManager::InitializeProblematicProgramsUpdater() {
  DCHECK(module_list_received_);
  DCHECK(installed_programs_.initialized());
  problematic_programs_updater_ = ProblematicProgramsUpdater::MaybeCreate(
      *module_list_filter_, installed_programs_);
  if (problematic_programs_updater_)
    module_database_->AddObserver(problematic_programs_updater_.get());
}
