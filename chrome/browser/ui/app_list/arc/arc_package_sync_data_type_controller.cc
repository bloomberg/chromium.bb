// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_package_sync_data_type_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_prefs.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_thread.h"

// ArcPackage sync service is controlled by apps checkbox in sync settings. Arc
// apps and regular Chrome apps have same user control.
ArcPackageSyncDataTypeController::ArcPackageSyncDataTypeController(
    syncer::ModelType type,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    Profile* profile)
    : sync_driver::UIDataTypeController(
          content::BrowserThread::GetTaskRunnerForThread(
              content::BrowserThread::UI),
          error_callback,
          type,
          sync_client),
      profile_(profile),
      sync_client_(sync_client) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      sync_driver::SyncPrefs::GetPrefNameForDataType(type),
      base::Bind(&ArcPackageSyncDataTypeController::OnArcAppsSyncPrefChanged,
                 base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kArcEnabled,
      base::Bind(&ArcPackageSyncDataTypeController::OnArcEnabledPrefChanged,
                 base::Unretained(this)));
}

ArcPackageSyncDataTypeController::~ArcPackageSyncDataTypeController() {}

bool ArcPackageSyncDataTypeController::ReadyForStart() const {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  return profile_->GetPrefs()->GetBoolean(
             sync_driver::SyncPrefs::GetPrefNameForDataType(type())) &&
         prefs && prefs->app_instance_holder()->instance();
}

void ArcPackageSyncDataTypeController::OnArcAppsSyncPrefChanged() {
  DCHECK(ui_thread()->BelongsToCurrentThread());

  if (!ReadyForStart()) {
    // If apps sync in advanced sync settings is turned off then generate an
    // unrecoverable error.
    if (state() != NOT_RUNNING && state() != STOPPING) {
      syncer::SyncError error(
          FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
          "Arc package sync is now disabled by user.", type());
      OnSingleDataTypeUnrecoverableError(error);
    }
    return;
  }
  sync_driver::SyncService* sync_service = sync_client_->GetSyncService();
  DCHECK(sync_service);
  sync_service->ReenableDatatype(type());
}

void ArcPackageSyncDataTypeController::OnArcEnabledPrefChanged() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
    // If enable Arc in settings is turned off then generate an unrecoverable
    // error.
    if (state() != NOT_RUNNING && state() != STOPPING) {
      syncer::SyncError error(
          FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
          "Arc package sync is now disabled because user disables Arc.",
          type());
      OnSingleDataTypeUnrecoverableError(error);
    }
  }
}
