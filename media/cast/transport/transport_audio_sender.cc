// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport_audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/transport/rtp_sender/rtp_sender.h"

namespace media {
namespace cast {
namespace transport {

TransportAudioSender::TransportAudioSender(
    const CastTransportConfig& config,
    base::TickClock* clock,
    const scoped_refptr<base::TaskRunner>& transport_task_runner,
    PacedSender* const paced_packet_sender)
    : rtp_sender_(clock,
                  config,
                  true,
                  transport_task_runner,
                  paced_packet_sender),
      encryptor_() {
  initialized_ = encryptor_.Initialize(config.aes_key, config.aes_iv_mask);
}

TransportAudioSender::~TransportAudioSender() {}

void TransportAudioSender::InsertCodedAudioFrame(
    const EncodedAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time) {
  if (encryptor_.initialized()) {
    EncodedAudioFrame encrypted_frame;
    if (!EncryptAudioFrame(*audio_frame, &encrypted_frame)) {
      return;
    }
    rtp_sender_.IncomingEncodedAudioFrame(&encrypted_frame, recorded_time);
  } else {
    rtp_sender_.IncomingEncodedAudioFrame(audio_frame, recorded_time);
  }
}

bool TransportAudioSender::EncryptAudioFrame(
    const EncodedAudioFrame& audio_frame,
    EncodedAudioFrame* encrypted_frame) {
  if (!encryptor_.Encrypt(
           audio_frame.frame_id, audio_frame.data, &encrypted_frame->data))
    return false;

  encrypted_frame->codec = audio_frame.codec;
  encrypted_frame->frame_id = audio_frame.frame_id;
  encrypted_frame->rtp_timestamp = audio_frame.rtp_timestamp;
  return true;
}

void TransportAudioSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  rtp_sender_.ResendPackets(missing_frames_and_packets);
}

void TransportAudioSender::SubscribeAudioRtpStatsCallback(
    const CastTransportRtpStatistics& callback) {
  rtp_sender_.SubscribeRtpStatsCallback(callback);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
