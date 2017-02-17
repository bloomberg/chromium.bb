// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_
#define COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_

namespace ntp_snippets {
namespace prefs {

// If set to false, remote suggestions are completely disabled. This is set by
// an enterprise policy.
extern const char kEnableSnippets[];

// The pref name under which remote suggestion categories (including their ID
// and title) are stored.
extern const char kRemoteSuggestionCategories[];

// The pref name for the last time when a background fetch was attempted.
extern const char kSnippetLastFetchAttempt[];
// The pref name for the currently applied minimal interval between two
// successive soft background fetches that react to user activity (such as
// opening Chrome).
extern const char kSnippetSoftFetchingIntervalOnUsageEvent[];
// The pref name for the currently applied minimal interval between two
// successive soft brackground fetches when the New Tab Page is opened.
extern const char kSnippetSoftFetchingIntervalOnNtpOpened[];

// The pref name for the currently-scheduled background fetching interval when
// there is WiFi connectivity.
extern const char kSnippetPersistentFetchingIntervalWifi[];
// The pref name for the currently-scheduled background fetching interval when
// there is no WiFi connectivity.
extern const char kSnippetPersistentFetchingIntervalFallback[];

// The pref name for today's count of RemoteSuggestionsFetcher requests, so far.
extern const char kSnippetFetcherRequestCount[];
// The pref name for today's count of RemoteSuggestionsFetcher interactive
// requests.
extern const char kSnippetFetcherInteractiveRequestCount[];
// The pref name for the current day for the counter of RemoteSuggestionsFetcher
// requests.
extern const char kSnippetFetcherRequestsDay[];

// The pref name for today's count of requests for article thumbnails, so far.
extern const char kSnippetThumbnailsRequestCount[];
// The pref name for today's count of interactive requests for article
// thumbnails, so far.
extern const char kSnippetThumbnailsInteractiveRequestCount[];
// The pref name for the current day for the counter of requests for article
// thumbnails.
extern const char kSnippetThumbnailsRequestsDay[];

// The pref name for the time of the last successful background fetch.
extern const char kLastSuccessfulBackgroundFetchTime[];

extern const char kDismissedAssetDownloadSuggestions[];
extern const char kDismissedForeignSessionsSuggestions[];
extern const char kDismissedOfflinePageDownloadSuggestions[];
extern const char kDismissedPhysicalWebPageSuggestions[];
extern const char kDismissedRecentOfflineTabSuggestions[];

extern const char kDismissedCategories[];

// The pref name for the discounted average number of browsing sessions per hour
// that involve opening a new NTP.
extern const char kUserClassifierAverageNTPOpenedPerHour[];
// The pref name for the discounted average number of browsing sessions per hour
// that involve opening showing the content suggestions.
extern const char kUserClassifierAverageSuggestionsShownPerHour[];
// The pref name for the discounted average number of browsing sessions per hour
// that involve using content suggestions (i.e. opening one or clicking on the
// "More" button).
extern const char kUserClassifierAverageSuggestionsUsedPerHour[];

// The pref name for the last time a new NTP was opened.
extern const char kUserClassifierLastTimeToOpenNTP[];
// The pref name for the last time content suggestions were shown to the user.
extern const char kUserClassifierLastTimeToShowSuggestions[];
// The pref name for the last time content suggestions were used by the user.
extern const char kUserClassifierLastTimeToUseSuggestions[];

// The pref name for the current order of categories and their clicks.
extern const char kClickBasedCategoryRankerOrderWithClicks[];
// The pref name for the time when last click decay has happened.
extern const char kClickBasedCategoryRankerLastDecayTime[];

}  // namespace prefs
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_
