// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_METRICS_H_
#define CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_METRICS_H_

enum ContentSuggestionsNotificationImpression {
  CONTENT_SUGGESTIONS_ARTICLE = 0,  // Server-provided "articles" category.
  CONTENT_SUGGESTIONS_NONARTICLE,   // Anything else.

  MAX_CONTENT_SUGGESTIONS_NOTIFICATION_IMPRESSION
};

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum ContentSuggestionsNotificationAction {
  CONTENT_SUGGESTIONS_TAP = 0,    // User tapped notification to open article.
  CONTENT_SUGGESTIONS_DISMISSAL,  // User swiped notification to dismiss it.

  CONTENT_SUGGESTIONS_HIDE_DEADLINE,   // notification_extra().deadline passed.
  CONTENT_SUGGESTIONS_HIDE_EXPIRY,     // NTP no longer shows notified article.
  CONTENT_SUGGESTIONS_HIDE_FRONTMOST,  // Chrome became the frontmost app.
  CONTENT_SUGGESTIONS_HIDE_DISABLED,   // NTP no longer shows whole category.
  CONTENT_SUGGESTIONS_HIDE_SHUTDOWN,   // Content sugg service is shutting down.

  MAX_CONTENT_SUGGESTIONS_NOTIFICATION_ACTION
};

enum ContentSuggestionsNotificationOptOut {
  CONTENT_SUGGESTIONS_IMPLICIT = 0,  // User ignored notifications.
  CONTENT_SUGGESTIONS_EXPLICIT,      // User explicitly opted-out.

  MAX_CONTENT_SUGGESTIONS_NOTIFICATION_OPT_OUT
};

void RecordContentSuggestionsNotificationImpression(
    ContentSuggestionsNotificationImpression what);
void RecordContentSuggestionsNotificationAction(
    ContentSuggestionsNotificationAction what);
void RecordContentSuggestionsNotificationOptOut(
    ContentSuggestionsNotificationOptOut what);

#endif  // CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_METRICS_H_
