// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "components/metrics/metrics_service_accessor.h"

class BrowserProcessImpl;
class Profile;
class ChromeMetricsServiceClient;
class ChromePasswordManagerClient;

namespace {
class CrashesDOMHandler;
class FlashDOMHandler;
}

namespace arc {
class ArcOptInPreferenceHandler;
}

namespace chrome {
void AttemptRestart();
namespace android {
class ExternalDataUseObserverBridge;
}
}

namespace domain_reliability {
class DomainReliabilityServiceFactory;
}

namespace extensions {
class ChromeExtensionWebContentsObserver;
class ChromeMetricsPrivateDelegate;
class FileManagerPrivateIsUMAEnabledFunction;
}

namespace options {
class BrowserOptionsHandler;
}

namespace prerender {
bool IsOmniboxEnabled(Profile* profile);
}

namespace safe_browsing {
class DownloadSBClient;
class IncidentReportingService;
class ReporterRunner;
class SafeBrowsingService;
class SafeBrowsingUIManager;
class SRTFetcher;
class SRTGlobalError;
}

namespace settings {
class MetricsReportingHandler;
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
 public:
  // This test method is public so tests don't need to befriend this class.

  // If arg is non-null, the value will be returned from future calls to
  // IsMetricsAndCrashReportingEnabled().  Pointer must be valid until
  // it is reset to null here.
  static void SetMetricsAndCrashReportingForTesting(const bool* value);

 private:
  friend class ::CrashesDOMHandler;
  friend class ::FlashDOMHandler;
  friend class arc::ArcOptInPreferenceHandler;
  friend class BrowserProcessImpl;
  friend void chrome::AttemptRestart();
  friend class chrome::android::ExternalDataUseObserverBridge;
  // For StackSamplingConfiguration.
  friend class ChromeBrowserMainParts;
  friend class ChromeMetricsServicesManagerClient;
  friend class ChromeRenderMessageFilter;
  friend class DataReductionProxyChromeSettings;
  friend class domain_reliability::DomainReliabilityServiceFactory;
  friend class extensions::ChromeExtensionWebContentsObserver;
  friend class extensions::ChromeMetricsPrivateDelegate;
  friend class extensions::FileManagerPrivateIsUMAEnabledFunction;
  friend void ChangeMetricsReportingStateWithReply(
      bool,
      const OnMetricsReportingCallbackType&);
  friend class options::BrowserOptionsHandler;
  friend bool prerender::IsOmniboxEnabled(Profile* profile);
  friend class settings::MetricsReportingHandler;
  friend class speech::ChromeSpeechRecognitionManagerDelegate;
  friend class system_logs::ChromeInternalLogSource;
  friend class UmaSessionStats;
  friend class safe_browsing::DownloadSBClient;
  friend class safe_browsing::IncidentReportingService;
  friend class safe_browsing::ReporterRunner;
  friend class safe_browsing::SRTFetcher;
  friend class safe_browsing::SRTGlobalError;
  friend class safe_browsing::SafeBrowsingService;
  friend class safe_browsing::SafeBrowsingUIManager;
  friend void SyzyASANRegisterExperiment(const char*, const char*);
  friend class ChromeMetricsServiceClient;
  friend class ChromePasswordManagerClient;

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

  // Calls MetricsServiceAccessor::RegisterSyntheticMultiGroupFieldTrial() with
  // g_browser_process->metrics_service(). See that function's declaration for
  // details.
  static bool RegisterSyntheticMultiGroupFieldTrial(
      const std::string& trial_name,
      const std::vector<uint32_t>& group_name_hashes);

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
