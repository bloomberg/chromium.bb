// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/event_constants.h"

namespace feature_engagement {

namespace events {

#if defined(OS_WIN) || defined(OS_LINUX)
const char kOmniboxInteraction[] = "omnibox_used";
const char kNewTabSessionTimeMet[] = "new_tab_session_time_met";

const char kHistoryDeleted[] = "history_deleted";
const char kIncognitoWindowOpened[] = "incognito_window_opened";

#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_IOS)
const char kNewTabOpened[] = "new_tab_opened";
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_IOS)

#if defined(OS_IOS)
const char kChromeOpened[] = "chrome_opened";
const char kIncognitoTabOpened[] = "incognito_tab_opened";
const char kClearedBrowsingData[] = "cleared_browsing_data";
const char kViewedReadingList[] = "viewed_reading_list";
#endif  // defined(OS_IOS)

}  // namespace events

}  // namespace feature_engagement
