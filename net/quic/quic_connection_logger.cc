// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_logger.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/net_log.h"
#include "net/quic/crypto/crypto_handshake_message.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_socket_address_coder.h"

using base::StringPiece;
using std::string;

namespace net {

namespace {

base::Value* NetLogQuicPacketCallback(const IPEndPoint* self_address,
                                      const IPEndPoint* peer_address,
                                      size_t packet_size,
                                      NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("self_address", self_address->ToString());
  dict->SetString("peer_address", peer_address->ToString());
  dict->SetInteger("size", packet_size);
  return dict;
}

base::Value* NetLogQuicPacketSentCallback(
    QuicPacketSequenceNumber sequence_number,
    EncryptionLevel level,
    size_t packet_size,
    WriteResult result,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("encryption_level", level);
  dict->SetString("packet_sequence_number",
                  base::Uint64ToString(sequence_number));
  dict->SetInteger("size", packet_size);
  if (result.status != WRITE_STATUS_OK) {
    dict->SetInteger("net_error", result.error_code);
  }
  return dict;
}

base::Value* NetLogQuicPacketRetransmittedCallback(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number,
    NetLog::LogLevel /* log_level */) {
 base::DictionaryValue* dict = new base::DictionaryValue();
 dict->SetString("old_packet_sequence_number",
                 base::Uint64ToString(old_sequence_number));
 dict->SetString("new_packet_sequence_number",
                 base::Uint64ToString(new_sequence_number));
 return dict;
}

base::Value* NetLogQuicPacketHeaderCallback(const QuicPacketHeader* header,
                                            NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("guid",
                  base::Uint64ToString(header->public_header.guid));
  dict->SetInteger("reset_flag", header->public_header.reset_flag);
  dict->SetInteger("version_flag", header->public_header.version_flag);
  dict->SetString("packet_sequence_number",
                  base::Uint64ToString(header->packet_sequence_number));
  dict->SetInteger("entropy_flag", header->entropy_flag);
  dict->SetInteger("fec_flag", header->fec_flag);
  dict->SetInteger("fec_group", header->fec_group);
  return dict;
}

base::Value* NetLogQuicStreamFrameCallback(const QuicStreamFrame* frame,
                                           NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("stream_id", frame->stream_id);
  dict->SetBoolean("fin", frame->fin);
  dict->SetString("offset", base::Uint64ToString(frame->offset));
  dict->SetInteger("length", frame->data.TotalBufferSize());
  return dict;
}

base::Value* NetLogQuicAckFrameCallback(const QuicAckFrame* frame,
                                        NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::DictionaryValue* sent_info = new base::DictionaryValue();
  dict->Set("sent_info", sent_info);
  sent_info->SetString("least_unacked",
                       base::Uint64ToString(frame->sent_info.least_unacked));
  base::DictionaryValue* received_info = new base::DictionaryValue();
  dict->Set("received_info", received_info);
  received_info->SetString(
      "largest_observed",
      base::Uint64ToString(frame->received_info.largest_observed));
  received_info->SetBoolean("truncated", frame->received_info.is_truncated);
  base::ListValue* missing = new base::ListValue();
  received_info->Set("missing_packets", missing);
  const SequenceNumberSet& missing_packets =
      frame->received_info.missing_packets;
  for (SequenceNumberSet::const_iterator it = missing_packets.begin();
       it != missing_packets.end(); ++it) {
    missing->AppendString(base::Uint64ToString(*it));
  }
  return dict;
}

base::Value* NetLogQuicCongestionFeedbackFrameCallback(
    const QuicCongestionFeedbackFrame* frame,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  switch (frame->type) {
    case kInterArrival: {
      dict->SetString("type", "InterArrival");
      base::ListValue* received = new base::ListValue();
      dict->Set("received_packets", received);
      for (TimeMap::const_iterator it =
               frame->inter_arrival.received_packet_times.begin();
           it != frame->inter_arrival.received_packet_times.end(); ++it) {
        std::string value = base::Uint64ToString(it->first) + "@" +
            base::Uint64ToString(it->second.ToDebuggingValue());
        received->AppendString(value);
      }
      break;
    }
    case kFixRate:
      dict->SetString("type", "FixRate");
      dict->SetInteger("bitrate_in_bytes_per_second",
                       frame->fix_rate.bitrate.ToBytesPerSecond());
      break;
    case kTCP:
      dict->SetString("type", "TCP");
      dict->SetInteger("receive_window", frame->tcp.receive_window);
      break;
  }

  return dict;
}

base::Value* NetLogQuicRstStreamFrameCallback(
    const QuicRstStreamFrame* frame,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("stream_id", frame->stream_id);
  dict->SetInteger("quic_rst_stream_error", frame->error_code);
  dict->SetString("details", frame->error_details);
  return dict;
}

base::Value* NetLogQuicConnectionCloseFrameCallback(
    const QuicConnectionCloseFrame* frame,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("quic_error", frame->error_code);
  dict->SetString("details", frame->error_details);
  return dict;
}

base::Value* NetLogQuicVersionNegotiationPacketCallback(
    const QuicVersionNegotiationPacket* packet,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::ListValue* versions = new base::ListValue();
  dict->Set("versions", versions);
  for (QuicVersionVector::const_iterator it = packet->versions.begin();
       it != packet->versions.end(); ++it) {
    versions->AppendString(QuicVersionToString(*it));
  }
  return dict;
}

base::Value* NetLogQuicCryptoHandshakeMessageCallback(
    const CryptoHandshakeMessage* message,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("quic_crypto_handshake_message", message->DebugString());
  return dict;
}

base::Value* NetLogQuicOnConnectionClosedCallback(
    QuicErrorCode error,
    bool from_peer,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("quic_error", error);
  dict->SetBoolean("from_peer", from_peer);
  return dict;
}

void UpdatePacketGapSentHistogram(size_t num_consecutive_missing_packets) {
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.PacketGapSent",
                       num_consecutive_missing_packets);
}

void UpdatePublicResetAddressMismatchHistogram(
    const IPEndPoint& server_hello_address,
    const IPEndPoint& public_reset_address) {
  enum {
    // The addresses don't match.
    kAddressMismatch_base = 0,
    kAddressMismatch_v4_v4 = 0,
    kAddressMismatch_v6_v6 = 1,
    kAddressMismatch_v4_v6 = 2,
    kAddressMismatch_v6_v4 = 3,

    // The addresses match, but the ports don't match.
    kPortMismatch_base = 4,
    kPortMismatch_v4_v4 = 4,
    kPortMismatch_v6_v6 = 5,

    kAddressAndPortMatch_base = 6,
    kAddressAndPortMatch_v4_v4 = 6,
    kAddressAndPortMatch_v6_v6 = 7,

    kBoundaryValue,
  };

  // We are seemingly talking to an older server that does not support the
  // feature, so we can't report the results in the histogram.
  if (server_hello_address.address().empty() ||
      public_reset_address.address().empty()) {
    return;
  }

  int sample;
  if (server_hello_address.address() != public_reset_address.address()) {
    sample = kAddressMismatch_base;
  } else if (server_hello_address.port() != public_reset_address.port()) {
    sample = kPortMismatch_base;
  } else {
    sample = kAddressAndPortMatch_base;
  }

  // Add an offset to |sample|:
  //   v4_v4: add 0
  //   v6_v6: add 1
  //   v4_v6: add 2
  //   v6_v4: add 3
  bool first_ipv4 =
    (server_hello_address.address().size() == kIPv4AddressSize);
  bool second_ipv4 =
    (public_reset_address.address().size() == kIPv4AddressSize);
  if (first_ipv4 != second_ipv4) {
    CHECK_EQ(sample, kAddressMismatch_base);
    sample += 2;
  }
  if (!first_ipv4) {
    sample += 1;
  }
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.PublicResetAddressMismatch",
                            sample, kBoundaryValue);
}

}  // namespace

QuicConnectionLogger::QuicConnectionLogger(const BoundNetLog& net_log)
    : net_log_(net_log),
      last_received_packet_sequence_number_(0),
      largest_received_packet_sequence_number_(0),
      largest_received_missing_packet_sequence_number_(0),
      out_of_order_recieved_packet_count_(0),
      num_truncated_acks_sent_(0),
      num_truncated_acks_received_(0) {
}

QuicConnectionLogger::~QuicConnectionLogger() {
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.OutOfOrderPacketsReceived",
                       out_of_order_recieved_packet_count_);
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.TruncatedAcksSent",
                       num_truncated_acks_sent_);
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.TruncatedAcksReceived",
                       num_truncated_acks_received_);
}

void QuicConnectionLogger::OnFrameAddedToPacket(const QuicFrame& frame) {
  switch (frame.type) {
    case PADDING_FRAME:
      break;
    case STREAM_FRAME:
      net_log_.AddEvent(
          NetLog::TYPE_QUIC_SESSION_STREAM_FRAME_SENT,
          base::Bind(&NetLogQuicStreamFrameCallback, frame.stream_frame));
      break;
    case ACK_FRAME:
      net_log_.AddEvent(
          NetLog::TYPE_QUIC_SESSION_ACK_FRAME_SENT,
          base::Bind(&NetLogQuicAckFrameCallback, frame.ack_frame));
      if (frame.ack_frame->received_info.is_truncated)
        ++num_truncated_acks_sent_;
      break;
    case CONGESTION_FEEDBACK_FRAME:
      net_log_.AddEvent(
          NetLog::TYPE_QUIC_SESSION_CONGESTION_FEEDBACK_FRAME_SENT,
          base::Bind(&NetLogQuicCongestionFeedbackFrameCallback,
                     frame.congestion_feedback_frame));
      break;
    case RST_STREAM_FRAME:
      UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.RstStreamErrorCodeClient",
                                  frame.rst_stream_frame->error_code);
      net_log_.AddEvent(
          NetLog::TYPE_QUIC_SESSION_RST_STREAM_FRAME_SENT,
          base::Bind(&NetLogQuicRstStreamFrameCallback,
                     frame.rst_stream_frame));
      break;
    case CONNECTION_CLOSE_FRAME:
      net_log_.AddEvent(
          NetLog::TYPE_QUIC_SESSION_CONNECTION_CLOSE_FRAME_SENT,
          base::Bind(&NetLogQuicConnectionCloseFrameCallback,
                     frame.connection_close_frame));
      break;
    case GOAWAY_FRAME:
      break;
    default:
      DCHECK(false) << "Illegal frame type: " << frame.type;
  }
}

void QuicConnectionLogger::OnPacketSent(
    QuicPacketSequenceNumber sequence_number,
    EncryptionLevel level,
    const QuicEncryptedPacket& packet,
    WriteResult result) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_PACKET_SENT,
      base::Bind(&NetLogQuicPacketSentCallback, sequence_number, level,
                 packet.length(), result));
}

void QuicConnectionLogger:: OnPacketRetransmitted(
      QuicPacketSequenceNumber old_sequence_number,
      QuicPacketSequenceNumber new_sequence_number) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_PACKET_RETRANSMITTED,
      base::Bind(&NetLogQuicPacketRetransmittedCallback,
                 old_sequence_number, new_sequence_number));
}

void QuicConnectionLogger::OnPacketReceived(const IPEndPoint& self_address,
                                            const IPEndPoint& peer_address,
                                            const QuicEncryptedPacket& packet) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_PACKET_RECEIVED,
      base::Bind(&NetLogQuicPacketCallback, &self_address, &peer_address,
                 packet.length()));
}

void QuicConnectionLogger::OnProtocolVersionMismatch(
    QuicVersion received_version) {
  // TODO(rtenneti): Add logging.
}

void QuicConnectionLogger::OnPacketHeader(const QuicPacketHeader& header) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_PACKET_HEADER_RECEIVED,
      base::Bind(&NetLogQuicPacketHeaderCallback, &header));
  if (largest_received_packet_sequence_number_ <
      header.packet_sequence_number) {
    QuicPacketSequenceNumber delta = header.packet_sequence_number -
        largest_received_packet_sequence_number_;
    if (delta > 1) {
      // There is a gap between the largest packet previously received and
      // the current packet.  This indicates either loss, or out-of-order
      // delivery.
      UMA_HISTOGRAM_COUNTS("Net.QuicSession.PacketGapReceived", delta - 1);
    }
    largest_received_packet_sequence_number_ = header.packet_sequence_number;
  }
  if (header.packet_sequence_number < last_received_packet_sequence_number_) {
    ++out_of_order_recieved_packet_count_;
    UMA_HISTOGRAM_COUNTS("Net.QuicSession.OutOfOrderGapReceived",
                         last_received_packet_sequence_number_ -
                             header.packet_sequence_number);
  }
  last_received_packet_sequence_number_ = header.packet_sequence_number;
}

void QuicConnectionLogger::OnStreamFrame(const QuicStreamFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_STREAM_FRAME_RECEIVED,
      base::Bind(&NetLogQuicStreamFrameCallback, &frame));
}

void QuicConnectionLogger::OnAckFrame(const QuicAckFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_ACK_FRAME_RECEIVED,
      base::Bind(&NetLogQuicAckFrameCallback, &frame));

  if (frame.received_info.is_truncated)
    ++num_truncated_acks_received_;

  if (frame.received_info.missing_packets.empty())
    return;

  SequenceNumberSet missing_packets = frame.received_info.missing_packets;
  SequenceNumberSet::const_iterator it = missing_packets.lower_bound(
      largest_received_missing_packet_sequence_number_);
  if (it == missing_packets.end())
    return;

  if (*it == largest_received_missing_packet_sequence_number_) {
    ++it;
    if (it == missing_packets.end())
      return;
  }
  // Scan through the list and log consecutive ranges of missing packets.
  size_t num_consecutive_missing_packets = 0;
  QuicPacketSequenceNumber previous_missing_packet = *it - 1;
  while (it != missing_packets.end()) {
    if (previous_missing_packet == *it - 1) {
      ++num_consecutive_missing_packets;
    } else {
      DCHECK_NE(0u, num_consecutive_missing_packets);
      UpdatePacketGapSentHistogram(num_consecutive_missing_packets);
      // Make sure this packet it included in the count.
      num_consecutive_missing_packets = 1;
    }
    previous_missing_packet = *it;
    ++it;
  }
  if (num_consecutive_missing_packets != 0) {
    UpdatePacketGapSentHistogram(num_consecutive_missing_packets);
  }
  largest_received_missing_packet_sequence_number_ =
        *missing_packets.rbegin();
}

void QuicConnectionLogger::OnCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CONGESTION_FEEDBACK_FRAME_RECEIVED,
      base::Bind(&NetLogQuicCongestionFeedbackFrameCallback, &frame));
}

void QuicConnectionLogger::OnRstStreamFrame(const QuicRstStreamFrame& frame) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Net.QuicSession.RstStreamErrorCodeServer",
                              frame.error_code);
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_RST_STREAM_FRAME_RECEIVED,
      base::Bind(&NetLogQuicRstStreamFrameCallback, &frame));
}

void QuicConnectionLogger::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CONNECTION_CLOSE_FRAME_RECEIVED,
      base::Bind(&NetLogQuicConnectionCloseFrameCallback, &frame));
}

void QuicConnectionLogger::OnPublicResetPacket(
    const QuicPublicResetPacket& packet) {
  net_log_.AddEvent(NetLog::TYPE_QUIC_SESSION_PUBLIC_RESET_PACKET_RECEIVED);
  UpdatePublicResetAddressMismatchHistogram(client_address_,
                                            packet.client_address);
}

void QuicConnectionLogger::OnVersionNegotiationPacket(
    const QuicVersionNegotiationPacket& packet) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_VERSION_NEGOTIATION_PACKET_RECEIVED,
      base::Bind(&NetLogQuicVersionNegotiationPacketCallback, &packet));
}

void QuicConnectionLogger::OnRevivedPacket(
    const QuicPacketHeader& revived_header,
    base::StringPiece payload) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_PACKET_HEADER_REVIVED,
      base::Bind(&NetLogQuicPacketHeaderCallback, &revived_header));
}

void QuicConnectionLogger::OnCryptoHandshakeMessageReceived(
    const CryptoHandshakeMessage& message) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CRYPTO_HANDSHAKE_MESSAGE_RECEIVED,
      base::Bind(&NetLogQuicCryptoHandshakeMessageCallback, &message));

  if (message.tag() == kSHLO) {
    StringPiece address;
    QuicSocketAddressCoder decoder;
    if (message.GetStringPiece(kCADR, &address) &&
        decoder.Decode(address.data(), address.size())) {
      client_address_ = IPEndPoint(decoder.ip(), decoder.port());
    }
  }
}

void QuicConnectionLogger::OnCryptoHandshakeMessageSent(
    const CryptoHandshakeMessage& message) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CRYPTO_HANDSHAKE_MESSAGE_SENT,
      base::Bind(&NetLogQuicCryptoHandshakeMessageCallback, &message));
}

void QuicConnectionLogger::OnConnectionClosed(QuicErrorCode error,
                                              bool from_peer) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CLOSED,
      base::Bind(&NetLogQuicOnConnectionClosedCallback, error, from_peer));
}

void QuicConnectionLogger::OnSuccessfulVersionNegotiation(
    const QuicVersion& version) {
  string quic_version = QuicVersionToString(version);
  net_log_.AddEvent(NetLog::TYPE_QUIC_SESSION_VERSION_NEGOTIATED,
                    NetLog::StringCallback("version", &quic_version));
}

}  // namespace net
