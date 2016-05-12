// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"

class PrefService;

namespace metrics {
class EnabledStateProvider;
class MetricsStateManager;
}

// Provides a //chrome-specific implementation of MetricsServicesManagerClient.
class ChromeMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  explicit ChromeMetricsServicesManagerClient(PrefService* local_state);
  ~ChromeMetricsServicesManagerClient() override;

 private:
  // This is defined as a member class to get access to
  // ChromeMetricsServiceAccessor through ChromeMetricsServicesManagerClient's
  // friendship.
  class ChromeEnabledStateProvider;

  // metrics_services_manager::MetricsServicesManagerClient:
  std::unique_ptr<rappor::RapporService> CreateRapporService() override;
  std::unique_ptr<variations::VariationsService> CreateVariationsService()
      override;
  std::unique_ptr<metrics::MetricsServiceClient> CreateMetricsServiceClient()
      override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  bool IsSafeBrowsingEnabled(const base::Closure& on_update_callback) override;
  bool IsMetricsReportingEnabled() override;
  bool OnlyDoMetricsRecording() override;

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
