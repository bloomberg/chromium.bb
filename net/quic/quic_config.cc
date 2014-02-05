// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_config.h"

#include <algorithm>

#include "base/logging.h"
#include "net/quic/crypto/crypto_handshake_message.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"
#include "net/quic/quic_utils.h"

using std::string;

namespace net {

QuicNegotiableValue::QuicNegotiableValue(QuicTag tag, Presence presence)
    : tag_(tag),
      presence_(presence),
      negotiated_(false) {
}

QuicNegotiableUint32::QuicNegotiableUint32(QuicTag tag, Presence presence)
    : QuicNegotiableValue(tag, presence),
      max_value_(0),
      default_value_(0) {
}

void QuicNegotiableUint32::set(uint32 max, uint32 default_value) {
  DCHECK_LE(default_value, max);
  max_value_ = max;
  default_value_ = default_value;
}

uint32 QuicNegotiableUint32::GetUint32() const {
  if (negotiated_) {
    return negotiated_value_;
  }
  return default_value_;
}

void QuicNegotiableUint32::ToHandshakeMessage(
    CryptoHandshakeMessage* out) const {
  if (negotiated_) {
    out->SetValue(tag_, negotiated_value_);
  } else {
    out->SetValue(tag_, max_value_);
  }
}

QuicErrorCode QuicNegotiableUint32::ReadUint32(
    const CryptoHandshakeMessage& msg,
    uint32* out,
    string* error_details) const {
  DCHECK(error_details != NULL);
  QuicErrorCode error = msg.GetUint32(tag_, out);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == QuicNegotiableValue::PRESENCE_REQUIRED) {
        *error_details = "Missing " + QuicUtils::TagToString(tag_);
        break;
      }
      error = QUIC_NO_ERROR;
      *out = default_value_;

    case QUIC_NO_ERROR:
      break;
    default:
      *error_details = "Bad " + QuicUtils::TagToString(tag_);
      break;
  }
  return error;
}

QuicErrorCode QuicNegotiableUint32::ProcessClientHello(
    const CryptoHandshakeMessage& client_hello,
    string* error_details) {
  DCHECK(!negotiated_);
  DCHECK(error_details != NULL);
  uint32 value;
  QuicErrorCode error = ReadUint32(client_hello, &value, error_details);
  if (error != QUIC_NO_ERROR) {
    return error;
  }

  negotiated_ = true;
  negotiated_value_ = std::min(value, max_value_);

  return QUIC_NO_ERROR;
}

QuicErrorCode QuicNegotiableUint32::ProcessServerHello(
    const CryptoHandshakeMessage& server_hello,
    string* error_details) {
  DCHECK(!negotiated_);
  DCHECK(error_details != NULL);
  uint32 value;
  QuicErrorCode error = ReadUint32(server_hello, &value, error_details);
  if (error != QUIC_NO_ERROR) {
    return error;
  }

  if (value > max_value_) {
    *error_details = "Invalid value received for " +
        QuicUtils::TagToString(tag_);
    return QUIC_INVALID_NEGOTIATED_VALUE;
  }

  negotiated_ = true;
  negotiated_value_ = value;
  return QUIC_NO_ERROR;
}

QuicNegotiableTag::QuicNegotiableTag(QuicTag tag, Presence presence)
    : QuicNegotiableValue(tag, presence),
      negotiated_tag_(0),
      default_value_(0) {
}

QuicNegotiableTag::~QuicNegotiableTag() {}

void QuicNegotiableTag::set(const QuicTagVector& possible,
                            QuicTag default_value) {
  DCHECK(std::find(possible.begin(), possible.end(), default_value) !=
            possible.end());
  possible_values_ = possible;
  default_value_ = default_value;
}

QuicTag QuicNegotiableTag::GetTag() const {
  if (negotiated_) {
    return negotiated_tag_;
  }
  return default_value_;
}

void QuicNegotiableTag::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  if (negotiated_) {
    // Because of the way we serialize and parse handshake messages we can
    // serialize this as value and still parse it as a vector.
    out->SetValue(tag_, negotiated_tag_);
  } else {
    out->SetVector(tag_, possible_values_);
  }
}

QuicErrorCode QuicNegotiableTag::ReadVector(
    const CryptoHandshakeMessage& msg,
    const QuicTag** out,
    size_t* out_length,
    string* error_details) const {
  DCHECK(error_details != NULL);
  QuicErrorCode error = msg.GetTaglist(tag_, out, out_length);
  switch (error) {
    case QUIC_CRYPTO_MESSAGE_PARAMETER_NOT_FOUND:
      if (presence_ == PRESENCE_REQUIRED) {
        *error_details = "Missing " + QuicUtils::TagToString(tag_);
        break;
      }
      error = QUIC_NO_ERROR;
      *out_length = 1;
      *out = &default_value_;

    case QUIC_NO_ERROR:
      break;
    default:
      *error_details = "Bad " + QuicUtils::TagToString(tag_);
      break;
  }
  return error;
}

QuicErrorCode QuicNegotiableTag::ProcessClientHello(
    const CryptoHandshakeMessage& client_hello,
    string* error_details) {
  DCHECK(!negotiated_);
  DCHECK(error_details != NULL);
  const QuicTag* received_tags;
  size_t received_tags_length;
  QuicErrorCode error = ReadVector(client_hello, &received_tags,
                                   &received_tags_length, error_details);
  if (error != QUIC_NO_ERROR) {
    return error;
  }

  QuicTag negotiated_tag;
  if (!QuicUtils::FindMutualTag(possible_values_,
                                received_tags,
                                received_tags_length,
                                QuicUtils::LOCAL_PRIORITY,
                                &negotiated_tag,
                                NULL)) {
    *error_details = "Unsuported " + QuicUtils::TagToString(tag_);
    return QUIC_CRYPTO_MESSAGE_PARAMETER_NO_OVERLAP;
  }

  negotiated_ = true;
  negotiated_tag_ = negotiated_tag;
  return QUIC_NO_ERROR;
}

QuicErrorCode QuicNegotiableTag::ProcessServerHello(
    const CryptoHandshakeMessage& server_hello,
    string* error_details) {
  DCHECK(!negotiated_);
  DCHECK(error_details != NULL);
  const QuicTag* received_tags;
  size_t received_tags_length;
  QuicErrorCode error = ReadVector(server_hello, &received_tags,
                                   &received_tags_length, error_details);
  if (error != QUIC_NO_ERROR) {
    return error;
  }

  if (received_tags_length != 1 ||
      std::find(possible_values_.begin(), possible_values_.end(),
                *received_tags) == possible_values_.end()) {
    *error_details = "Invalid " + QuicUtils::TagToString(tag_);
    return QUIC_INVALID_NEGOTIATED_VALUE;
  }

  negotiated_ = true;
  negotiated_tag_ = *received_tags;
  return QUIC_NO_ERROR;
}

QuicConfig::QuicConfig() :
    congestion_control_(kCGST, QuicNegotiableValue::PRESENCE_REQUIRED),
    idle_connection_state_lifetime_seconds_(
        kICSL, QuicNegotiableValue::PRESENCE_REQUIRED),
    keepalive_timeout_seconds_(kKATO, QuicNegotiableValue::PRESENCE_OPTIONAL),
    max_streams_per_connection_(kMSPC, QuicNegotiableValue::PRESENCE_REQUIRED),
    max_time_before_crypto_handshake_(QuicTime::Delta::Zero()),
    server_initial_congestion_window_(
        kSWND, QuicNegotiableValue::PRESENCE_OPTIONAL),
    initial_round_trip_time_us_(kIRTT, QuicNegotiableValue::PRESENCE_OPTIONAL) {
  // All optional non-zero parameters should be initialized here.
  server_initial_congestion_window_.set(kMaxInitialWindow,
                                        kDefaultInitialWindow);
}

QuicConfig::~QuicConfig() {}

void QuicConfig::set_congestion_control(
    const QuicTagVector& congestion_control,
    QuicTag default_congestion_control) {
  congestion_control_.set(congestion_control, default_congestion_control);
}

QuicTag QuicConfig::congestion_control() const {
  return congestion_control_.GetTag();
}

void QuicConfig::set_idle_connection_state_lifetime(
    QuicTime::Delta max_idle_connection_state_lifetime,
    QuicTime::Delta default_idle_conection_state_lifetime) {
  idle_connection_state_lifetime_seconds_.set(
      max_idle_connection_state_lifetime.ToSeconds(),
      default_idle_conection_state_lifetime.ToSeconds());
}

QuicTime::Delta QuicConfig::idle_connection_state_lifetime() const {
  return QuicTime::Delta::FromSeconds(
      idle_connection_state_lifetime_seconds_.GetUint32());
}

QuicTime::Delta QuicConfig::keepalive_timeout() const {
  return QuicTime::Delta::FromSeconds(
      keepalive_timeout_seconds_.GetUint32());
}

void QuicConfig::set_max_streams_per_connection(size_t max_streams,
                                                size_t default_streams) {
  max_streams_per_connection_.set(max_streams, default_streams);
}

uint32 QuicConfig::max_streams_per_connection() const {
  return max_streams_per_connection_.GetUint32();
}

void QuicConfig::set_max_time_before_crypto_handshake(
    QuicTime::Delta max_time_before_crypto_handshake) {
  max_time_before_crypto_handshake_ = max_time_before_crypto_handshake;
}

QuicTime::Delta QuicConfig::max_time_before_crypto_handshake() const {
  return max_time_before_crypto_handshake_;
}

void QuicConfig::set_server_initial_congestion_window(size_t max_initial_window,
                                               size_t default_initial_window) {
  server_initial_congestion_window_.set(max_initial_window,
                                        default_initial_window);
}

uint32 QuicConfig::server_initial_congestion_window() const {
  return server_initial_congestion_window_.GetUint32();
}

void QuicConfig::set_initial_round_trip_time_us(size_t max_rtt,
                                                size_t default_rtt) {
  initial_round_trip_time_us_.set(max_rtt, default_rtt);
}

uint32 QuicConfig::initial_round_trip_time_us() const {
  return initial_round_trip_time_us_.GetUint32();
}

bool QuicConfig::negotiated() {
  // TODO(ianswett): Add the negotiated parameters once and iterate over all
  // of them in negotiated, ToHandshakeMessage, ProcessClientHello, and
  // ProcessServerHello.
  return congestion_control_.negotiated() &&
      idle_connection_state_lifetime_seconds_.negotiated() &&
      keepalive_timeout_seconds_.negotiated() &&
      max_streams_per_connection_.negotiated() &&
      server_initial_congestion_window_.negotiated() &&
      initial_round_trip_time_us_.negotiated();
}

void QuicConfig::SetDefaults() {
  QuicTagVector congestion_control;
  if (FLAGS_enable_quic_pacing) {
    congestion_control.push_back(kPACE);
  }
  congestion_control.push_back(kQBIC);
  congestion_control_.set(congestion_control, kQBIC);
  idle_connection_state_lifetime_seconds_.set(kDefaultTimeoutSecs,
                                              kDefaultInitialTimeoutSecs);
  // kKATO is optional. Return 0 if not negotiated.
  keepalive_timeout_seconds_.set(0, 0);
  max_streams_per_connection_.set(kDefaultMaxStreamsPerConnection,
                                  kDefaultMaxStreamsPerConnection);
  max_time_before_crypto_handshake_ = QuicTime::Delta::FromSeconds(
      kDefaultMaxTimeForCryptoHandshakeSecs);
  server_initial_congestion_window_.set(kDefaultInitialWindow,
                                        kDefaultInitialWindow);
}

void QuicConfig::ToHandshakeMessage(CryptoHandshakeMessage* out) const {
  congestion_control_.ToHandshakeMessage(out);
  idle_connection_state_lifetime_seconds_.ToHandshakeMessage(out);
  keepalive_timeout_seconds_.ToHandshakeMessage(out);
  max_streams_per_connection_.ToHandshakeMessage(out);
  server_initial_congestion_window_.ToHandshakeMessage(out);
  // TODO(ianswett): Don't transmit parameters which are optional and not set.
  initial_round_trip_time_us_.ToHandshakeMessage(out);
}

QuicErrorCode QuicConfig::ProcessClientHello(
    const CryptoHandshakeMessage& client_hello,
    string* error_details) {
  DCHECK(error_details != NULL);

  QuicErrorCode error = QUIC_NO_ERROR;
  if (error == QUIC_NO_ERROR) {
    error = congestion_control_.ProcessClientHello(client_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = idle_connection_state_lifetime_seconds_.ProcessClientHello(
        client_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = keepalive_timeout_seconds_.ProcessClientHello(
        client_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = max_streams_per_connection_.ProcessClientHello(
        client_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = server_initial_congestion_window_.ProcessClientHello(
        client_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_round_trip_time_us_.ProcessClientHello(
        client_hello, error_details);
  }
  return error;
}

QuicErrorCode QuicConfig::ProcessServerHello(
    const CryptoHandshakeMessage& server_hello,
    string* error_details) {
  DCHECK(error_details != NULL);

  QuicErrorCode error = QUIC_NO_ERROR;
  if (error == QUIC_NO_ERROR) {
    error = congestion_control_.ProcessServerHello(server_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = idle_connection_state_lifetime_seconds_.ProcessServerHello(
        server_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = keepalive_timeout_seconds_.ProcessServerHello(
        server_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = max_streams_per_connection_.ProcessServerHello(
        server_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = server_initial_congestion_window_.ProcessServerHello(
        server_hello, error_details);
  }
  if (error == QUIC_NO_ERROR) {
    error = initial_round_trip_time_us_.ProcessServerHello(
        server_hello, error_details);
  }
  return error;
}

}  // namespace net
