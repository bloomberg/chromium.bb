// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/framer.h"

#include "base/logging.h"

namespace media {
namespace cast {

typedef FrameList::const_iterator ConstFrameIterator;

Framer::Framer(base::TickClock* clock,
               RtpPayloadFeedback* incoming_payload_feedback,
               uint32 ssrc,
               bool decoder_faster_than_max_frame_rate,
               int max_unacked_frames)
    : decoder_faster_than_max_frame_rate_(decoder_faster_than_max_frame_rate),
      cast_msg_builder_(
          new CastMessageBuilder(clock,
                                 incoming_payload_feedback,
                                 this,
                                 ssrc,
                                 decoder_faster_than_max_frame_rate,
                                 max_unacked_frames)),
      waiting_for_key_(true),
      last_released_frame_(kStartFrameId),
      newest_frame_id_(kStartFrameId) {
  DCHECK(incoming_payload_feedback) << "Invalid argument";
}

Framer::~Framer() {}

bool Framer::InsertPacket(const uint8* payload_data,
                          size_t payload_size,
                          const RtpCastHeader& rtp_header,
                          bool* duplicate) {
  *duplicate = false;
  uint32 frame_id = rtp_header.frame_id;

  if (rtp_header.is_key_frame && waiting_for_key_) {
    last_released_frame_ = static_cast<uint32>(frame_id - 1);
    waiting_for_key_ = false;
  }

  VLOG(1) << "InsertPacket frame:" << frame_id
          << " packet:" << static_cast<int>(rtp_header.packet_id)
          << " max packet:" << static_cast<int>(rtp_header.max_packet_id);

  if (IsOlderFrameId(frame_id, last_released_frame_) && !waiting_for_key_) {
    // Packet is too old.
    return false;
  }

  // Update the last received frame id.
  if (IsNewerFrameId(frame_id, newest_frame_id_)) {
    newest_frame_id_ = frame_id;
  }

  // Does this packet belong to a new frame?
  FrameList::iterator it = frames_.find(frame_id);
  if (it == frames_.end()) {
    // New frame.
    linked_ptr<FrameBuffer> frame_info(new FrameBuffer);
    std::pair<FrameList::iterator, bool> retval =
        frames_.insert(std::make_pair(frame_id, frame_info));
    it = retval.first;
  }

    // Insert packet.
  if (!it->second->InsertPacket(payload_data, payload_size, rtp_header)) {
    VLOG(3) << "Packet already received, ignored: frame "
            << static_cast<int>(rtp_header.frame_id) << ", packet "
            << rtp_header.packet_id;
    *duplicate = true;
    return false;
  }

  return it->second->Complete();
}

// This does not release the frame.
bool Framer::GetEncodedFrame(EncodedFrame* frame,
                             bool* next_frame,
                             bool* have_multiple_decodable_frames) {
  *have_multiple_decodable_frames = HaveMultipleDecodableFrames();

  uint32 frame_id;
  // Find frame id.
  if (NextContinuousFrame(&frame_id)) {
    // We have our next frame.
    *next_frame = true;
  } else {
    // Check if we can skip frames when our decoder is too slow.
    if (!decoder_faster_than_max_frame_rate_)
      return false;

    if (!NextFrameAllowingSkippingFrames(&frame_id)) {
      return false;
    }
    *next_frame = false;
  }

  ConstFrameIterator it = frames_.find(frame_id);
  DCHECK(it != frames_.end());
  if (it == frames_.end())
    return false;

  return it->second->AssembleEncodedFrame(frame);
}

void Framer::AckFrame(uint32 frame_id) {
  VLOG(2) << "ACK frame " << frame_id;
  cast_msg_builder_->CompleteFrameReceived(frame_id);
}

void Framer::Reset() {
  waiting_for_key_ = true;
  last_released_frame_ = kStartFrameId;
  newest_frame_id_ = kStartFrameId;
  frames_.clear();
  cast_msg_builder_->Reset();
}

void Framer::ReleaseFrame(uint32 frame_id) {
  RemoveOldFrames(frame_id);
  frames_.erase(frame_id);

  // We have a frame - remove all frames with lower frame id.
  bool skipped_old_frame = false;
  FrameList::iterator it;
  for (it = frames_.begin(); it != frames_.end();) {
    if (IsOlderFrameId(it->first, frame_id)) {
      frames_.erase(it++);
      skipped_old_frame = true;
    } else {
      ++it;
    }
  }
  if (skipped_old_frame) {
    cast_msg_builder_->UpdateCastMessage();
  }
}

bool Framer::TimeToSendNextCastMessage(base::TimeTicks* time_to_send) {
  return cast_msg_builder_->TimeToSendNextCastMessage(time_to_send);
}

void Framer::SendCastMessage() { cast_msg_builder_->UpdateCastMessage(); }

void Framer::RemoveOldFrames(uint32 frame_id) {
  FrameList::iterator it = frames_.begin();

  while (it != frames_.end()) {
    if (IsNewerFrameId(it->first, frame_id)) {
      ++it;
    } else {
      // Older or equal; erase.
      frames_.erase(it++);
    }
  }
  last_released_frame_ = frame_id;
}

uint32 Framer::NewestFrameId() const { return newest_frame_id_; }

bool Framer::NextContinuousFrame(uint32* frame_id) const {
  FrameList::const_iterator it;

  for (it = frames_.begin(); it != frames_.end(); ++it) {
    if (it->second->Complete() && ContinuousFrame(it->second.get())) {
      *frame_id = it->first;
      return true;
    }
  }
  return false;
}

bool Framer::HaveMultipleDecodableFrames() const {
  // Find the oldest decodable frame.
  FrameList::const_iterator it;
  bool found_one = false;
  for (it = frames_.begin(); it != frames_.end(); ++it) {
    if (it->second->Complete() && DecodableFrame(it->second.get())) {
      if (found_one) {
        return true;
      } else {
        found_one = true;
      }
    }
  }
  return false;
}

uint32 Framer::LastContinuousFrame() const {
  uint32 last_continuous_frame_id = last_released_frame_;
  uint32 next_expected_frame = last_released_frame_;

  FrameList::const_iterator it;

  do {
    next_expected_frame++;
    it = frames_.find(next_expected_frame);
    if (it == frames_.end())
      break;
    if (!it->second->Complete())
      break;

    // We found the next continuous frame.
    last_continuous_frame_id = it->first;
  } while (next_expected_frame != newest_frame_id_);
  return last_continuous_frame_id;
}

bool Framer::NextFrameAllowingSkippingFrames(uint32* frame_id) const {
  // Find the oldest decodable frame.
  FrameList::const_iterator it_best_match = frames_.end();
  FrameList::const_iterator it;
  for (it = frames_.begin(); it != frames_.end(); ++it) {
    if (it->second->Complete() && DecodableFrame(it->second.get())) {
      if (it_best_match == frames_.end() ||
          IsOlderFrameId(it->first, it_best_match->first)) {
        it_best_match = it;
      }
    }
  }
  if (it_best_match == frames_.end())
    return false;

  *frame_id = it_best_match->first;
  return true;
}

bool Framer::Empty() const { return frames_.empty(); }

int Framer::NumberOfCompleteFrames() const {
  int count = 0;
  FrameList::const_iterator it;
  for (it = frames_.begin(); it != frames_.end(); ++it) {
    if (it->second->Complete()) {
      ++count;
    }
  }
  return count;
}

bool Framer::FrameExists(uint32 frame_id) const {
  return frames_.end() != frames_.find(frame_id);
}

void Framer::GetMissingPackets(uint32 frame_id,
                                   bool last_frame,
                                   PacketIdSet* missing_packets) const {
  FrameList::const_iterator it = frames_.find(frame_id);
  if (it == frames_.end())
    return;

  it->second->GetMissingPackets(last_frame, missing_packets);
}

bool Framer::ContinuousFrame(FrameBuffer* frame) const {
  DCHECK(frame);
  if (waiting_for_key_ && !frame->is_key_frame())
    return false;
  return static_cast<uint32>(last_released_frame_ + 1) == frame->frame_id();
}

bool Framer::DecodableFrame(FrameBuffer* frame) const {
  if (frame->is_key_frame())
    return true;
  if (waiting_for_key_ && !frame->is_key_frame())
    return false;
  // Self-reference?
  if (frame->last_referenced_frame_id() == frame->frame_id())
    return true;

  // Current frame is not necessarily referencing the last frame.
  // Do we have the reference frame?
  if (IsOlderFrameId(frame->last_referenced_frame_id(), last_released_frame_)) {
    return true;
  }
  return frame->last_referenced_frame_id() == last_released_frame_;
}


}  // namespace cast
}  // namespace media
