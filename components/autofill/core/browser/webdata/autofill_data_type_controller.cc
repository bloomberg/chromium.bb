// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/experiments.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/syncable_service.h"

namespace browser_sync {

AutofillDataTypeController::AutofillDataTypeController(
    scoped_refptr<base::SingleThreadTaskRunner> db_thread,
    const base::Closure& dump_stack,
    syncer::SyncClient* sync_client,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : AsyncDirectoryTypeController(syncer::AUTOFILL,
                                   dump_stack,
                                   sync_client,
                                   syncer::GROUP_DB,
                                   std::move(db_thread)),
      web_data_service_(web_data_service),
      currently_enabled_(IsEnabled()) {
  pref_registrar_.Init(sync_client_->GetPrefService());
  pref_registrar_.Add(autofill::prefs::kAutofillEnabled,
                      base::Bind(&AutofillDataTypeController::OnUserPrefChanged,
                                 base::AsWeakPtr(this)));
}

void AutofillDataTypeController::WebDatabaseLoaded() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(MODEL_STARTING, state());

  OnModelLoaded();
}

AutofillDataTypeController::~AutofillDataTypeController() {
  DCHECK(CalledOnValidThread());
}

bool AutofillDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(MODEL_STARTING, state());

  if (!IsEnabled()) {
    DisableForPolicy();
    return false;
  }

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded()) {
    return true;
  } else {
    web_data_service_->RegisterDBLoadedCallback(base::Bind(
        &AutofillDataTypeController::WebDatabaseLoaded, base::AsWeakPtr(this)));
    return false;
  }
}

bool AutofillDataTypeController::ReadyForStart() const {
  DCHECK(CalledOnValidThread());
  return currently_enabled_;
}

void AutofillDataTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());

  bool new_enabled = IsEnabled();
  if (currently_enabled_ == new_enabled)
    return;  // No change to sync state.
  currently_enabled_ = new_enabled;

  if (currently_enabled_) {
    // The preference was just enabled. Trigger a reconfiguration. This will do
    // nothing if the type isn't preferred.
    syncer::SyncService* sync_service = sync_client_->GetSyncService();
    sync_service->ReenableDatatype(type());
  } else {
    DisableForPolicy();
  }
}

bool AutofillDataTypeController::IsEnabled() {
  DCHECK(CalledOnValidThread());

  // Require the user-visible pref to be enabled to sync Autofill data.
  PrefService* ps = sync_client_->GetPrefService();
  return ps->GetBoolean(autofill::prefs::kAutofillEnabled);
}

void AutofillDataTypeController::DisableForPolicy() {
  if (state() != NOT_RUNNING && state() != STOPPING) {
    CreateErrorHandler()->OnUnrecoverableError(
        syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_POLICY_ERROR,
                          "Autofill syncing is disabled by policy.", type()));
  }
}

}  // namespace browser_sync
