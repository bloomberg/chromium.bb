// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_

#include "base/basictypes.h"
#include "chrome/browser/android/activity_type_ids.h"
#include "components/metrics/metrics_provider.h"

class PrefService;
class PrefRegistrySimple;

// AndroidMetricsProvider provides Android-specific stability metrics.
class AndroidMetricsProvider : public metrics::MetricsProvider {
 public:
  // Creates the AndroidMetricsProvider with the given |local_state|.
  explicit AndroidMetricsProvider(PrefService* local_state);
  virtual ~AndroidMetricsProvider();

  // Called when the Activity that the user interacts with is swapped out.
  // TODO(asvitkine): Expose a way for Android code to actually invoke this.
  void OnForegroundActivityChanged(ActivityTypeIds::Type type);

  // metrics::MetricsDataProvider:
  virtual void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) OVERRIDE;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Called to log launch and crash stats to preferences.
  void LogStabilityToPrefs();

  // Converts crash stats stored in the preferences into histograms.
  void ConvertStabilityPrefsToHistograms();

  // Weak pointer to the local state prefs store.
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(AndroidMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_
