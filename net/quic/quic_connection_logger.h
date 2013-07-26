// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONNECTION_LOGGER_H_
#define NET_QUIC_QUIC_CONNECTION_LOGGER_H_

#include "net/quic/quic_connection.h"

namespace net {

class BoundNetLog;

// This class is a debug visitor of a QuicConnection which logs
// events to |net_log|.
class NET_EXPORT_PRIVATE QuicConnectionLogger
    : public QuicConnectionDebugVisitorInterface {
 public:
  explicit QuicConnectionLogger(const BoundNetLog& net_log);

  virtual ~QuicConnectionLogger();

  // QuicPacketGenerator::DebugDelegateInterface
  virtual void OnFrameAddedToPacket(const QuicFrame& frame) OVERRIDE;

  // QuicConnectionDebugVisitorInterface
  virtual void OnPacketSent(QuicPacketSequenceNumber sequence_number,
                            EncryptionLevel level,
                            const QuicEncryptedPacket& packet,
                            int rv) OVERRIDE;

  virtual void OnPacketReceived(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet) OVERRIDE;
  virtual void OnProtocolVersionMismatch(QuicVersion version) OVERRIDE;
  virtual void OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;
  virtual void OnAckFrame(const QuicAckFrame& frame) OVERRIDE;
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE;
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE;
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE;
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE;
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) OVERRIDE;
  virtual void OnRevivedPacket(const QuicPacketHeader& revived_header,
                               base::StringPiece payload) OVERRIDE;

 private:
  BoundNetLog net_log_;
  // The last packet sequence number received.
  QuicPacketSequenceNumber last_received_packet_sequence_number_;
  // The largest packet sequence number received.  In case of that a packet is
  // received late, this value will not be updated.
  QuicPacketSequenceNumber largest_received_packet_sequence_number_;
  // The largest packet sequence number which the peer has failed to
  // receive, according to the missing packet set in their ack frames.
  QuicPacketSequenceNumber largest_received_missing_packet_sequence_number_;
  // Number of times that the current received packet sequence number is
  // smaller than the last received packet sequence number.
  size_t out_of_order_recieved_packet_count_;
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionLogger);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_LOGGER_H_
