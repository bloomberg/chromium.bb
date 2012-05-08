// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

PasswordDataTypeController::PasswordDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : NonFrontendDataTypeController(profile_sync_factory,
                                    profile,
                                    sync_service) {
}

syncable::ModelType PasswordDataTypeController::type() const {
  return syncable::PASSWORDS;
}

browser_sync::ModelSafeGroup PasswordDataTypeController::model_safe_group()
    const {
  return browser_sync::GROUP_PASSWORD;
}

PasswordDataTypeController::~PasswordDataTypeController() {}

bool PasswordDataTypeController::PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(password_store_.get());
  return password_store_->ScheduleTask(task);
}

bool PasswordDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  password_store_ = PasswordStoreFactory::GetForProfile(
      profile(), Profile::EXPLICIT_ACCESS);
  if (!password_store_.get()) {
    SyncError error(
        FROM_HERE,
        "PasswordStore not initialized, password datatype controller aborting.",
        type());
    StartDoneImpl(ASSOCIATION_FAILED, DISABLED, error);
    return false;
  }
  return true;
}

void PasswordDataTypeController::CreateSyncComponents() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory()->CreatePasswordSyncComponents(
          profile_sync_service(),
          password_store_.get(),
          this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

}  // namespace browser_sync
