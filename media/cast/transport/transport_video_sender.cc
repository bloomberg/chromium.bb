// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport_video_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace transport {

TransportVideoSender::TransportVideoSender(
    const CastTransportVideoConfig& config,
    base::TickClock* clock,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
    PacedSender* const paced_packet_sender)
    : rtp_sender_(clock, transport_task_runner, paced_packet_sender) {
  initialized_ = rtp_sender_.InitializeVideo(config) &&
      encryptor_.Initialize(config.rtp.config.aes_key,
                            config.rtp.config.aes_iv_mask);
}

TransportVideoSender::~TransportVideoSender() {}

void TransportVideoSender::SendFrame(const EncodedFrame& video_frame) {
  if (!initialized_) {
    return;
  }
  if (encryptor_.initialized()) {
    EncodedFrame encrypted_frame;
    if (!EncryptVideoFrame(video_frame, &encrypted_frame)) {
      NOTREACHED();
      return;
    }
    rtp_sender_.SendFrame(encrypted_frame);
  } else {
    rtp_sender_.SendFrame(video_frame);
  }
}

bool TransportVideoSender::EncryptVideoFrame(
    const EncodedFrame& video_frame, EncodedFrame* encrypted_frame) {
  if (!initialized_) {
    return false;
  }
  if (!encryptor_.Encrypt(
          video_frame.frame_id, video_frame.data, &(encrypted_frame->data)))
    return false;

  encrypted_frame->dependency = video_frame.dependency;
  encrypted_frame->frame_id = video_frame.frame_id;
  encrypted_frame->referenced_frame_id = video_frame.referenced_frame_id;
  encrypted_frame->rtp_timestamp = video_frame.rtp_timestamp;
  encrypted_frame->reference_time = video_frame.reference_time;
  return true;
}

void TransportVideoSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  if (!initialized_) {
    return;
  }
  rtp_sender_.ResendPackets(missing_frames_and_packets);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
