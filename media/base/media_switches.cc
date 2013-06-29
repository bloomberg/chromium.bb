// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_switches.h"

namespace switches {

// Allow users to specify a custom buffer size for debugging purpose.
const char kAudioBufferSize[] = "audio-buffer-size";

// Enable EAC3 playback in MSE.
const char kEnableEac3Playback[] = "enable-eac3-playback";

// Enables support for inband text tracks in media content.
const char kEnableInbandTextTracks[] = "enable-inband-text-tracks";

// Enables Opus playback in media elements.
const char kEnableOpusPlayback[] = "enable-opus-playback";

// Enables VP8 Alpha playback in media elements.
const char kEnableVp8AlphaPlayback[] = "enable-vp8-alpha-playback";

// Set number of threads to use for video decoding.
const char kVideoThreads[] = "video-threads";

// Override suppressed responses to canPlayType().
const char kOverrideEncryptedMediaCanPlayType[] =
    "override-encrypted-media-canplaytype";

#if defined(GOOGLE_TV)
// Use external video surface for video with more than or equal pixels to
// specified value. For example, value of 0 will enable external video surface
// for all videos, and value of 921600 (=1280*720) will enable external video
// surface for 720p video and larger.
const char kUseExternalVideoSurfaceThresholdInPixels[] =
    "use-external-video-surface-threshold-in-pixels";
#endif

#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_SOLARIS)
// The Alsa device to use when opening an audio input stream.
const char kAlsaInputDevice[] = "alsa-input-device";
// The Alsa device to use when opening an audio stream.
const char kAlsaOutputDevice[] = "alsa-output-device";
#endif

#if defined(OS_MACOSX)
// Unlike other platforms, OSX requires CoreAudio calls to happen on the main
// thread of the process.  Provide a way to disable this until support is well
// tested.  See http://crbug.com/158170.
// TODO(dalecurtis): Remove this once we're sure nothing has exploded.
const char kDisableMainThreadAudio[] = "disable-main-thread-audio";
#endif

#if defined(OS_WIN)
// Use exclusive mode audio streaming for Windows Vista and higher.
// Leads to lower latencies for audio streams which uses the
// AudioParameters::AUDIO_PCM_LOW_LATENCY audio path.
// See http://msdn.microsoft.com/en-us/library/windows/desktop/dd370844.aspx
// for details.
const char kEnableExclusiveAudio[] = "enable-exclusive-audio";

// Use Windows WaveOut/In audio API even if Core Audio is supported.
const char kForceWaveAudio[] = "force-wave-audio";

// Number of buffers to use for WaveOut.
const char kWaveOutBuffers[] = "waveout-buffers";
#endif

#if defined(USE_CRAS)
// Use CRAS, the ChromeOS audio server.
const char kUseCras[] = "use-cras";
#endif

}  // namespace switches
