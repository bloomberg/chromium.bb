// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/base/experiments.h"
#include "components/sync/model/sync_error.h"

namespace browser_sync {

AutofillDataTypeController::AutofillDataTypeController(
    scoped_refptr<base::SingleThreadTaskRunner> db_thread,
    const base::Closure& dump_stack,
    syncer::SyncClient* sync_client,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : NonUIDataTypeController(syncer::AUTOFILL,
                              dump_stack,
                              sync_client,
                              syncer::GROUP_DB,
                              std::move(db_thread)),
      web_data_service_(web_data_service) {}

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

}  // namespace browser_sync
