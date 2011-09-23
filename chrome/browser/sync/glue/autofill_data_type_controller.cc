// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "base/task.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

namespace browser_sync {

AutofillDataTypeController::AutofillDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile)
    : NonFrontendDataTypeController(profile_sync_factory, profile),
      personal_data_(NULL) {
}

AutofillDataTypeController::~AutofillDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool AutofillDataTypeController::StartModels() {
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
                                NotificationService::AllSources());
    return false;
  }
}

void AutofillDataTypeController::OnPersonalDataChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  personal_data_->RemoveObserver(this);
  web_data_service_ = profile()->GetWebDataService(Profile::IMPLICIT_ACCESS);
  if (web_data_service_.get() && web_data_service_->IsDatabaseLoaded()) {
    set_state(ASSOCIATING);
    if (!StartAssociationAsync()) {
      StartDoneImpl(ASSOCIATION_FAILED, NOT_RUNNING, FROM_HERE);
    }
  } else {
    notification_registrar_.Add(this, chrome::NOTIFICATION_WEB_DATABASE_LOADED,
                                NotificationService::AllSources());
  }
}

void AutofillDataTypeController::Observe(int type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  notification_registrar_.RemoveAll();
  set_state(ASSOCIATING);
  if (!StartAssociationAsync()) {
    StartDoneImpl(ASSOCIATION_FAILED, NOT_RUNNING, FROM_HERE);
  }
}

bool AutofillDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      NewRunnableMethod(
          this,
          &AutofillDataTypeController::StartAssociation));
}

void AutofillDataTypeController::CreateSyncComponents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK_EQ(state(), ASSOCIATING);
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory()->
          CreateAutofillSyncComponents(
          profile_sync_service(),
          web_data_service_->GetDatabase(),
          this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

void AutofillDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state() == STOPPING || state() == NOT_RUNNING);
  notification_registrar_.RemoveAll();
  personal_data_->RemoveObserver(this);
}

bool AutofillDataTypeController::StopAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), STOPPING);
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      NewRunnableMethod(
          this,
          &AutofillDataTypeController::StopAssociation));
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

PersonalDataManager* AutofillDataTypeController::personal_data() const {
  return personal_data_;
}

WebDataService* AutofillDataTypeController::web_data_service() const {
  return web_data_service_;
}

}  // namespace browser_sync
