// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/autofill_wallet_data_type_controller.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"

using content::BrowserThread;

namespace browser_sync {

AutofillWalletDataTypeController::AutofillWalletDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    syncer::ModelType model_type)
    : NonUIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          profile_sync_factory),
      profile_(profile),
      callback_registered_(false),
      model_type_(model_type),
      currently_enabled_(IsEnabled()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(model_type_ == syncer::AUTOFILL_WALLET_DATA ||
         model_type_ == syncer::AUTOFILL_WALLET_METADATA);
  pref_registrar_.Init(profile->GetPrefs());
  pref_registrar_.Add(
      autofill::prefs::kAutofillWalletSyncExperimentEnabled,
      base::Bind(&AutofillWalletDataTypeController::OnSyncPrefChanged,
                 base::Unretained(this)));
  pref_registrar_.Add(
      autofill::prefs::kAutofillWalletImportEnabled,
      base::Bind(&AutofillWalletDataTypeController::OnSyncPrefChanged,
                 base::Unretained(this)));
}

AutofillWalletDataTypeController::~AutofillWalletDataTypeController() {
}

syncer::ModelType AutofillWalletDataTypeController::type() const {
  return model_type_;
}

syncer::ModelSafeGroup
    AutofillWalletDataTypeController::model_safe_group() const {
  return syncer::GROUP_DB;
}

bool AutofillWalletDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BrowserThread::PostTask(BrowserThread::DB, from_here, task);
}

bool AutofillWalletDataTypeController::StartModels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
          &AutofillWalletDataTypeController::OnModelLoaded, this));
     callback_registered_ = true;
  }

  return false;
}

void AutofillWalletDataTypeController::StopModels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This function is called when shutting down (nothing is changing), when
  // sync is disabled completely, or when wallet sync is disabled. In the
  // cases where wallet sync or sync in general is disabled, clear wallet cards
  // and addresses copied from the server. This is different than other sync
  // cases since this type of data reflects what's on the server rather than
  // syncing local data between clients, so this extra step is required.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);

  // HasSyncSetupCompleted indicates if sync is currently enabled at all. The
  // preferred data type indicates if wallet sync data/metadata is enabled, and
  // currently_enabled_ indicates if the other prefs are enabled. All of these
  // have to be enabled to sync wallet data/metadata.
  if (!service->HasSyncSetupCompleted() ||
      !service->GetPreferredDataTypes().Has(type()) ||
      !currently_enabled_) {
    autofill::PersonalDataManager* pdm =
        autofill::PersonalDataManagerFactory::GetForProfile(profile_);
    if (pdm)
      pdm->ClearAllServerData();
  }
}

bool AutofillWalletDataTypeController::ReadyForStart() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return currently_enabled_;
}

void AutofillWalletDataTypeController::OnSyncPrefChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool new_enabled = IsEnabled();
  if (currently_enabled_ == new_enabled)
    return;  // No change to sync state.
  currently_enabled_ = new_enabled;

  if (currently_enabled_) {
    // The experiment was just enabled. Trigger a reconfiguration. This will do
    // nothing if the type isn't preferred.
    ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    sync_service->ReenableDatatype(type());
  } else {
    // Post a task to the backend thread to stop the datatype.
    if (state() != NOT_RUNNING && state() != STOPPING) {
      PostTaskOnBackendThread(
          FROM_HERE,
          base::Bind(&DataTypeController::OnSingleDataTypeUnrecoverableError,
                     this,
                     syncer::SyncError(
                         FROM_HERE,
                         syncer::SyncError::DATATYPE_POLICY_ERROR,
                         "Wallet syncing is disabled by policy.",
                         type())));
    }
  }
}

bool AutofillWalletDataTypeController::IsEnabled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Require both the sync experiment and the user-visible pref to be
  // enabled to sync Wallet data/metadata.
  PrefService* ps = profile_->GetPrefs();
  return
      ps->GetBoolean(autofill::prefs::kAutofillWalletSyncExperimentEnabled) &&
      ps->GetBoolean(autofill::prefs::kAutofillWalletImportEnabled);
}

}  // namespace browser_sync
