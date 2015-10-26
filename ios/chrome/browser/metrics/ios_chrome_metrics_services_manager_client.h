// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"

class PrefService;

namespace metrics {
class MetricsStateManager;
}

// Provides an //ios/chrome-specific implementation of
// MetricsServicesManagerClient.
class IOSChromeMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  explicit IOSChromeMetricsServicesManagerClient(PrefService* local_state);
  ~IOSChromeMetricsServicesManagerClient() override;

 private:
  // metrics_services_manager::MetricsServicesManagerClient:
  scoped_ptr<rappor::RapporService> CreateRapporService() override;
  scoped_ptr<variations::VariationsService> CreateVariationsService() override;
  scoped_ptr<metrics::MetricsServiceClient> CreateMetricsServiceClient()
      override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  bool IsSafeBrowsingEnabled(const base::Closure& on_update_callback) override;
  bool IsMetricsReportingEnabled() override;
  bool OnlyDoMetricsRecording() override;

  // Gets the MetricsStateManager, creating it if it has not already been
  // created.
  metrics::MetricsStateManager* GetMetricsStateManager();

  // MetricsStateManager which is passed as a parameter to service constructors.
  scoped_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // Weak pointer to the local state prefs store.
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeMetricsServicesManagerClient);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
