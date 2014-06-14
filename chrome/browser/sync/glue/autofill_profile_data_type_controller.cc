// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

using autofill::AutofillWebDataService;
using content::BrowserThread;

namespace browser_sync {

AutofillProfileDataTypeController::AutofillProfileDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    const DisableTypeCallback& disable_callback)
    : NonUIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          disable_callback,
          profile_sync_factory),
      profile_(profile),
      personal_data_(NULL),
      callback_registered_(false) {}

syncer::ModelType AutofillProfileDataTypeController::type() const {
  return syncer::AUTOFILL_PROFILE;
}

syncer::ModelSafeGroup
    AutofillProfileDataTypeController::model_safe_group() const {
  return syncer::GROUP_DB;
}

void AutofillProfileDataTypeController::WebDatabaseLoaded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  OnModelLoaded();
}

void AutofillProfileDataTypeController::OnPersonalDataChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);

  personal_data_->RemoveObserver(this);
  autofill::AutofillWebDataService* web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, Profile::EXPLICIT_ACCESS).get();

  if (!web_data_service)
    return;

  if (web_data_service->IsDatabaseLoaded()) {
    OnModelLoaded();
  } else  if (!callback_registered_) {
     web_data_service->RegisterDBLoadedCallback(base::Bind(
        &AutofillProfileDataTypeController::WebDatabaseLoaded, this));
     callback_registered_ = true;
  }
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
  personal_data_ =
      autofill::PersonalDataManagerFactory::GetForProfile(profile_);
  if (!personal_data_->IsDataLoaded()) {
    personal_data_->AddObserver(this);
    return false;
  }

  autofill::AutofillWebDataService* web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, Profile::EXPLICIT_ACCESS).get();

  if (!web_data_service)
    return false;

  if (web_data_service->IsDatabaseLoaded())
    return true;

  if (!callback_registered_) {
     web_data_service->RegisterDBLoadedCallback(base::Bind(
        &AutofillProfileDataTypeController::WebDatabaseLoaded, this));
     callback_registered_ = true;
  }

  return false;
}

void AutofillProfileDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  personal_data_->RemoveObserver(this);
}

}  // namespace browser_sync
