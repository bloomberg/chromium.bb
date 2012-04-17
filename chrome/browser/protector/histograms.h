// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_HISTOGRAMS_H_
#define CHROME_BROWSER_PROTECTOR_HISTOGRAMS_H_
#pragma once

class TemplateURL;

namespace protector {

// Histogram name to report protection errors for the default search
// provider. Values are below.
extern const char kProtectorHistogramDefaultSearchProvider[];

// Histogram name to report protection errors for preferences. Values are below.
extern const char kProtectorHistogramPrefs[];

// Protector histogram values.
enum ProtectorError {
  kProtectorErrorBackupInvalid,
  kProtectorErrorValueChanged,
  kProtectorErrorValueValid,
  kProtectorErrorValueValidZero,
  kProtectorErrorForcedUpdate,

  // This is for convenience only, must always be the last.
  kProtectorErrorCount
};

// Histogram name to report when user accepts new default search provider.
extern const char kProtectorHistogramSearchProviderApplied[];
// Histogram name to report the default search provider when the backup is
// invalid.
extern const char kProtectorHistogramSearchProviderCorrupt[];
// Histogram name to report when user keeps previous default search provider.
extern const char kProtectorHistogramSearchProviderDiscarded[];
// Histogram name to report the fallback default search provider when the
// backup value is invalid or doesn't match an existing provider.
extern const char kProtectorHistogramSearchProviderFallback[];
// Histogram name to report the new default search provider when the backup is
// valid and a change is detected.
extern const char kProtectorHistogramSearchProviderHijacked[];
// Histogram name to report when the prepopulated default search provider was
// missing and has been added for fallback.
extern const char kProtectorHistogramSearchProviderMissing[];
// Histogram name to report the default search provider restored by Protector
// before showing user the bubble.
extern const char kProtectorHistogramSearchProviderRestored[];
// Histogram name to report when user ignores search provider change.
extern const char kProtectorHistogramSearchProviderTimeout[];

// Histogram name to report when user accepts new startup settings.
extern const char kProtectorHistogramStartupSettingsApplied[];
// Histogram name to report the new startup settings when the backup is
// valid and a change is detected.
extern const char kProtectorHistogramStartupSettingsChanged[];
// Histogram name to report when user keeps previous startup settings.
extern const char kProtectorHistogramStartupSettingsDiscarded[];
// Histogram name to report when user ignores startup settings change.
extern const char kProtectorHistogramStartupSettingsTimeout[];

// Histogram name to report when user accepts new homepage.
extern const char kProtectorHistogramHomepageApplied[];
// Histogram name to report the new homepage when the backup is valid and a
// change is detected.
extern const char kProtectorHistogramHomepageChanged[];
// Histogram name to report when user keeps previous homepage.
extern const char kProtectorHistogramHomepageDiscarded[];
// Histogram name to report when user ignores homepage change.
extern const char kProtectorHistogramHomepageTimeout[];

// Maximum value of search provider index in histogram enums.
extern const int kProtectorMaxSearchProviderID;

// Returns index to be used in histograms for given search provider (which may
// be NULL, in which case a special index will be returned).
int GetSearchProviderHistogramID(const TemplateURL* turl);

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_HISTOGRAMS_H_
