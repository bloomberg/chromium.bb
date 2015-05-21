// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_tracing_config.h"

namespace content {

BackgroundTracingConfig::BackgroundTracingConfig(Mode mode) : mode(mode) {
}

BackgroundTracingConfig::~BackgroundTracingConfig() {
}

}  // namespace content
