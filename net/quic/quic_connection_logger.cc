// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_logger.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/base/net_log.h"
#include "net/quic/crypto/crypto_handshake_message.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_address_mismatch.h"
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
  dict->SetString("connection_id",
                  base::Uint64ToString(header->public_header.connection_id));
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
        string value = base::Uint64ToString(it->first) + "@" +
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

base::Value* NetLogQuicWindowUpdateFrameCallback(
    const QuicWindowUpdateFrame* frame,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("stream_id", frame->stream_id);
  dict->SetString("byte_offset", base::Uint64ToString(frame->byte_offset));
  return dict;
}

base::Value* NetLogQuicBlockedFrameCallback(
    const QuicBlockedFrame* frame,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("stream_id", frame->stream_id);
  return dict;
}

base::Value* NetLogQuicStopWaitingFrameCallback(
    const QuicStopWaitingFrame* frame,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  base::DictionaryValue* sent_info = new base::DictionaryValue();
  dict->Set("sent_info", sent_info);
  sent_info->SetString("least_unacked",
                       base::Uint64ToString(frame->least_unacked));
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
  int sample = GetAddressMismatch(server_hello_address, public_reset_address);
  // We are seemingly talking to an older server that does not support the
  // feature, so we can't report the results in the histogram.
  if (sample < 0) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.PublicResetAddressMismatch",
                            sample, QUIC_ADDRESS_MISMATCH_MAX);
}

}  // namespace

QuicConnectionLogger::QuicConnectionLogger(const BoundNetLog& net_log)
    : net_log_(net_log),
      last_received_packet_sequence_number_(0),
      largest_received_packet_sequence_number_(0),
      largest_received_missing_packet_sequence_number_(0),
      out_of_order_recieved_packet_count_(0),
      num_truncated_acks_sent_(0),
      num_truncated_acks_received_(0),
      connection_type_(NetworkChangeNotifier::GetConnectionType()) {
}

QuicConnectionLogger::~QuicConnectionLogger() {
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.OutOfOrderPacketsReceived",
                       out_of_order_recieved_packet_count_);
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.TruncatedAcksSent",
                       num_truncated_acks_sent_);
  UMA_HISTOGRAM_COUNTS("Net.QuicSession.TruncatedAcksReceived",
                       num_truncated_acks_received_);

  RecordAckNackHistograms();
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
    case WINDOW_UPDATE_FRAME:
      net_log_.AddEvent(
          NetLog::TYPE_QUIC_SESSION_WINDOW_UPDATE_FRAME_SENT,
          base::Bind(&NetLogQuicWindowUpdateFrameCallback,
                     frame.window_update_frame));
      break;
    case BLOCKED_FRAME:
      net_log_.AddEvent(
          NetLog::TYPE_QUIC_SESSION_BLOCKED_FRAME_SENT,
          base::Bind(&NetLogQuicBlockedFrameCallback,
                     frame.blocked_frame));
      break;
    case STOP_WAITING_FRAME:
      net_log_.AddEvent(
          NetLog::TYPE_QUIC_SESSION_STOP_WAITING_FRAME_SENT,
          base::Bind(&NetLogQuicStopWaitingFrameCallback,
                     frame.stop_waiting_frame));
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
  if (header.packet_sequence_number < packets_received_.size())
    packets_received_[header.packet_sequence_number] = true;
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

void QuicConnectionLogger::OnStopWaitingFrame(
    const QuicStopWaitingFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_STOP_WAITING_FRAME_RECEIVED,
      base::Bind(&NetLogQuicStopWaitingFrameCallback, &frame));
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

base::HistogramBase* QuicConnectionLogger::GetAckHistogram(
    const char* ack_or_nack) {
  string prefix("Net.QuicSession.PacketReceived_");
  const char* suffix = NetworkChangeNotifier::ConnectionTypeToString(
      connection_type_);
  return base::LinearHistogram::FactoryGet(prefix + ack_or_nack + suffix, 1,
      packets_received_.size(), packets_received_.size() + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

void QuicConnectionLogger::RecordAckNackHistograms() {
  if (largest_received_packet_sequence_number_ == 0)
    return;  // Connection was never used.
  base::HistogramBase* packet_ack_histogram = GetAckHistogram("Ack_");
  base::HistogramBase* packet_nack_histogram = GetAckHistogram("Nack_");
  const QuicPacketSequenceNumber last_index =
      std::min<QuicPacketSequenceNumber>(
          packets_received_.size() - 1,
          largest_received_packet_sequence_number_);
  // Zero is an invalid packet sequence number.
  DCHECK(!packets_received_[0]);
  for (size_t i = 1; i <= last_index; ++i) {
    if (packets_received_[i])
      packet_ack_histogram->Add(i);
    else
      packet_nack_histogram->Add(i);
  }
}

}  // namespace net
