// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_COMMON_RTP_DEFINES_H_
#define MEDIA_CAST_RTP_COMMON_RTP_DEFINES_H_

#include <cstring>

#include "base/basictypes.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp_defines.h"
#include "third_party/webrtc/modules/interface/module_common_types.h"

namespace media {
namespace cast {

const uint8 kRtpMarkerBitMask = 0x80;

struct RtpCastHeader {
  void InitRTPVideoHeaderCast() {
    is_key_frame = false;
    frame_id = 0;
    packet_id = 0;
    max_packet_id = 0;
    is_reference = false;
    reference_frame_id = 0;
  }
  webrtc::WebRtcRTPHeader webrtc;
  bool is_key_frame;
  uint8 frame_id;
  uint16 packet_id;
  uint16 max_packet_id;
  bool is_reference;  // Set to true if the previous frame is not available,
                      // and the reference frame id  is available.
  uint8 reference_frame_id;
};

class RtpPayloadFeedback {
 public:
  virtual void CastFeedback(const RtcpCastMessage& cast_feedback) = 0;
  virtual void RequestKeyFrame() = 0;  // TODO(pwestin): can we remove this?

 protected:
  virtual ~RtpPayloadFeedback() {}
};

inline bool IsNewerFrameId(uint8 frame_id, uint8 prev_frame_id) {
  return (frame_id != prev_frame_id) &&
      static_cast<uint8>(frame_id - prev_frame_id) < 0x80;
}

inline bool IsOlderFrameId(uint8 frame_id, uint8 prev_frame_id) {
  return (frame_id == prev_frame_id) || IsNewerFrameId(prev_frame_id, frame_id);
}

inline bool IsNewerPacketId(uint16 packet_id, uint16 prev_packet_id) {
  return (packet_id != prev_packet_id) &&
      static_cast<uint16>(packet_id - prev_packet_id) < 0x8000;
}

inline bool IsNewerSequenceNumber(uint16 sequence_number,
                                  uint16 prev_sequence_number) {
  // Same function as IsNewerPacketId just different data and name.
  return IsNewerPacketId(sequence_number, prev_sequence_number);
}

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_COMMON_RTP_DEFINES_H_
