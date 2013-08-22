// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_DEFINES_H_
#define MEDIA_CAST_CAST_DEFINES_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"

namespace media {
namespace cast {

const int64 kDontShowTimeoutMs = 33;
const float kDefaultCongestionControlBackOff = 0.875f;
const uint8 kStartFrameId = 255;
const uint32 kVideoFrequency = 90000;
const int64 kSkippedFramesCheckPeriodkMs = 10000;

// Number of skipped frames threshold in fps (as configured) per period above.
const int kSkippedFramesThreshold = 3;
const size_t kIpPacketSize = 1500;
const int kStartRttMs = 20;
const int64 kCastMessageUpdateIntervalMs = 33;
const int64 kNackRepeatIntervalMs = 30;

enum DefaultSettings {
  kDefaultMaxQp = 56,
  kDefaultMinQp = 4,
  kDefaultMaxFrameRate = 30,
  kDefaultNumberOfVideoBuffers = 1,
  kDefaultRtcpIntervalMs = 500,
  kDefaultRtpHistoryMs = 1000,
  kDefaultRtpMaxDelayMs = 100,
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_DEFINES_H_
