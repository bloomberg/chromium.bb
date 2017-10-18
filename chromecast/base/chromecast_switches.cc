// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/chromecast_switches.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"

namespace switches {

// Value indicating whether flag from command line switch is true.
const char kSwitchValueTrue[] = "true";

// Value indicating whether flag from command line switch is false.
const char kSwitchValueFalse[] = "false";

// Server url to upload crash data to.
// Default is "http://clients2.google.com/cr/report" for prod devices.
// Default is "http://clients2.google.com/cr/staging_report" for non prod.
const char kCrashServerUrl[] = "crash-server-url";

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

// Flag indicating that a resource provider must be set up to provide cast
// receiver with resources. Apps cannot start until provided resources.
// This flag implies --alsa-check-close-timeout=0.
const char kAcceptResourceProvider[] = "accept-resource-provider";

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
// remain. Assumed to be 0 if --accept-resource-provider is present.
const char kAlsaCheckCloseTimeout[] = "alsa-check-close-timeout";

// Flag that enables resampling audio with sample rate below 32kHz up to 48kHz.
// Should be set to true for internal audio products.
const char kAlsaEnableUpsampling[] = "alsa-enable-upsampling";

// Optional flag to set a fixed sample rate for the alsa device.
const char kAlsaFixedOutputSampleRate[] = "alsa-fixed-output-sample-rate";

// Name of the simple mixer control element that the ALSA-based media library
// should use to control the volume.
const char kAlsaVolumeElementName[] = "alsa-volume-element-name";

// Name of the device the volume control mixer should be opened on. Will use the
// same device as kAlsaOutputDevice and fall back to "default" if
// kAlsaOutputDevice is not supplied.
const char kAlsaVolumeDeviceName[] = "alsa-volume-device-name";

// Name of the simple mixer control element that the ALSA-based media library
// should use to mute the system.
const char kAlsaMuteElementName[] = "alsa-mute-element-name";

// Name of the device the mute mixer should be opened on. If this flag is not
// specified it will default to the same device as kAlsaVolumeDeviceName.
const char kAlsaMuteDeviceName[] = "alsa-mute-device-name";

// Calibrated max output volume dBa for voice content at 1 meter, if known.
const char kMaxOutputVolumeDba1m[] = "max-output-volume-dba1m";

// Number of audio output channels. This will be used to send audio buffer with
// specific number of channels to ALSA and generate loopback audio. Default
// value is 2.
const char kAudioOutputChannels[] = "audio-output-channels";

// Some platforms typically have very little 'free' memory, but plenty is
// available in buffers+cached.  For such platforms, configure this amount
// as the portion of buffers+cached memory that should be treated as
// unavailable.  If this switch is not used, a simple pressure heuristic based
// purely on free memory will be used.
const char kMemPressureSystemReservedKb[] = "mem-pressure-system-reserved-kb";

// Used to pass initial screen resolution to GPU process.  This allows us to set
// screen size correctly (so no need to resize when first window is created).
const char kCastInitialScreenWidth[] = "cast-initial-screen-width";
const char kCastInitialScreenHeight[] = "cast-initial-screen-height";
const char kGraphicsBufferCount[] = "graphics-buffer-count";

// Used to pass configuration for the I2S input to enable loopback for AEC.
const char kLoopbackI2sBits[] = "loopback-i2s-bits";
const char kLoopbackI2sChannels[] = "loopback-i2s-channels";
const char kLoopbackI2sBusName[] = "loopback-i2s-bus-name";
const char kLoopbackI2sRateHz[] = "loopback-i2s-rate-hz";

// When present, desktop cast_shell will create 1080p window (provided display
// resolution is high enough).  Otherwise, cast_shell defaults to 720p.
const char kDesktopWindow1080p[] = "desktop-window-1080p";

// Enables input event handling by the window manager.
const char kEnableInput[] = "enable-input";

}  // namespace switches

namespace chromecast {

bool GetSwitchValueBoolean(const std::string& switch_string,
                           const bool default_value) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_string)) {
    if (command_line->GetSwitchValueASCII(switch_string) !=
            switches::kSwitchValueTrue &&
        command_line->GetSwitchValueASCII(switch_string) !=
            switches::kSwitchValueFalse &&
        command_line->GetSwitchValueASCII(switch_string) != "") {
      LOG(WARNING) << "Invalid switch value " << switch_string << "="
                   << command_line->GetSwitchValueASCII(switch_string)
                   << "; assuming default value of " << default_value;
      return default_value;
    }
    return command_line->GetSwitchValueASCII(switch_string) !=
           switches::kSwitchValueFalse;
  }
  return default_value;
}

int GetSwitchValueInt(const std::string& switch_name, const int default_value) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name)) {
    return default_value;
  }

  int arg_value;
  if (!base::StringToInt(command_line->GetSwitchValueASCII(switch_name),
                         &arg_value)) {
    LOG(DFATAL) << "--" << switch_name << " only accepts integers as arguments";
    return default_value;
  }
  return arg_value;
}

int GetSwitchValueNonNegativeInt(const std::string& switch_name,
                                 const int default_value) {
  DCHECK_GE(default_value, 0)
      << "--" << switch_name << " must have a non-negative default value";

  int value = GetSwitchValueInt(switch_name, default_value);
  if (value < 0) {
    LOG(DFATAL) << "--" << switch_name << " must have a non-negative value";
    return default_value;
  }
  return value;
}

}  // namespace chromecast
