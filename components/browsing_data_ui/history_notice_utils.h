// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_UI_HISTORY_NOTICE_UTILS_H_
#define COMPONENTS_BROWSING_DATA_UI_HISTORY_NOTICE_UTILS_H_

class ProfileSyncService;

namespace history {
class WebHistoryService;
}

namespace browsing_data_ui {

// Whether the Clear Browsing Data UI should show a notice about the existence
// of other forms of browsing history stored in user's account.
bool ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const ProfileSyncService* sync_service,
    const history::WebHistoryService* history_service);

}  // namespace browsing_data_ui

#endif  // COMPONENTS_BROWSING_DATA_UI_HISTORY_NOTICE_UTILS_H_
