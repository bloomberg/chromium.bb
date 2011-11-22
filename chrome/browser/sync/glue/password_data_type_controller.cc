// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "base/task.h"
#include "chrome/browser/password_manager/password_store.h"
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
    Profile* profile)
    : NonFrontendDataTypeController(profile_sync_factory,
                                    profile) {
}

PasswordDataTypeController::~PasswordDataTypeController() {
}

bool PasswordDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  password_store_ = profile()->GetPasswordStore(Profile::EXPLICIT_ACCESS);
  if (!password_store_.get()) {
    SyncError error(
        FROM_HERE,
        "PasswordStore not initialized, password datatype controller aborting.",
        type());
    StartDoneImpl(ABORTED, NOT_RUNNING, error);
    return false;
  }
  return true;
}

bool PasswordDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  DCHECK(password_store_.get());
  password_store_->ScheduleTask(
      base::Bind(&PasswordDataTypeController::StartAssociation, this));
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

bool PasswordDataTypeController::StopAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), STOPPING);
  DCHECK(password_store_.get());
  password_store_->ScheduleTask(
      base::Bind(&PasswordDataTypeController::StopAssociation, this));
  return true;
}

syncable::ModelType PasswordDataTypeController::type() const {
  return syncable::PASSWORDS;
}

browser_sync::ModelSafeGroup PasswordDataTypeController::model_safe_group()
    const {
  return browser_sync::GROUP_PASSWORD;
}

void PasswordDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_COUNTS("Sync.PasswordRunFailures", 1);
}

void PasswordDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_TIMES("Sync.PasswordAssociationTime", time);
}

void PasswordDataTypeController::RecordStartFailure(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.PasswordStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
