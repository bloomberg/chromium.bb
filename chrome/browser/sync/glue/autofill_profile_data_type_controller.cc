// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync_driver/sync_client.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

using autofill::AutofillWebDataService;
using content::BrowserThread;

namespace browser_sync {

AutofillProfileDataTypeController::AutofillProfileDataTypeController(
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client)
    : NonUIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          error_callback,
          sync_client),
      sync_client_(sync_client),
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnModelLoaded();
}

void AutofillProfileDataTypeController::OnPersonalDataChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state(), MODEL_STARTING);

  sync_client_->GetPersonalDataManager()->RemoveObserver(this);
  scoped_refptr<autofill::AutofillWebDataService> web_data_service =
      sync_client_->GetWebDataService();

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BrowserThread::PostTask(BrowserThread::DB, from_here, task);
}

bool AutofillProfileDataTypeController::StartModels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(state(), MODEL_STARTING);
  // Waiting for the personal data is subtle:  we do this as the PDM resets
  // its cache of unique IDs once it gets loaded. If we were to proceed with
  // association, the local ids in the mappings would wind up colliding.
  autofill::PersonalDataManager* personal_data =
      sync_client_->GetPersonalDataManager();
  if (!personal_data->IsDataLoaded()) {
    personal_data->AddObserver(this);
    return false;
  }

  scoped_refptr<autofill::AutofillWebDataService> web_data_service =
      sync_client_->GetWebDataService();

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_client_->GetPersonalDataManager()->RemoveObserver(this);
}

}  // namespace browser_sync
