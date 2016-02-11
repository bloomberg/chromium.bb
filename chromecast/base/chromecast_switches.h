// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CHROMECAST_SWITCHES_H_
#define CHROMECAST_BASE_CHROMECAST_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

// Media switches
extern const char kEnableCmaMediaPipeline[];
extern const char kHdmiSinkSupportedCodecs[];

// Content-implementation switches
extern const char kEnableLocalFileAccesses[];

// Metrics switches
extern const char kOverrideMetricsUploadUrl[];

// Network switches
extern const char kNoWifi[];

// App switches
extern const char kAllowHiddenMediaPlayback[];

// Switches to communicate app state information
extern const char kLastLaunchedApp[];
extern const char kPreviousApp[];

// ALSA-based CMA switches. (Only valid for audio products.)
extern const char kAlsaOutputBufferSize[];
extern const char kAlsaOutputPeriodSize[];
extern const char kAlsaOutputStartThreshold[];
extern const char kAlsaOutputAvailMin[];
extern const char kAlsaCheckCloseTimeout[];
extern const char kAlsaNumOutputChannels[];

}  // namespace switches

#endif  // CHROMECAST_BASE_CHROMECAST_SWITCHES_H_
