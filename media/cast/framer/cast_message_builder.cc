// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/framer/cast_message_builder.h"

#include "media/cast/cast_defines.h"

namespace media {
namespace cast {

CastMessageBuilder::CastMessageBuilder(
    base::TickClock* clock,
    RtpPayloadFeedback* incoming_payload_feedback,
    FrameIdMap* frame_id_map,
    uint32 media_ssrc,
    bool decoder_faster_than_max_frame_rate,
    int max_unacked_frames)
    : clock_(clock),
      cast_feedback_(incoming_payload_feedback),
      frame_id_map_(frame_id_map),
      media_ssrc_(media_ssrc),
      decoder_faster_than_max_frame_rate_(decoder_faster_than_max_frame_rate),
      max_unacked_frames_(max_unacked_frames),
      cast_msg_(media_ssrc),
      waiting_for_key_frame_(true),
      slowing_down_ack_(false),
      acked_last_frame_(true),
      last_acked_frame_id_(kStartFrameId) {
  cast_msg_.ack_frame_id_ = kStartFrameId;
}

CastMessageBuilder::~CastMessageBuilder() {}

void CastMessageBuilder::CompleteFrameReceived(uint32 frame_id,
                                               bool is_key_frame) {
  if (last_update_time_.is_null()) {
    // Our first update.
    last_update_time_ = clock_->NowTicks();
  }
  if (waiting_for_key_frame_) {
    if (!is_key_frame) {
      // Ignore that we have received this complete frame since we are
      // waiting on a key frame.
      return;
    }
    waiting_for_key_frame_ = false;
    cast_msg_.missing_frames_and_packets_.clear();
    cast_msg_.ack_frame_id_ = frame_id;
    last_update_time_ = clock_->NowTicks();
    // We might have other complete frames waiting after we receive the last
    // packet in the key-frame.
    UpdateAckMessage();
  } else {
    if (!UpdateAckMessage()) return;

    BuildPacketList();
  }
  // Send cast message.
  VLOG(1) << "Send cast message Ack:" << static_cast<int>(frame_id);
  cast_feedback_->CastFeedback(cast_msg_);
}

bool CastMessageBuilder::UpdateAckMessage() {
  if (!decoder_faster_than_max_frame_rate_) {
    int complete_frame_count = frame_id_map_->NumberOfCompleteFrames();
    if (complete_frame_count > max_unacked_frames_) {
      // We have too many frames pending in our framer; slow down ACK.
      slowing_down_ack_ = true;
    } else if (complete_frame_count <= 1) {
      // We are down to one or less frames in our framer; ACK normally.
      slowing_down_ack_ = false;
    }
  }
  if (slowing_down_ack_) {
    // We are slowing down acknowledgment by acknowledging every other frame.
    if (acked_last_frame_) {
      acked_last_frame_ = false;
    } else {
      acked_last_frame_ = true;
      last_acked_frame_id_++;
      // Note: frame skipping and slowdown ACK is not supported at the same
      // time; and it's not needed since we can skip frames to catch up.
    }
  } else {
    uint32 frame_id = frame_id_map_->LastContinuousFrame();

    // Is it a new frame?
    if (last_acked_frame_id_ == frame_id) return false;

    last_acked_frame_id_ = frame_id;
    acked_last_frame_ = true;
  }
  cast_msg_.ack_frame_id_ = last_acked_frame_id_;
  cast_msg_.missing_frames_and_packets_.clear();
  last_update_time_ = clock_->NowTicks();
  return true;
}

bool CastMessageBuilder::TimeToSendNextCastMessage(
    base::TimeTicks* time_to_send) {
  // We haven't received any packets.
  if (last_update_time_.is_null() && frame_id_map_->Empty()) return false;

  *time_to_send = last_update_time_ +
      base::TimeDelta::FromMilliseconds(kCastMessageUpdateIntervalMs);
  return true;
}

void CastMessageBuilder::UpdateCastMessage() {
  RtcpCastMessage message(media_ssrc_);
  if (!UpdateCastMessageInternal(&message)) return;

  // Send cast message.
  cast_feedback_->CastFeedback(message);
}

void CastMessageBuilder::Reset() {
  waiting_for_key_frame_ = true;
  cast_msg_.ack_frame_id_ = kStartFrameId;
  cast_msg_.missing_frames_and_packets_.clear();
  time_last_nacked_map_.clear();
}

bool CastMessageBuilder::UpdateCastMessageInternal(RtcpCastMessage* message) {
  if (last_update_time_.is_null()) {
    if (!frame_id_map_->Empty()) {
      // We have received packets.
      last_update_time_ = clock_->NowTicks();
    }
    return false;
  }
  // Is it time to update the cast message?
  base::TimeTicks now = clock_->NowTicks();
  if (now - last_update_time_ <
      base::TimeDelta::FromMilliseconds(kCastMessageUpdateIntervalMs)) {
    return false;
  }
  last_update_time_ = now;

  UpdateAckMessage();  // Needed to cover when a frame is skipped.
  BuildPacketList();
  *message = cast_msg_;
  return true;
}

void CastMessageBuilder::BuildPacketList() {
  base::TimeTicks now = clock_->NowTicks();

  // Clear message NACK list.
  cast_msg_.missing_frames_and_packets_.clear();

  // Are we missing packets?
  if (frame_id_map_->Empty()) return;

  uint32 newest_frame_id = frame_id_map_->NewestFrameId();
  uint32 next_expected_frame_id = cast_msg_.ack_frame_id_ + 1;

  // Iterate over all frames.
  for (; !IsNewerFrameId(next_expected_frame_id, newest_frame_id);
       ++next_expected_frame_id) {
    TimeLastNackMap::iterator it =
        time_last_nacked_map_.find(next_expected_frame_id);
    if (it != time_last_nacked_map_.end()) {
      // We have sent a NACK in this frame before, make sure enough time have
      // passed.
      if (now - it->second <
          base::TimeDelta::FromMilliseconds(kNackRepeatIntervalMs)) {
        continue;
      }
    }

    PacketIdSet missing;
    if (frame_id_map_->FrameExists(next_expected_frame_id)) {
      bool last_frame = (newest_frame_id == next_expected_frame_id);
      frame_id_map_->GetMissingPackets(next_expected_frame_id, last_frame,
                                       &missing);
      if (!missing.empty()) {
        time_last_nacked_map_[next_expected_frame_id] = now;
        cast_msg_.missing_frames_and_packets_.insert(
            std::make_pair(next_expected_frame_id, missing));
      }
    } else {
      time_last_nacked_map_[next_expected_frame_id] = now;
      missing.insert(kRtcpCastAllPacketsLost);
      cast_msg_.missing_frames_and_packets_[next_expected_frame_id] = missing;
    }
  }
}

}  //  namespace cast
}  //  namespace media
