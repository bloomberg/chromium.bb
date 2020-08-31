// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_LOOPBACK_INTERRUPT_REASON_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_LOOPBACK_INTERRUPT_REASON_H_

namespace chromecast {
namespace media {

// Reasons for loopback interruption. Should be kept in sync with
// StreamInterruption::InterruptionReason in mixer_service.proto.
enum class LoopbackInterruptReason {
  kUnknown = 0,
  kDisconnected = 1,   // Disconnected from mixer.
  kUnderrun = 2,       // Mixer output underrun.
  kConfigChange = 3,   // Mixer output config changed.
  kOutputStopped = 4,  // Mixer stopped playing out audio.
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_LOOPBACK_INTERRUPT_REASON_H_
