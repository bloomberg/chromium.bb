// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_
#define CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"

class ChromeMetricsServiceClient;
class PrefService;

namespace base {
class FilePath;
}

namespace metrics {
class MetricsService;
class MetricsStateManager;
}

namespace rappor {
class RapporService;
}

namespace variations {
class VariationsService;
}

// MetricsServicesManager is a helper class that has ownership over the various
// metrics-related services in Chrome: MetricsService (via its client),
// RapporService and VariationsService.
class MetricsServicesManager {
 public:
  // Creates the MetricsServicesManager with the |local_state| prefs service.
  explicit MetricsServicesManager(PrefService* local_state);
  virtual ~MetricsServicesManager();

  // Returns the MetricsService, creating it if it hasn't been created yet (and
  // additionally creating the ChromeMetricsServiceClient in that case).
  metrics::MetricsService* GetMetricsService();

  // Returns the GetRapporService, creating it if it hasn't been created yet.
  rappor::RapporService* GetRapporService();

  // Returns the VariationsService, creating it if it hasn't been created yet.
  variations::VariationsService* GetVariationsService();

  // Should be called when a plugin loading error occurs.
  void OnPluginLoadingError(const base::FilePath& plugin_path);

  // Update the managed services when permissions for recording/uploading
  // metrics change.
  void UpdatePermissions(bool may_record, bool may_upload);

  // Update the managed services when permissions for uploading metrics change.
  void UpdateUploadPermissions(bool may_upload);

 private:
  // Update the managed services when permissions for recording/uploading
  // metrics change.
  void UpdateRapporService();

  // Returns the ChromeMetricsServiceClient, creating it if it hasn't been
  // created yet (and additionally creating the MetricsService in that case).
  ChromeMetricsServiceClient* GetChromeMetricsServiceClient();

  metrics::MetricsStateManager* GetMetricsStateManager();

  // Retrieve the latest SafeBrowsing preferences state.
  bool GetSafeBrowsingState();

  // Update which services are running to match current permissions.
  void UpdateRunningServices();

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // Weak pointer to the local state prefs store.
  PrefService* local_state_;

  // Subscription to SafeBrowsing service state changes.
  scoped_ptr<SafeBrowsingService::StateSubscription> sb_state_subscription_;

  // The current metrics reporting setting.
  bool may_upload_;

  // The current metrics recording setting.
  bool may_record_;

  // MetricsStateManager which is passed as a parameter to service constructors.
  scoped_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  // Chrome embedder implementation of the MetricsServiceClient. Owns the
  // MetricsService.
  scoped_ptr<ChromeMetricsServiceClient> metrics_service_client_;

  // The RapporService, for RAPPOR metric uploads.
  scoped_ptr<rappor::RapporService> rappor_service_;

  // The VariationsService, for server-side experiments infrastructure.
  scoped_ptr<variations::VariationsService> variations_service_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServicesManager);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_
