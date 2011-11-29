// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

AutofillDataTypeController::AutofillDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile)
    : NewNonFrontendDataTypeController(profile_sync_factory, profile) {
}

AutofillDataTypeController::~AutofillDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool AutofillDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(MODEL_STARTING, state());

  web_data_service_ = profile()->GetWebDataService(Profile::IMPLICIT_ACCESS);
  if (web_data_service_->IsDatabaseLoaded()) {
    return true;
  } else {
    notification_registrar_.Add(
        this, chrome::NOTIFICATION_WEB_DATABASE_LOADED,
        content::Source<WebDataService>(web_data_service_.get()));
    return false;
  }
}

void AutofillDataTypeController::Observe(
    int notification_type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(chrome::NOTIFICATION_WEB_DATABASE_LOADED, notification_type);
  DCHECK_EQ(MODEL_STARTING, state());
  notification_registrar_.RemoveAll();
  set_state(ASSOCIATING);
  if (!StartAssociationAsync()) {
    SyncError error(FROM_HERE, "Failed to post association task.", type());
    StartDoneImpl(ASSOCIATION_FAILED, DISABLED, error);
  }
}

bool AutofillDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(ASSOCIATING, state());
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&AutofillDataTypeController::StartAssociation, this));
}

base::WeakPtr<SyncableService>
    AutofillDataTypeController::GetWeakPtrToSyncableService() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  return profile_sync_factory()->GetAutocompleteSyncableService(
      web_data_service_.get());
}

void AutofillDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state() == STOPPING || state() == NOT_RUNNING || state() == DISABLED);
  DVLOG(1) << "AutofillDataTypeController::StopModels() : State = " << state();
  notification_registrar_.RemoveAll();
}

void AutofillDataTypeController::StopLocalServiceAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&AutofillDataTypeController::StopLocalService, this));
}

syncable::ModelType AutofillDataTypeController::type() const {
  return syncable::AUTOFILL;
}

browser_sync::ModelSafeGroup AutofillDataTypeController::model_safe_group()
    const {
  return browser_sync::GROUP_DB;
}

void AutofillDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  UMA_HISTOGRAM_COUNTS("Sync.AutofillRunFailures", 1);
}

void AutofillDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  UMA_HISTOGRAM_TIMES("Sync.AutofillAssociationTime", time);
}

void AutofillDataTypeController::RecordStartFailure(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.AutofillStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
