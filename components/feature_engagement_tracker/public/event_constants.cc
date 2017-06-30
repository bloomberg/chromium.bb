// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/public/event_constants.h"

namespace feature_engagement_tracker {

namespace events {

#if defined(OS_WIN) || defined(OS_LINUX)
const char kNewTabOpened[] = "new_tab_opened";
const char kOmniboxInteraction[] = "omnibox_used";

const char kHistoryDeleted[] = "history_deleted";
const char kIncognitoWindowOpened[] = "incognito_window_opened";

const char kSessionTime[] = "session_time";
#endif  // defined(OS_WIN) || defined(OS_LINUX)

}  // namespace events

}  // namespace feature_engagement_tracker
