// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_mobile.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/reached_code_profiler.h"
#include "base/base_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#endif

namespace chrome {

void SetupMobileFieldTrials() {
#if defined(OS_ANDROID)
  prerender::ConfigureNoStatePrefetch();

  // For tests on some platforms, g_browser_process is not initialized yet.
  if (g_browser_process) {
    static constexpr char kEnabledGroup[] = "Enabled";
    static constexpr char kDisabledGroup[] = "Disabled";

    static constexpr char kOrderfileOptimizationTrial[] =
        "AndroidOrderfileOptimization";
    if (base::android::IsUsingOrderfileOptimization()) {
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          kOrderfileOptimizationTrial, kEnabledGroup);
    } else {
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          kOrderfileOptimizationTrial, kDisabledGroup);
    }

    static constexpr char kReachedCodeProfilerTrial[] =
        "ReachedCodeProfilerSynthetic";
    if (base::android::IsReachedCodeProfilerEnabled()) {
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          kReachedCodeProfilerTrial, kEnabledGroup);
    } else {
      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          kReachedCodeProfilerTrial, kDisabledGroup);
    }
  }

#endif
}

}  // namespace chrome
