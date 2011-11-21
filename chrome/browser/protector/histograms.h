// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

// Histogram value to report that the backup value is invalid or missing.
extern const char kProtectorBackupInvalidCounter[];
// Histogram value to report that the value does not match the backup.
extern const char kProtectorValueChangedCounter[];
// Histogram value to report that the value matches the backup.
extern const char kProtectorValueValidCounter[];

// Protector histogram values.
enum ProtectorError {
  kProtectorErrorBackupInvalid,
  kProtectorErrorValueChanged,
  kProtectorErrorValueValid,

  // This is for convenience only, must always be the last.
  kProtectorErrorCount
};

// Histogram name to report the new default search provider.
extern const char kProtectorHistogramNewSearchProvider[];
// Histogram name to report when user accepts new default search provider.
extern const char kProtectorHistogramSearchProviderApplied[];
// Histogram name to report when user keeps previous default search provider.
extern const char kProtectorHistogramSearchProviderDiscarded[];
// Histogram name to report when user ignores search provider change.
extern const char kProtectorHistogramSearchProviderTimeout[];

// Returns index to be used in histograms for given search provider (which may
// be NULL, in which case a special index will be returned).
int GetSearchProviderHistogramID(const TemplateURL* t_url);

// Maximum value of search provider index in histogram enums.
extern const int kProtectorMaxSearchProviderID;

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_HISTOGRAMS_H_
