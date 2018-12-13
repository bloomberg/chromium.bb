// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_data_type_controller.h"

#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"

namespace browser_sync {

ExtensionDataTypeController::ExtensionDataTypeController(
    syncer::ModelType type,
    const base::Closure& dump_stack,
    syncer::SyncService* sync_service,
    syncer::SyncClient* sync_client,
    Profile* profile)
    : AsyncDirectoryTypeController(type,
                                   dump_stack,
                                   sync_service,
                                   sync_client,
                                   syncer::GROUP_UI,
                                   base::ThreadTaskRunnerHandle::Get()),
      profile_(profile) {
  DCHECK(type == syncer::EXTENSIONS || type == syncer::APPS);
}

ExtensionDataTypeController::~ExtensionDataTypeController() {}

bool ExtensionDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(
      true /* extensions_enabled */);
  return true;
}

}  // namespace browser_sync
