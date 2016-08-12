// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_package_sync_data_type_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/instance_holder.h"
#include "content/public/browser/browser_thread.h"

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
      profile_(profile) {}

ArcPackageSyncDataTypeController::~ArcPackageSyncDataTypeController() {}

bool ArcPackageSyncDataTypeController::ReadyForStart() const {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs && prefs->app_instance_holder()->instance())
    return true;
  return false;
}
