// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_UI_HISTORY_NOTICE_UTILS_H_
#define COMPONENTS_BROWSING_DATA_UI_HISTORY_NOTICE_UTILS_H_

#include "base/callback_forward.h"

class ProfileSyncService;

namespace history {
class WebHistoryService;
}

namespace browsing_data_ui {

// Whether the Clear Browsing Data UI should show a notice about the existence
// of other forms of browsing history stored in user's account. The response
// is returned in a |callback|.
void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const ProfileSyncService* sync_service,
    history::WebHistoryService* history_service,
    base::Callback<void(bool)> callback);

}  // namespace browsing_data_ui

#endif  // COMPONENTS_BROWSING_DATA_UI_HISTORY_NOTICE_UTILS_H_
