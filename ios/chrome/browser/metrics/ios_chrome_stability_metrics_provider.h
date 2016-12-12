// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_CHROME_STABILITY_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_CHROME_STABILITY_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "ios/web/public/web_state/global_web_state_observer.h"

class PrefService;

// IOSChromeStabilityMetricsProvider gathers and logs Chrome-specific stability-
// related metrics.
class IOSChromeStabilityMetricsProvider : public metrics::MetricsProvider,
                                          public web::GlobalWebStateObserver {
 public:
  explicit IOSChromeStabilityMetricsProvider(PrefService* local_state);
  ~IOSChromeStabilityMetricsProvider() override;

  // metrics::MetricsDataProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ClearSavedStabilityMetrics() override;

  // web::GlobalWebStateObserver:
  void WebStateDidStartLoading(web::WebState* web_state) override;

  // Records a renderer process crash.
  void LogRendererCrash();

 private:
  metrics::StabilityMetricsHelper helper_;

  // True if recording is currently enabled.
  bool recording_enabled_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeStabilityMetricsProvider);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_CHROME_STABILITY_METRICS_PROVIDER_H_
