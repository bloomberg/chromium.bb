// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data_ui/history_notice_utils.h"

#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/history/core/browser/web_history_service.h"

namespace browsing_data_ui {

bool ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const ProfileSyncService* sync_service,
    const history::WebHistoryService* history_service) {
  return sync_service &&
         sync_service->IsUsingSecondaryPassphrase() &&
         history_service &&
         history_service->HasOtherFormsOfBrowsingHistory();
}

}  // namespace browsing_data_ui
