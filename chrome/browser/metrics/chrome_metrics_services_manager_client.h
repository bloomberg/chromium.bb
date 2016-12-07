// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_

#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"

class PrefService;

namespace metrics {
class EnabledStateProvider;
class MetricsStateManager;
}

namespace version_info {
enum class Channel;
}

// Provides a //chrome-specific implementation of MetricsServicesManagerClient.
class ChromeMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  explicit ChromeMetricsServicesManagerClient(PrefService* local_state);
  ~ChromeMetricsServicesManagerClient() override;

  // Unconditionally attempts to create a field trial to control client side
  // metrics/crash sampling to use as a fallback when one hasn't been
  // provided. This is expected to occur on first-run on platforms that don't
  // have first-run variations support. This should only be called when there is
  // no existing field trial controlling the sampling feature, and on the
  // correct platform. |channel| will affect the sampling rates that are
  // applied. Stable will be sampled at 10%, other channels at 99%.
  static void CreateFallbackSamplingTrial(version_info::Channel channel,
                                          base::FeatureList* feature_list);

  // Determines if this client is eligible to send metrics. If they are, and
  // there was user consent, then metrics and crashes would be reported.
  static bool IsClientInSample();

  // Gets the sample rate for in-sample clients. If the sample rate is not
  // defined, returns false, and |rate| is unchanged, otherwise returns true,
  // and |rate| contains the sample rate. If the client isn't in-sample, the
  // sample rate is undefined. It is also undefined for clients that are not
  // eligible for sampling.
  static bool GetSamplingRatePerMille(int* rate);

 private:
  // This is defined as a member class to get access to
  // ChromeMetricsServiceAccessor through ChromeMetricsServicesManagerClient's
  // friendship.
  class ChromeEnabledStateProvider;

  // metrics_services_manager::MetricsServicesManagerClient:
  std::unique_ptr<rappor::RapporServiceImpl> CreateRapporServiceImpl() override;
  std::unique_ptr<variations::VariationsService> CreateVariationsService()
      override;
  std::unique_ptr<metrics::MetricsServiceClient> CreateMetricsServiceClient()
      override;
  std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateEntropyProvider() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  bool IsSafeBrowsingEnabled(const base::Closure& on_update_callback) override;
  bool IsMetricsReportingEnabled() override;
  bool OnlyDoMetricsRecording() override;

#if defined(OS_WIN)
  // On Windows, the client controls whether Crashpad can upload crash reports.
  void UpdateRunningServices(bool may_record, bool may_upload) override;
#endif  // defined(OS_WIN)

  // Gets the MetricsStateManager, creating it if it has not already been
  // created.
  metrics::MetricsStateManager* GetMetricsStateManager();

  // MetricsStateManager which is passed as a parameter to service constructors.
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  // EnabledStateProvider to communicate if the client has consented to metrics
  // reporting, and if it's enabled.
  std::unique_ptr<metrics::EnabledStateProvider> enabled_state_provider_;

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // Weak pointer to the local state prefs store.
  PrefService* local_state_;

  // Subscription to SafeBrowsing service state changes.
  std::unique_ptr<safe_browsing::SafeBrowsingService::StateSubscription>
      sb_state_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsServicesManagerClient);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
