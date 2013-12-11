// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/framer/frame_buffer.h"

#include "base/logging.h"

namespace media {
namespace cast {

FrameBuffer::FrameBuffer()
    : frame_id_(0),
      max_packet_id_(0),
      num_packets_received_(0),
      is_key_frame_(false),
      total_data_size_(0),
      last_referenced_frame_id_(0),
      packets_() {}

FrameBuffer::~FrameBuffer() {}

void FrameBuffer::InsertPacket(const uint8* payload_data,
                               size_t payload_size,
                               const RtpCastHeader& rtp_header) {
  // Is this the first packet in the frame?
  if (packets_.empty()) {
    frame_id_ = rtp_header.frame_id;
    max_packet_id_ = rtp_header.max_packet_id;
    is_key_frame_ = rtp_header.is_key_frame;
    if (rtp_header.is_reference) {
      last_referenced_frame_id_ = rtp_header.reference_frame_id;
    } else {
      last_referenced_frame_id_ = rtp_header.frame_id - 1;
    }

    rtp_timestamp_ = rtp_header.webrtc.header.timestamp;
  }
  // Is this the correct frame?
  if (rtp_header.frame_id != frame_id_) return;

  // Insert every packet only once.
  if (packets_.find(rtp_header.packet_id) != packets_.end()) {
    VLOG(3) << "Packet already received, ignored: frame "
            << frame_id_ << ", packet " << rtp_header.packet_id;
    return;
  }

  std::vector<uint8> data;
  std::pair<PacketMap::iterator, bool> retval =
      packets_.insert(make_pair(rtp_header.packet_id, data));

  // Insert the packet.
  retval.first->second.resize(payload_size);
  std::copy(payload_data, payload_data + payload_size,
            retval.first->second.begin());

  ++num_packets_received_;
  total_data_size_ += payload_size;
}

bool FrameBuffer::Complete() const {
  return num_packets_received_ - 1 == max_packet_id_;
}

bool FrameBuffer::GetEncodedAudioFrame(EncodedAudioFrame* audio_frame,
                                       uint32* rtp_timestamp) const {
  if (!Complete()) return false;

  *rtp_timestamp = rtp_timestamp_;

  // Frame is complete -> construct.
  audio_frame->frame_id = frame_id_;

  // Build the data vector.
  audio_frame->data.clear();
  audio_frame->data.reserve(total_data_size_);
  PacketMap::const_iterator it;
  for (it = packets_.begin(); it != packets_.end(); ++it) {
    audio_frame->data.insert(audio_frame->data.end(),
                             it->second.begin(), it->second.end());
  }
  return true;
}

bool FrameBuffer::GetEncodedVideoFrame(EncodedVideoFrame* video_frame,
                                       uint32* rtp_timestamp) const {
  if (!Complete()) return false;

  *rtp_timestamp = rtp_timestamp_;

  // Frame is complete -> construct.
  video_frame->key_frame = is_key_frame_;
  video_frame->frame_id = frame_id_;
  video_frame->last_referenced_frame_id = last_referenced_frame_id_;

  // Build the data vector.
  video_frame->data.clear();
  video_frame->data.reserve(total_data_size_);
  PacketMap::const_iterator it;
  for (it = packets_.begin(); it != packets_.end(); ++it) {
    video_frame->data.insert(video_frame->data.end(),
                             it->second.begin(), it->second.end());
  }
  return true;
}

}  // namespace cast
}  // namespace media
