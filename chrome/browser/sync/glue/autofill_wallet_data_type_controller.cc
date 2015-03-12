// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_wallet_data_type_controller.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

using content::BrowserThread;

namespace browser_sync {

AutofillWalletDataTypeController::AutofillWalletDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile)
    : NonUIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          profile_sync_factory),
      profile_(profile),
      callback_registered_(false) {
}

AutofillWalletDataTypeController::~AutofillWalletDataTypeController() {
}

syncer::ModelType AutofillWalletDataTypeController::type() const {
  return syncer::AUTOFILL_WALLET_DATA;
}

syncer::ModelSafeGroup
    AutofillWalletDataTypeController::model_safe_group() const {
  return syncer::GROUP_DB;
}

bool AutofillWalletDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return BrowserThread::PostTask(BrowserThread::DB, from_here, task);
}

bool AutofillWalletDataTypeController::StartModels() {
  DCHECK(content::BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);

  autofill::AutofillWebDataService* web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS).get();

  if (!web_data_service)
    return false;

  if (web_data_service->IsDatabaseLoaded())
    return true;

  if (!callback_registered_) {
     web_data_service->RegisterDBLoadedCallback(base::Bind(
          &AutofillWalletDataTypeController::WebDatabaseLoaded, this));
     callback_registered_ = true;
  }

  return false;
}

void AutofillWalletDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void AutofillWalletDataTypeController::WebDatabaseLoaded() {
  OnModelLoaded();
}

}  // namespace browser_sync
