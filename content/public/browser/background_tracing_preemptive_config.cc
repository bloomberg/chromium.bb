// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_tracing_preemptive_config.h"

namespace content {

BackgroundTracingPreemptiveConfig::BackgroundTracingPreemptiveConfig()
    : BackgroundTracingConfig(BackgroundTracingConfig::PREEMPTIVE_TRACING_MODE),
      category_preset(BackgroundTracingConfig::BENCHMARK) {
}

BackgroundTracingPreemptiveConfig::~BackgroundTracingPreemptiveConfig() {
}

}  // namespace content
