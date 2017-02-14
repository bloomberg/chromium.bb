// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_
#define CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_

#include "base/feature_list.h"

// Enables and configures notifications for content suggestions.
extern const base::Feature kContentSuggestionsNotificationsFeature;

// "false": use server signals to decide whether to send a notification
// "true": always send a notification when we receive ARTICLES suggestions
extern const char kContentSuggestionsNotificationsAlwaysNotifyParam[];

// "true": use article's snippet as notification's text
// "false": use article's publisher as notification's text
extern const char kContentSuggestionsNotificationsUseSnippetAsTextParam[];

// "true": when Chrome becomes frontmost, leave notifications open.
// "false": automatically dismiss notification when Chrome becomes frontmost.
extern const char
    kContentSuggestionsNotificationsKeepNotificationWhenFrontmostParam[];

// An integer. The maximum number of notifications that will be shown in 1 day.
extern const char kContentSuggestionsNotificationsDailyLimit[];
constexpr int kContentSuggestionsNotificationsDefaultDailyLimit = 1;

// An integer. The number of notifications that can be ignored. If the user
// ignores this many notifications or more, we stop sending them.
extern const char kContentSuggestionsNotificationsIgnoredLimitParam[];
constexpr int kContentSuggestionsNotificationsIgnoredDefaultLimit = 3;

#endif  // CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_
