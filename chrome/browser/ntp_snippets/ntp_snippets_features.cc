// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/ntp_snippets_features.h"

namespace params {
namespace ntp_snippets {

const base::Feature kNotificationsFeature = {"ContentSuggestionsNotifications",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const char kNotificationsPriorityParam[] = "priority";
const char kNotificationsAlwaysNotifyParam[] = "always_notify";
const char kNotificationsUseSnippetAsTextParam[] = "use_snippet_as_text";
const char kNotificationsKeepWhenFrontmostParam[] =
    "keep_notification_when_frontmost";
const char kNotificationsOpenToNTPParam[] = "open_to_ntp";
const char kNotificationsDailyLimit[] = "daily_limit";
const char kNotificationsIgnoredLimitParam[] = "ignored_limit";

}  // namespace ntp_snippets
}  // namespace params
