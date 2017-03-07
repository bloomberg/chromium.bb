// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/features.h"

namespace browser_watcher {

// Enables recording persistent stability information, which can later be
// collected in the event of an unclean shutdown.
const base::Feature kStabilityDebuggingFeature{
    "StabilityDebugging", base::FEATURE_DISABLED_BY_DEFAULT};
// Controls the flushing behavior of the persistent stability information.
const base::Feature kStabilityDebuggingFlushFeature{
    "StabilityDebuggingFlush", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace browser_watcher
