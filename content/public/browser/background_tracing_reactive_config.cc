// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_tracing_reactive_config.h"

namespace content {

BackgroundTracingReactiveConfig::BackgroundTracingReactiveConfig()
    : BackgroundTracingConfig(BackgroundTracingConfig::REACTIVE_TRACING_MODE) {
}

BackgroundTracingReactiveConfig::~BackgroundTracingReactiveConfig() {
}

}  // namespace content
