// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_DEFINES_H_
#define MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_DEFINES_H_

#include "base/basictypes.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "third_party/webrtc/modules/interface/module_common_types.h"

namespace media {
namespace cast {

struct RtpCastHeader {
  RtpCastHeader() {
    is_key_frame = false;
    frame_id = 0;
    packet_id = 0;
    max_packet_id = 0;
    is_reference = false;
    reference_frame_id = 0;
  }
  webrtc::WebRtcRTPHeader webrtc;
  bool is_key_frame;
  uint32 frame_id;
  uint16 packet_id;
  uint16 max_packet_id;
  bool is_reference;  // Set to true if the previous frame is not available,
                      // and the reference frame id  is available.
  uint32 reference_frame_id;
};

class RtpPayloadFeedback {
 public:
  virtual void CastFeedback(const RtcpCastMessage& cast_feedback) = 0;

 protected:
  virtual ~RtpPayloadFeedback() {}
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_DEFINES_H_
