// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "base/task.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

AutofillProfileDataTypeController::AutofillProfileDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile)
    : NewNonFrontendDataTypeController(profile_sync_factory, profile),
      personal_data_(NULL) {
}

AutofillProfileDataTypeController::~AutofillProfileDataTypeController() {}

bool AutofillProfileDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  // Waiting for the personal data is subtle:  we do this as the PDM resets
  // its cache of unique IDs once it gets loaded. If we were to proceed with
  // association, the local ids in the mappings would wind up colliding.
  personal_data_ = PersonalDataManagerFactory::GetForProfile(profile());
  if (!personal_data_->IsDataLoaded()) {
    personal_data_->SetObserver(this);
    return false;
  }

  web_data_service_ = profile()->GetWebDataService(Profile::IMPLICIT_ACCESS);
  if (web_data_service_.get() && web_data_service_->IsDatabaseLoaded()) {
    return true;
  } else {
    notification_registrar_.Add(this, chrome::NOTIFICATION_WEB_DATABASE_LOADED,
                                content::NotificationService::AllSources());
    return false;
  }
}

void AutofillProfileDataTypeController::OnPersonalDataChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  personal_data_->RemoveObserver(this);
  web_data_service_ = profile()->GetWebDataService(Profile::IMPLICIT_ACCESS);
  if (web_data_service_.get() && web_data_service_->IsDatabaseLoaded()) {
    DoStartAssociationAsync();
  } else {
    notification_registrar_.Add(this, chrome::NOTIFICATION_WEB_DATABASE_LOADED,
                                content::NotificationService::AllSources());
  }
}

void AutofillProfileDataTypeController::Observe(
    int notification_type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  notification_registrar_.RemoveAll();
  DoStartAssociationAsync();
}

void AutofillProfileDataTypeController::DoStartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  set_state(ASSOCIATING);
  if (!StartAssociationAsync()) {
    SyncError error(FROM_HERE,
                    "Failed to post association task.",
                    type());
    StartDoneImpl(ASSOCIATION_FAILED, NOT_RUNNING, error);
  }
}

bool AutofillProfileDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&AutofillProfileDataTypeController::StartAssociation,
                 this));
}

base::WeakPtr<SyncableService>
    AutofillProfileDataTypeController::GetWeakPtrToSyncableService() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  return profile_sync_factory()->GetAutofillProfileSyncableService(
      web_data_service_.get());
}

void AutofillProfileDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state() == STOPPING || state() == NOT_RUNNING);
  notification_registrar_.RemoveAll();
  personal_data_->RemoveObserver(this);
}

void AutofillProfileDataTypeController::StopLocalServiceAsync() {
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&AutofillProfileDataTypeController::StopLocalService,
                 this));
}
syncable::ModelType AutofillProfileDataTypeController::type() const {
  return syncable::AUTOFILL_PROFILE;
}

browser_sync::ModelSafeGroup
    AutofillProfileDataTypeController::model_safe_group() const {
  return browser_sync::GROUP_DB;
}

void AutofillProfileDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  UMA_HISTOGRAM_COUNTS("Sync.AutofillProfileRunFailures", 1);
}

void AutofillProfileDataTypeController::RecordAssociationTime(
    base::TimeDelta time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  UMA_HISTOGRAM_TIMES("Sync.AutofillProfileAssociationTime", time);
}

void AutofillProfileDataTypeController::RecordStartFailure(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.AutofillProfileStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namepsace browser_sync
