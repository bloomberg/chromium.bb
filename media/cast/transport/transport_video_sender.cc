// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport_video_sender.h"

#include <list>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/base/video_frame.h"
#include "media/cast/transport/pacing/paced_sender.h"

namespace media {
namespace cast {
namespace transport {

TransportVideoSender::TransportVideoSender(
    const CastTransportConfig& config,
    base::TickClock* clock,
    PacedSender* const paced_packet_sender)
    : rtp_max_delay_(base::TimeDelta::FromMilliseconds(
          config.video_rtp_config.max_delay_ms)),
      rtp_sender_(clock, config, false, paced_packet_sender),
    initialized_(true) {
  if (config.aes_iv_mask.size() == kAesKeySize &&
      config.aes_key.size() == kAesKeySize) {
    iv_mask_ = config.aes_iv_mask;
    key_.reset(crypto::SymmetricKey::Import(
        crypto::SymmetricKey::AES, config.aes_key));
    encryptor_.reset(new crypto::Encryptor());
    encryptor_->Init(key_.get(), crypto::Encryptor::CTR, std::string());
  } else if (config.aes_iv_mask.size() != 0 ||
             config.aes_key.size() != 0) {
    initialized_ = false;
    DCHECK_EQ(config.aes_iv_mask.size(), 0u)
        << "Invalid Crypto configuration: aes_iv_mask.size" ;
    DCHECK_EQ(config.aes_key.size(), 0u)
        << "Invalid Crypto configuration: aes_key.size";
  }
}

TransportVideoSender::~TransportVideoSender() {}

void TransportVideoSender::InsertCodedVideoFrame(
    const EncodedVideoFrame* coded_frame,
    const base::TimeTicks& capture_time) {
  if (encryptor_) {
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
  if (!encryptor_->SetCounter(GetAesNonce(video_frame.frame_id, iv_mask_))) {
    NOTREACHED() << "Failed to set counter";
    return false;
  }

  if (!encryptor_->Encrypt(video_frame.data, &encrypted_frame->data)) {
    NOTREACHED() << "Encrypt error";
    return false;
  }
  encrypted_frame->codec = video_frame.codec;
  encrypted_frame->key_frame = video_frame.key_frame;
  encrypted_frame->frame_id = video_frame.frame_id;
  encrypted_frame->last_referenced_frame_id =
      video_frame.last_referenced_frame_id;
  return true;
}

void TransportVideoSender::GetStatistics(const base::TimeTicks& now,
                                         RtcpSenderInfo* sender_info) {
  rtp_sender_.RtpStatistics(now, sender_info);
}

void TransportVideoSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  rtp_sender_.ResendPackets(missing_frames_and_packets);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
