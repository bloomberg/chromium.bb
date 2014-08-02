// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/metrics/metrics_service_accessor.h"

class ChromeBrowserMetricsServiceObserver;
class Profile;

namespace {
class CrashesDOMHandler;
class FlashDOMHandler;
}

namespace extensions {
class ExtensionDownloader;
class ManifestFetchData;
class MetricsPrivateGetIsCrashReportingEnabledFunction;
}

namespace prerender {
bool IsOmniboxEnabled(Profile* profile);
}

namespace system_logs {
class ChromeInternalLogSource;
}

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class ChromeMetricsServiceAccessor : public MetricsServiceAccessor {
 private:
  friend bool prerender::IsOmniboxEnabled(Profile* profile);
  friend class ::ChromeBrowserMetricsServiceObserver;
  friend class ChromeRenderMessageFilter;
  friend class ::CrashesDOMHandler;
  friend class DataReductionProxyChromeSettings;
  friend class extensions::ExtensionDownloader;
  friend class extensions::ManifestFetchData;
  friend class extensions::MetricsPrivateGetIsCrashReportingEnabledFunction;
  friend class ::FlashDOMHandler;
  friend class system_logs::ChromeInternalLogSource;

  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServiceAccessorTest,
                           MetricsReportingEnabled);
  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServiceAccessorTest,
                           CrashReportingEnabled);

  // Returns true if prefs::kMetricsReportingEnabled is set.
  // TODO(asvitkine): Consolidate the method in MetricsStateManager.
  // TODO(asvitkine): This function does not report the correct value on
  // Android and ChromeOS, see http://crbug.com/362192.
  static bool IsMetricsReportingEnabled();

  // Returns true if crash reporting is enabled.  This is set at the platform
  // level for Android and ChromeOS, and otherwise is the same as
  // IsMetricsReportingEnabled for desktop Chrome.
  static bool IsCrashReportingEnabled();

  // Registers a field trial name and group to be used to annotate a UMA report
  // with a particular Chrome configuration state. A UMA report will be
  // annotated with this trial group if and only if all events in the report
  // were created after the trial is registered. Only one group name may be
  // registered at a time for a given trial name. Only the last group name that
  // is registered for a given trial name will be recorded. The values passed
  // in must not correspond to any real field trial in the code.
  static bool RegisterSyntheticFieldTrial(const std::string& trial,
                                          const std::string& group);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromeMetricsServiceAccessor);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
