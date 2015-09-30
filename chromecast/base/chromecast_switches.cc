// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/chromecast_switches.h"

namespace switches {

// Enable the CMA media pipeline.
const char kEnableCmaMediaPipeline[] = "enable-cma-media-pipeline";

// The bitmask of codecs (media_caps.h) supported by the current HDMI sink.
const char kHdmiSinkSupportedCodecs[] = "hdmi-sink-supported-codecs";

// Enable file accesses. It should not be enabled for most Cast devices.
const char kEnableLocalFileAccesses[] = "enable-local-file-accesses";

// Override the URL to which metrics logs are sent for debugging.
const char kOverrideMetricsUploadUrl[] = "override-metrics-upload-url";

// Disable features that require WiFi management.
const char kNoWifi[] = "no-wifi";

// Allows media playback for hidden WebContents
const char kAllowHiddenMediaPlayback[] = "allow-hidden-media-playback";

// Pass the app id information to the renderer process, to be used for logging.
// last-launched-app should be the app that just launched and is spawning the
// renderer.
const char kLastLaunchedApp[] = "last-launched-app";
// previous-app should be the app that was running when last-launched-app
// started.
const char kPreviousApp[] = "previous-app";

}  // namespace switches
