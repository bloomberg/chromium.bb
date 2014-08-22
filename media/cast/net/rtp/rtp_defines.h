// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace media {
namespace cast {

static const uint16 kRtpHeaderLength = 12;
static const uint16 kCastHeaderLength = 7;
static const uint8 kRtpExtensionBitMask = 0x10;
static const uint8 kCastKeyFrameBitMask = 0x80;
static const uint8 kCastReferenceFrameIdBitMask = 0x40;
static const uint8 kRtpMarkerBitMask = 0x80;
static const uint8 kCastExtensionCountmask = 0x3f;

// Cast RTP extensions.
static const uint8 kCastRtpExtensionAdaptiveLatency = 1;

}  // namespace cast
}  // namespace media
