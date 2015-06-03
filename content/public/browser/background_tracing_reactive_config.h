// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_REACTIVE_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_REACTIVE_CONFIG_H_

#include "content/public/browser/background_tracing_config.h"

namespace content {

// BackgroundTracingReactiveConfig holds trigger rules for use during
// reactive tracing. Tracing will be not be enabled immediately, rather
// the BackgroundTracingManager will wait for a trigger to occur, and
// enable tracing at that point. Tracing will be finalized later, either
// after some time has elapsed or the trigger occurs again.
struct CONTENT_EXPORT BackgroundTracingReactiveConfig
    : public BackgroundTracingConfig {
 public:
  BackgroundTracingReactiveConfig();
  ~BackgroundTracingReactiveConfig() override;

  enum RuleType { TRACE_FOR_10S_OR_TRIGGER_OR_FULL };
  struct TracingRule {
    RuleType type;
    std::string trigger_name;
    CategoryPreset category_preset;
  };

  std::vector<TracingRule> configs;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_REACTIVE_CONFIG_H_
