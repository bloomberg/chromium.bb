// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_
#define COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_

namespace ntp_snippets {
namespace prefs {

extern const char kEnableSnippets[];

extern const char kSnippetHosts[];

// The pref name for today's count of NTPSnippetsFetcher requests, so far.
extern const char kSnippetFetcherQuotaCount[];
// The pref name for the current day for the counter of NTPSnippetsFetcher
// requests.
extern const char kSnippetFetcherQuotaDay[];

}  // namespace prefs
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_
