// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "components/metrics/metrics_service_accessor.h"

class BrowserProcessImpl;
class ChromeExtensionDownloaderFactory;
class Profile;

namespace {
class CrashesDOMHandler;
class FlashDOMHandler;
}

namespace component_updater {
class ComponentUpdateService;
}

namespace domain_reliability {
class DomainReliabilityServiceFactory;
}

namespace extensions {
class FileManagerPrivateIsUMAEnabledFunction;
class MetricsPrivateGetIsCrashReportingEnabledFunction;
}

namespace options {
class BrowserOptionsHandler;
}

namespace prerender {
bool IsOmniboxEnabled(Profile* profile);
}

namespace safe_browsing {
class IncidentReportingService;
class SRTFetcher;
}

namespace speech {
class ChromeSpeechRecognitionManagerDelegate;
}

namespace system_logs {
class ChromeInternalLogSource;
}

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class ChromeMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 private:
  friend class ::CrashesDOMHandler;
  friend class ::FlashDOMHandler;
  friend class BrowserProcessImpl;
  friend class ChromeExtensionDownloaderFactory;
  friend class ChromeMetricsServicesManagerClient;
  friend class ChromeRenderMessageFilter;
  friend class DataReductionProxyChromeSettings;
  friend class domain_reliability::DomainReliabilityServiceFactory;
  friend class extensions::MetricsPrivateGetIsCrashReportingEnabledFunction;
  friend class extensions::FileManagerPrivateIsUMAEnabledFunction;
  friend void InitiateMetricsReportingChange(
      bool, const OnMetricsReportingCallbackType&);
  friend class options::BrowserOptionsHandler;
  friend bool prerender::IsOmniboxEnabled(Profile* profile);
  friend class safe_browsing::IncidentReportingService;
  friend class speech::ChromeSpeechRecognitionManagerDelegate;
  friend class StackSamplingConfiguration;
  friend class system_logs::ChromeInternalLogSource;
  friend class UmaSessionStats;
  friend class safe_browsing::SRTFetcher;

  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServiceAccessorTest,
                           MetricsReportingEnabled);

  // Returns true if metrics reporting is enabled.
  // TODO(gayane): Consolidate metric prefs on all platforms.
  // http://crbug.com/362192,  http://crbug.com/532084
  static bool IsMetricsAndCrashReportingEnabled();

  // Calls metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial() with
  // g_browser_process->metrics_service(). See that function's declaration for
  // details.
  static bool RegisterSyntheticFieldTrial(const std::string& trial_name,
                                          const std::string& group_name);

  // Calls
  // metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash()
  // with g_browser_process->metrics_service(). See that function's declaration
  // for details.
  static bool RegisterSyntheticFieldTrialWithNameHash(
      uint32_t trial_name_hash,
      const std::string& group_name);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromeMetricsServiceAccessor);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
