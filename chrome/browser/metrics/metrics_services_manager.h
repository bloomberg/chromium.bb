// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_
#define CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"

class MetricsService;
class PrefService;

namespace metrics {
class MetricsStateManager;
}

namespace rappor {
class RapporService;
}

namespace chrome_variations {
class VariationsService;
}

// MetricsServicesManager is a helper class that has ownership over the various
// metrics-related services in Chrome: MetricsService, RapporService and
// VariationsService.
class MetricsServicesManager {
 public:
  // Creates the MetricsServicesManager with the |local_state| prefs service.
  explicit MetricsServicesManager(PrefService* local_state);
  virtual ~MetricsServicesManager();

  // Returns the MetricsService, creating it if it hasn't been created yet.
  MetricsService* GetMetricsService();

  // Returns the GetRapporService, creating it if it hasn't been created yet.
  rappor::RapporService* GetRapporService();

  // Returns the VariationsService, creating it if it hasn't been created yet.
  chrome_variations::VariationsService* GetVariationsService();

 private:
  metrics::MetricsStateManager* GetMetricsStateManager();

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // Weak pointer to the local state prefs store.
  PrefService* local_state_;

  // MetricsStateManager which is passed as a parameter to service constructors.
  scoped_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  // The MetricsService, used for UMA report uploads.
  scoped_ptr<MetricsService> metrics_service_;

  // The RapporService, for RAPPOR metric uploads.
  scoped_ptr<rappor::RapporService> rappor_service_;

  // The VariationsService, for server-side experiments infrastructure.
  scoped_ptr<chrome_variations::VariationsService> variations_service_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServicesManager);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICES_MANAGER_H_
