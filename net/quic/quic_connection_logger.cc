// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_logger.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "net/base/net_log.h"

namespace net {

namespace {

Value* NetLogQuicPacketCallback(const IPEndPoint* self_address,
                                const IPEndPoint* peer_address,
                                size_t packet_size,
                                NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("self_address", self_address->ToString());
  dict->SetString("peer_address", peer_address->ToString());
  dict->SetInteger("size", packet_size);
  return dict;
}

Value* NetLogQuicPacketHeaderCallback(const QuicPacketHeader* header,
                                      NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("guid",
                  base::Uint64ToString(header->public_header.guid));
  dict->SetInteger("reset_flag", header->public_header.reset_flag);
  dict->SetInteger("version_flag", header->public_header.version_flag);
  dict->SetString("packet_sequence_number",
                  base::Uint64ToString(header->packet_sequence_number));
  dict->SetInteger("entropy_flag", header->entropy_flag);
  dict->SetInteger("fec_flag", header->fec_flag);
  dict->SetInteger("fec_entropy_flag", header->fec_entropy_flag);
  dict->SetInteger("fec_group", header->fec_group);
  return dict;
}

Value* NetLogQuicStreamFrameCallback(const QuicStreamFrame* frame,
                                     NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("stream_id", frame->stream_id);
  dict->SetBoolean("fin", frame->fin);
  dict->SetString("offset", base::Uint64ToString(frame->offset));
  dict->SetInteger("length", frame->data.length());
  return dict;
}

Value* NetLogQuicAckFrameCallback(const QuicAckFrame* frame,
                                  NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  DictionaryValue* sent_info = new DictionaryValue();
  dict->Set("sent_info", sent_info);
  sent_info->SetString("least_unacked",
                       base::Uint64ToString(frame->sent_info.least_unacked));
  DictionaryValue* received_info = new DictionaryValue();
  dict->Set("received_info", received_info);
  received_info->SetString(
      "largest_observed",
      base::Uint64ToString(frame->received_info.largest_observed));
  ListValue* missing = new ListValue();
  received_info->Set("missing_packets", missing);
  const SequenceNumberSet& missing_packets =
      frame->received_info.missing_packets;
  for (SequenceNumberSet::const_iterator it = missing_packets.begin();
       it != missing_packets.end(); ++it) {
    missing->Append(new base::StringValue(base::Uint64ToString(*it)));
  }
  return dict;
}

Value* NetLogQuicCongestionFeedbackFrameCallback(
    const QuicCongestionFeedbackFrame* frame,
    NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  switch (frame->type) {
    case kInterArrival: {
      dict->SetString("type", "InterArrival");
      dict->SetInteger("accumulated_number_of_lost_packets",
                       frame->inter_arrival.accumulated_number_of_lost_packets);
      ListValue* received = new ListValue();
      dict->Set("received_packets", received);
      for (TimeMap::const_iterator it =
               frame->inter_arrival.received_packet_times.begin();
           it != frame->inter_arrival.received_packet_times.end(); ++it) {
        std::string value = base::Uint64ToString(it->first) + "@" +
            base::Uint64ToString(it->second.ToMilliseconds());
        received->Append(new base::StringValue(value));
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
      dict->SetInteger("accumulated_number_of_lost_packets",
                       frame->tcp.accumulated_number_of_lost_packets);
      dict->SetInteger("receive_window", frame->tcp.receive_window);
      break;
  }

  return dict;
}

Value* NetLogQuicRstStreamFrameCallback(const QuicRstStreamFrame* frame,
                                        NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("stream_id", frame->stream_id);
  dict->SetInteger("error_code", frame->error_code);
  dict->SetString("details", frame->error_details);
  return dict;
}

Value* NetLogQuicConnectionCloseFrameCallback(
    const QuicConnectionCloseFrame* frame,
    NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("error_code", frame->error_code);
  dict->SetString("details", frame->error_details);
  return dict;
}

}  // namespace

QuicConnectionLogger::QuicConnectionLogger(const BoundNetLog& net_log)
    : net_log_(net_log) {
}

QuicConnectionLogger::~QuicConnectionLogger() {
}

  // QuicConnectionDebugVisitorInterface
void QuicConnectionLogger::OnPacketReceived(const IPEndPoint& self_address,
                                            const IPEndPoint& peer_address,
                                            const QuicEncryptedPacket& packet) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_PACKET_RECEIVED,
      base::Bind(&NetLogQuicPacketCallback, &self_address, &peer_address,
                 packet.length()));
}

void QuicConnectionLogger::OnPacketHeader(const QuicPacketHeader& header) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_PACKET_HEADER_RECEIVED,
      base::Bind(&NetLogQuicPacketHeaderCallback, &header));
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
}

void QuicConnectionLogger::OnCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame) {
  net_log_.AddEvent(
      NetLog::TYPE_QUIC_SESSION_CONGESTION_FEEDBACK_FRAME_RECEIVED,
      base::Bind(&NetLogQuicCongestionFeedbackFrameCallback, &frame));
}

void QuicConnectionLogger::OnRstStreamFrame(const QuicRstStreamFrame& frame) {
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
}

void QuicConnectionLogger::OnRevivedPacket(
    const QuicPacketHeader& revived_header,
    base::StringPiece payload) {
}

}  // namespace net
