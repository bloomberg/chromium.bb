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

// Size of the ALSA output buffer in frames. This directly sets the latency of
// the output device. Latency can be calculated by multiplying the sample rate
// by the output buffer size.
const char kAlsaOutputBufferSize[] = "alsa-output-buffer-size";

// Size of the ALSA output period in frames. The period of an ALSA output device
// determines how many frames elapse between hardware interrupts.
const char kAlsaOutputPeriodSize[] = "alsa-output-period-size";

// How many frames need to be in the output buffer before output starts.
const char kAlsaOutputStartThreshold[] = "alsa-output-start-threshold";

// Minimum number of available frames for scheduling a transfer.
const char kAlsaOutputAvailMin[] = "alsa-output-avail-min";

// Time in ms to wait before closing the PCM handle when no more mixer inputs
// remain.
const char kAlsaCheckCloseTimeout[] = "alsa-check-close-timeout";

// Number of channels on the alsa output device that the stream mixer uses.
// Default is 2 channels.
const char kAlsaNumOutputChannels[] = "alsa-num-output-channels";

}  // namespace switches
