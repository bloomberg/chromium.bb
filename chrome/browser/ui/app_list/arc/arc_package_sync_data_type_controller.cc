// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_package_sync_data_type_controller.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"

// ArcPackage sync service is controlled by apps checkbox in sync settings. ARC
// apps and regular Chrome apps have same user control.
namespace {

// Indicates whether ARC is enabled on this machine.
bool IsArcEnabled(Profile* profile) {
  return arc::IsArcAllowedForProfile(profile) &&
         profile->GetPrefs()->GetBoolean(prefs::kArcEnabled);
}

}  // namespace

ArcPackageSyncDataTypeController::ArcPackageSyncDataTypeController(
    syncer::ModelType type,
    const base::Closure& dump_stack,
    syncer::SyncClient* sync_client,
    Profile* profile)
    : syncer::AsyncDirectoryTypeController(type,
                                           dump_stack,
                                           sync_client,
                                           syncer::GROUP_UI,
                                           base::ThreadTaskRunnerHandle::Get()),
      profile_(profile) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kArcEnabled,
      base::Bind(&ArcPackageSyncDataTypeController::OnArcEnabledPrefChanged,
                 base::Unretained(this)));
}

ArcPackageSyncDataTypeController::~ArcPackageSyncDataTypeController() {
}

bool ArcPackageSyncDataTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return IsArcEnabled(profile_) && ShouldSyncArc();
}

bool ArcPackageSyncDataTypeController::StartModels() {
  DCHECK_EQ(state(), MODEL_STARTING);
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);
  model_normal_start_ = arc_prefs->package_list_initial_refreshed();
  arc_prefs->AddObserver(this);
  return model_normal_start_;
}

void ArcPackageSyncDataTypeController::StopModels() {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  if (arc_prefs)
    arc_prefs->RemoveObserver(this);
}

void ArcPackageSyncDataTypeController::OnPackageListInitialRefreshed() {
  // model_normal_start_ is true by default. Normally,
  // ArcPackageSyncDataTypeController::StartModels() gets called before ARC
  // package list is refreshed. But in integration test, the order can be either
  // way. If OnPackageListInitialRefreshed comes before
  // ArcPackageSyncDataTypeController ::StartModels(), this function is no-op
  // and waits for StartModels() to be called.
  if (model_normal_start_)
    return;

  model_normal_start_ = true;
  OnModelLoaded();
}

void ArcPackageSyncDataTypeController::OnArcEnabledPrefChanged() {
  DCHECK(CalledOnValidThread());

  if (!ReadyForStart()) {
    // If enable ARC in settings is turned off then generate an unrecoverable
    // error.
    if (state() != NOT_RUNNING && state() != STOPPING) {
      syncer::SyncError error(
          FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
          "ARC package sync is now disabled because user disables ARC.",
          type());
      CreateErrorHandler()->OnUnrecoverableError(error);
    }
    return;
  }
  EnableDataType();
}

void ArcPackageSyncDataTypeController::EnableDataType() {
  syncer::SyncService* sync_service = sync_client_->GetSyncService();
  DCHECK(sync_service);
  sync_service->ReenableDatatype(type());
}

bool ArcPackageSyncDataTypeController::ShouldSyncArc() const {
  syncer::SyncService* sync_service = sync_client_->GetSyncService();
  DCHECK(sync_service);
  return sync_service->GetPreferredDataTypes().Has(type());
}
