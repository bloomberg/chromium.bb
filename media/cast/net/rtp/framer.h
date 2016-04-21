// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTP_FRAMER_H_
#define MEDIA_CAST_NET_RTP_FRAMER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/net/rtp/cast_message_builder.h"
#include "media/cast/net/rtp/frame_buffer.h"
#include "media/cast/net/rtp/rtp_defines.h"

namespace media {
namespace cast {

typedef std::map<uint32_t, linked_ptr<FrameBuffer>> FrameList;

class Framer {
 public:
  Framer(base::TickClock* clock,
         RtpPayloadFeedback* incoming_payload_feedback,
         uint32_t ssrc,
         bool decoder_faster_than_max_frame_rate,
         int max_unacked_frames);
  ~Framer();

  // Return true when receiving the last packet in a frame, creating a
  // complete frame. If a duplicate packet for an already complete frame is
  // received, the function returns false but sets |duplicate| to true.
  bool InsertPacket(const uint8_t* payload_data,
                    size_t payload_size,
                    const RtpCastHeader& rtp_header,
                    bool* duplicate);

  // Extracts a complete encoded frame - will only return a complete and
  // decodable frame. Returns false if no such frames exist.
  // |next_frame| will be set to true if the returned frame is the very
  // next frame. |have_multiple_complete_frames| will be set to true
  // if there are more decodadble frames available.
  bool GetEncodedFrame(EncodedFrame* video_frame,
                       bool* next_frame,
                       bool* have_multiple_complete_frames);

  // TODO(hubbe): Move this elsewhere.
  void AckFrame(uint32_t frame_id);

  void ReleaseFrame(uint32_t frame_id);

  // Reset framer state to original state and flush all pending buffers.
  void Reset();
  bool TimeToSendNextCastMessage(base::TimeTicks* time_to_send);
  void SendCastMessage();

  bool Empty() const;
  bool FrameExists(uint32_t frame_id) const;
  uint32_t NewestFrameId() const;

  void RemoveOldFrames(uint32_t frame_id);

  // Identifies the next frame to be released (rendered).
  bool NextContinuousFrame(uint32_t* frame_id) const;
  uint32_t LastContinuousFrame() const;

  bool NextFrameAllowingSkippingFrames(uint32_t* frame_id) const;
  bool HaveMultipleDecodableFrames() const;

  int NumberOfCompleteFrames() const;
  void GetMissingPackets(uint32_t frame_id,
                         bool last_frame,
                         PacketIdSet* missing_packets) const;

 private:
  bool ContinuousFrame(FrameBuffer* frame) const;
  bool DecodableFrame(FrameBuffer* frame) const;

  const bool decoder_faster_than_max_frame_rate_;
  FrameList frames_;
  std::unique_ptr<CastMessageBuilder> cast_msg_builder_;
  bool waiting_for_key_;
  uint32_t last_released_frame_;
  uint32_t newest_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(Framer);
};

}  //  namespace cast
}  //  namespace media

#endif  // MEDIA_CAST_NET_RTP_FRAMER_H_
