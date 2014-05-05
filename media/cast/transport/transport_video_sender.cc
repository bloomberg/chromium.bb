// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport_video_sender.h"

#include <list>

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
    : rtp_max_delay_(base::TimeDelta::FromMilliseconds(
          config.base.rtp_config.max_delay_ms)),
      encryptor_(),
      rtp_sender_(clock, transport_task_runner, paced_packet_sender) {
  rtp_sender_.InitializeVideo(config);
  initialized_ =
      encryptor_.Initialize(config.base.aes_key, config.base.aes_iv_mask);
}

TransportVideoSender::~TransportVideoSender() {}

void TransportVideoSender::InsertCodedVideoFrame(
    const EncodedVideoFrame* coded_frame,
    const base::TimeTicks& capture_time) {
  if (encryptor_.initialized()) {
    EncodedVideoFrame encrypted_video_frame;

    if (!EncryptVideoFrame(*coded_frame, &encrypted_video_frame))
      return;

    rtp_sender_.IncomingEncodedVideoFrame(&encrypted_video_frame, capture_time);
  } else {
    rtp_sender_.IncomingEncodedVideoFrame(coded_frame, capture_time);
  }
  if (coded_frame->key_frame) {
    VLOG(1) << "Send encoded key frame; frame_id:"
            << static_cast<int>(coded_frame->frame_id);
  }
}

bool TransportVideoSender::EncryptVideoFrame(
    const EncodedVideoFrame& video_frame,
    EncodedVideoFrame* encrypted_frame) {
  if (!encryptor_.Encrypt(
          video_frame.frame_id, video_frame.data, &(encrypted_frame->data)))
    return false;

  encrypted_frame->codec = video_frame.codec;
  encrypted_frame->key_frame = video_frame.key_frame;
  encrypted_frame->frame_id = video_frame.frame_id;
  encrypted_frame->last_referenced_frame_id =
      video_frame.last_referenced_frame_id;
  return true;
}

void TransportVideoSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  rtp_sender_.ResendPackets(missing_frames_and_packets);
}

void TransportVideoSender::SubscribeVideoRtpStatsCallback(
    const CastTransportRtpStatistics& callback) {
  rtp_sender_.SubscribeRtpStatsCallback(callback);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
