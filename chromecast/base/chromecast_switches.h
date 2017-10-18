// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CHROMECAST_SWITCHES_H_
#define CHROMECAST_BASE_CHROMECAST_SWITCHES_H_

#include <string>

#include "build/build_config.h"

namespace switches {

// Switch values
extern const char kSwitchValueTrue[];
extern const char kSwitchValueFalse[];

// Url to upload crash data to.
extern const char kCrashServerUrl[];

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

// Cast Receiver switches
extern const char kAcceptResourceProvider[];

// ALSA-based CMA switches. (Only valid for audio products.)
extern const char kAlsaOutputBufferSize[];
extern const char kAlsaOutputPeriodSize[];
extern const char kAlsaOutputStartThreshold[];
extern const char kAlsaOutputAvailMin[];
extern const char kAlsaCheckCloseTimeout[];
extern const char kAlsaEnableUpsampling[];
extern const char kAlsaFixedOutputSampleRate[];
extern const char kAlsaVolumeDeviceName[];
extern const char kAlsaVolumeElementName[];
extern const char kAlsaMuteDeviceName[];
extern const char kAlsaMuteElementName[];
extern const char kMaxOutputVolumeDba1m[];
extern const char kAudioOutputChannels[];

// Memory pressure switches
extern const char kMemPressureSystemReservedKb[];

// GPU process switches
extern const char kCastInitialScreenWidth[];
extern const char kCastInitialScreenHeight[];
extern const char kGraphicsBufferCount[];

// I2S loopback configuration switches
extern const char kLoopbackI2sBits[];
extern const char kLoopbackI2sChannels[];
extern const char kLoopbackI2sBusName[];
extern const char kLoopbackI2sRateHz[];

// Graphics switches
extern const char kDesktopWindow1080p[];

// UI switches
extern const char kEnableInput[];

}  // namespace switches

namespace chromecast {

// Gets boolean value from switch |switch_string|.
// --|switch_string| -> true
// --|switch_string|="true" -> true
// --|switch_string|="false" -> false
// no switch named |switch_string| -> |default_value|
bool GetSwitchValueBoolean(const std::string& switch_string,
                           const bool default_value);

// Gets an integer value from switch |switch_name|. If the switch is not present
// in the command line, or the value is not an integer, the |default_value| is
// returned.
int GetSwitchValueInt(const std::string& switch_name, const int default_value);

// Gets a non-negative integer value from switch |switch_name|. If the switch is
// not present in the command line, or the value is not a non-negative integer,
// the |default_value| is returned.
int GetSwitchValueNonNegativeInt(const std::string& switch_name,
                                 const int default_value);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CHROMECAST_SWITCHES_H_
