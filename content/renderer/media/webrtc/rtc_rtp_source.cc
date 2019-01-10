// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_source.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/webrtc/rtc_base/scoped_ref_ptr.h"

namespace content {

RTCRtpSource::RTCRtpSource(const webrtc::RtpSource& source) : source_(source) {}

RTCRtpSource::~RTCRtpSource() {}

blink::WebRTCRtpSource::Type RTCRtpSource::SourceType() const {
  switch (source_.source_type()) {
    case webrtc::RtpSourceType::SSRC:
      return blink::WebRTCRtpSource::Type::kSSRC;
    case webrtc::RtpSourceType::CSRC:
      return blink::WebRTCRtpSource::Type::kCSRC;
    default:
      NOTREACHED();
      return blink::WebRTCRtpSource::Type::kSSRC;
  }
}

double RTCRtpSource::TimestampMs() const {
  return source_.timestamp_ms();
}

uint32_t RTCRtpSource::Source() const {
  return source_.source_id();
}

}  // namespace content
