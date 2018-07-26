// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_model_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_client.h"

using syncer::SyncClient;

namespace history {

namespace {

std::unique_ptr<syncer::ModelTypeControllerDelegate>
GetDelegateFromHistoryService(HistoryService* history_service) {
  if (history_service) {
    return history_service->GetTypedURLSyncControllerDelegate();
  }
  return nullptr;
}

}  // namespace

TypedURLModelTypeController::TypedURLModelTypeController(
    SyncClient* sync_client,
    const char* history_disabled_pref_name)
    : ModelTypeController(
          syncer::TYPED_URLS,
          GetDelegateFromHistoryService(sync_client->GetHistoryService()),
          sync_client),
      history_disabled_pref_name_(history_disabled_pref_name),
      history_service_(sync_client->GetHistoryService()) {
  pref_registrar_.Init(sync_client->GetPrefService());
  pref_registrar_.Add(
      history_disabled_pref_name_,
      base::Bind(
          &TypedURLModelTypeController::OnSavingBrowserHistoryDisabledChanged,
          base::AsWeakPtr(this)));
}

TypedURLModelTypeController::~TypedURLModelTypeController() {}

bool TypedURLModelTypeController::ReadyForStart() const {
  return history_service_ && !sync_client()->GetPrefService()->GetBoolean(
                                 history_disabled_pref_name_);
}

void TypedURLModelTypeController::OnSavingBrowserHistoryDisabledChanged() {
  if (sync_client()->GetPrefService()->GetBoolean(
          history_disabled_pref_name_)) {
    // We've turned off history persistence, so if we are running,
    // generate an unrecoverable error. This can be fixed by restarting
    // Chrome (on restart, typed urls will not be a registered type).
    if (state() != NOT_RUNNING && state() != STOPPING) {
      ReportModelError(
          syncer::SyncError::DATATYPE_POLICY_ERROR,
          {FROM_HERE, "History saving is now disabled by policy."});
    }
  }
}

}  // namespace history
