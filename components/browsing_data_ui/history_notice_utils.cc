// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data_ui/history_notice_utils.h"

#include "base/callback.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/history/core/browser/web_history_service.h"

namespace browsing_data_ui {

namespace testing {

bool g_override_other_forms_of_browsing_history_query = false;

}

void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const ProfileSyncService* sync_service,
    history::WebHistoryService* history_service,
    base::Callback<void(bool)> callback) {
  if (!sync_service ||
      !sync_service->IsSyncActive() ||
      sync_service->IsUsingSecondaryPassphrase() ||
      !history_service) {
    callback.Run(false);
    return;
  }

  history_service->QueryWebAndAppActivity(callback);
}

void ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
    const ProfileSyncService* sync_service,
    history::WebHistoryService* history_service,
    base::Callback<void(bool)> callback) {
  if (!history_service ||
      (!testing::g_override_other_forms_of_browsing_history_query &&
       !history_service->HasOtherFormsOfBrowsingHistory())) {
    callback.Run(false);
    return;
  }

  ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      sync_service, history_service, callback);
}

}  // namespace browsing_data_ui
