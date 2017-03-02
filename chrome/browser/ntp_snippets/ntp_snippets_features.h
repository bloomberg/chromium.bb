// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_
#define CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_

#include "base/feature_list.h"

namespace params {
namespace ntp_snippets {

// Enables and configures notifications for content suggestions.
extern const base::Feature kNotificationsFeature;

// An integer. The priority of the notification, ranging from -2 (PRIORITY_MIN)
// to 2 (PRIORITY_MAX). Vibrates and makes sound if >= 0.
extern const char kNotificationsPriorityParam[];
constexpr int kNotificationsDefaultPriority = -1;

// "false": use server signals to decide whether to send a notification
// "true": always send a notification when we receive ARTICLES suggestions
extern const char kNotificationsAlwaysNotifyParam[];

// "true": use article's snippet as notification's text
// "false": use article's publisher as notification's text
extern const char kNotificationsUseSnippetAsTextParam[];

// "true": when Chrome becomes frontmost, leave notifications open.
// "false": automatically dismiss notification when Chrome becomes frontmost.
extern const char kNotificationsKeepWhenFrontmostParam[];

// "true": notifications link to chrome://newtab, with appropriate text.
// "false": notifications link to URL of notifying article.
extern const char kNotificationsOpenToNTPParam[];

// An integer. The maximum number of notifications that will be shown in 1 day.
extern const char kNotificationsDailyLimit[];
constexpr int kNotificationsDefaultDailyLimit = 1;

// An integer. The number of notifications that can be ignored. If the user
// ignores this many notifications or more, we stop sending them.
extern const char kNotificationsIgnoredLimitParam[];
constexpr int kNotificationsIgnoredDefaultLimit = 3;

}  // namespace ntp_snippets
}  // namespace params

#endif  // CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_
