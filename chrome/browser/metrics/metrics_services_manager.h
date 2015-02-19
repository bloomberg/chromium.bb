// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_
#define CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/threading/thread_checker.h"
#include "components/rappor/rappor_service.h"

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

namespace chrome_variations {
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
  chrome_variations::VariationsService* GetVariationsService();

  // Should be called when a plugin loading error occurs.
  void OnPluginLoadingError(const base::FilePath& plugin_path);

  // Update the managed services when permissions for recording/uploading
  // metrics change.
  void UpdatePermissions(bool may_record, bool may_upload);

  // Update the managed services when permissions for uploading metrics change.
  void UpdateUploadPermissions(bool may_upload);

  // Returns true iff Rappor reporting is enabled.
  bool IsRapporEnabled(bool metrics_enabled) const;

  // Returns the recording level for Rappor metrics.
  rappor::RecordingLevel GetRapporRecordingLevel(bool metrics_enabled) const;

 private:
  // Update the managed services when permissions for recording/uploading
  // metrics change.
  void UpdateRapporService();

  // Returns the ChromeMetricsServiceClient, creating it if it hasn't been
  // created yet (and additionally creating the MetricsService in that case).
  ChromeMetricsServiceClient* GetChromeMetricsServiceClient();

  metrics::MetricsStateManager* GetMetricsStateManager();

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // Weak pointer to the local state prefs store.
  PrefService* local_state_;

  // A change registrar for local_state_;
  PrefChangeRegistrar pref_change_registrar_;

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
  scoped_ptr<chrome_variations::VariationsService> variations_service_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServicesManager);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_
