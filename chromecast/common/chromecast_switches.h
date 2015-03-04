// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CHROMECAST_SWITCHES_H_
#define CHROMECAST_COMMON_CHROMECAST_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

// Media switches
extern const char kEnableCmaMediaPipeline[];
extern const char kHdmiSinkSupportedCodecs[];

#if defined(OS_ANDROID)
// Content-implementation switches
extern const char kEnableLocalFileAccesses[];
#endif  // defined(OS_ANDROID)

// Metrics switches
extern const char kOverrideMetricsUploadUrl[];

// Network switches
extern const char kNoWifi[];

// Switches to communicate app state information
extern const char kLastLaunchedApp[];
extern const char kPreviousApp[];

}  // namespace switches

#endif  // CHROMECAST_COMMON_CHROMECAST_SWITCHES_H_
