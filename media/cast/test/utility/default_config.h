// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_DEFAULT_CONFIG_H_
#define MEDIA_CAST_TEST_UTILITY_DEFAULT_CONFIG_H_

#include "media/cast/cast_config.h"

namespace media {
namespace cast {

// Returns an AudioReceiverConfig initialized to "good-to-go" values.  This
// specifies 48 kHz, 2-channel Opus-coded audio, with standard ssrc's, payload
// type, and a dummy name.
AudioReceiverConfig GetDefaultAudioReceiverConfig();

// Returns a VideoReceiverConfig initialized to "good-to-go" values.  This
// specifies VP8-coded video, with standard ssrc's, payload type, and a dummy
// name.
VideoReceiverConfig GetDefaultVideoReceiverConfig();

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_DEFAULT_CONFIG_H_
