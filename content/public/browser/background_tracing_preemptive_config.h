// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_PREEMPTIVE_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_PREEMPTIVE_CONFIG_H_

#include "base/metrics/histogram_base.h"
#include "content/public/browser/background_tracing_config.h"

namespace content {

// BackgroundTracingPreemptiveConfig holds trigger rules for use during
// preemptive tracing. Tracing will be enabled immediately, and whenever
// a trigger occurs, the trace will be finalized.
struct CONTENT_EXPORT BackgroundTracingPreemptiveConfig
    : public BackgroundTracingConfig {
 public:
  BackgroundTracingPreemptiveConfig();
  ~BackgroundTracingPreemptiveConfig() override;

  enum RuleType {
    MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED,
    MONITOR_AND_DUMP_WHEN_SPECIFIC_HISTOGRAM_AND_VALUE,
    MONITOR_AND_DUMP_WHEN_BROWSER_STARTUP_COMPLETE,
  };
  struct HistogramTriggerInfo {
    std::string histogram_name;
    base::HistogramBase::Sample histogram_value;
  };
  struct NamedTriggerInfo {
    std::string trigger_name;
  };
  struct MonitoringRule {
    RuleType type;
    HistogramTriggerInfo histogram_trigger_info;
    NamedTriggerInfo named_trigger_info;
  };

  std::vector<MonitoringRule> configs;
  CategoryPreset category_preset;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_PREEMPTIVE_CONFIG_H_
