// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/webdata/autofill_web_data_service.h"
#include "components/autofill/browser/personal_data_manager.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

using content::BrowserThread;

namespace browser_sync {

AutofillProfileDataTypeController::AutofillProfileDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : NonUIDataTypeController(profile_sync_factory,
                              profile,
                              sync_service),
      personal_data_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(scoped_observer_(this)) {}

syncer::ModelType AutofillProfileDataTypeController::type() const {
  return syncer::AUTOFILL_PROFILE;
}

syncer::ModelSafeGroup
    AutofillProfileDataTypeController::model_safe_group() const {
  return syncer::GROUP_DB;
}

void AutofillProfileDataTypeController::WebDatabaseLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (scoped_observer_.IsObserving(web_data_service_.get()))
    scoped_observer_.Remove(web_data_service_.get());
  OnModelLoaded();
}

void AutofillProfileDataTypeController::OnPersonalDataChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);

  personal_data_->RemoveObserver(this);
  web_data_service_ = AutofillWebDataService::FromBrowserContext(profile());

  if (!web_data_service_)
    return;

  if (web_data_service_->IsDatabaseLoaded())
    OnModelLoaded();
  else
    scoped_observer_.Add(web_data_service_.get());
}

AutofillProfileDataTypeController::~AutofillProfileDataTypeController() {}

bool AutofillProfileDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return BrowserThread::PostTask(BrowserThread::DB, from_here, task);
}

bool AutofillProfileDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  // Waiting for the personal data is subtle:  we do this as the PDM resets
  // its cache of unique IDs once it gets loaded. If we were to proceed with
  // association, the local ids in the mappings would wind up colliding.
  personal_data_ = PersonalDataManagerFactory::GetForProfile(profile());
  if (!personal_data_->IsDataLoaded()) {
    personal_data_->AddObserver(this);
    return false;
  }

  web_data_service_ = AutofillWebDataService::FromBrowserContext(profile());

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded())
    return true;

  scoped_observer_.Add(web_data_service_.get());
  return false;
}

void AutofillProfileDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state() == STOPPING || state() == NOT_RUNNING);

  if (scoped_observer_.IsObserving(web_data_service_.get()))
    scoped_observer_.Remove(web_data_service_.get());

  personal_data_->RemoveObserver(this);
}

}  // namepsace browser_sync
