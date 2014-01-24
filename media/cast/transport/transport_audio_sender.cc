// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport_audio_sender.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/cast/transport/rtp_sender/rtp_sender.h"

namespace media {
namespace cast {
namespace transport {

// TODO(mikhal): Add a base class for encryption.
TransportAudioSender::TransportAudioSender(
    const CastTransportConfig& config,
    base::TickClock* clock,
    PacedSender* const paced_packet_sender)
    : rtp_sender_(clock, config, true, paced_packet_sender),
      initialized_(true) {
  if (config.aes_iv_mask.size() == kAesKeySize &&
      config.aes_key.size() == kAesKeySize) {
    iv_mask_ = config.aes_iv_mask;
    crypto::SymmetricKey* key = crypto::SymmetricKey::Import(
        crypto::SymmetricKey::AES, config.aes_key);
    encryptor_.reset(new crypto::Encryptor());
    encryptor_->Init(key, crypto::Encryptor::CTR, std::string());
  } else if (config.aes_iv_mask.size() != 0 ||
             config.aes_key.size() != 0) {
    initialized_ = false;
    DCHECK_EQ(config.aes_iv_mask.size(), 0u)
        << "Invalid Crypto configuration: aes_iv_mask.size" ;
    DCHECK_EQ(config.aes_key.size(), 0u)
        << "Invalid Crypto configuration: aes_key.size";
  }
}

TransportAudioSender::~TransportAudioSender() {}

void TransportAudioSender::InsertCodedAudioFrame(
    const EncodedAudioFrame* audio_frame,
    const base::TimeTicks& recorded_time) {
  if (encryptor_) {
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
  if (!encryptor_->SetCounter(GetAesNonce(audio_frame.frame_id, iv_mask_))) {
    NOTREACHED() << "Failed to set counter";
    return false;
  }
  if (!encryptor_->Encrypt(audio_frame.data, &encrypted_frame->data)) {
    NOTREACHED() << "Encrypt error";
    return false;
  }
  encrypted_frame->codec = audio_frame.codec;
  encrypted_frame->frame_id = audio_frame.frame_id;
  encrypted_frame->samples = audio_frame.samples;
  return true;
}

void TransportAudioSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets) {
  rtp_sender_.ResendPackets(missing_frames_and_packets);
}

void TransportAudioSender::GetStatistics(const base::TimeTicks& now,
                                         RtcpSenderInfo* sender_info) {
  rtp_sender_.RtpStatistics(now, sender_info);
}

}  // namespace transport
}  // namespace cast
}  // namespace media
