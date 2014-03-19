// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONNECTION_LOGGER_H_
#define NET_QUIC_QUIC_CONNECTION_LOGGER_H_

#include <bitset>

#include "net/base/ip_endpoint.h"
#include "net/base/network_change_notifier.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_protocol.h"

namespace net {

class CryptoHandshakeMessage;

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
                            TransmissionType transmission_type,
                            const QuicEncryptedPacket& packet,
                            WriteResult result) OVERRIDE;
  virtual void OnPacketRetransmitted(
      QuicPacketSequenceNumber old_sequence_number,
      QuicPacketSequenceNumber new_sequence_number) OVERRIDE;
  virtual void OnPacketReceived(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet) OVERRIDE;
  virtual void OnProtocolVersionMismatch(QuicVersion version) OVERRIDE;
  virtual void OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE;
  virtual void OnAckFrame(const QuicAckFrame& frame) OVERRIDE;
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE;
  virtual void OnStopWaitingFrame(const QuicStopWaitingFrame& frame) OVERRIDE;
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE;
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE;
  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE;
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) OVERRIDE;
  virtual void OnRevivedPacket(const QuicPacketHeader& revived_header,
                               base::StringPiece payload) OVERRIDE;

  void OnCryptoHandshakeMessageReceived(
      const CryptoHandshakeMessage& message);
  void OnCryptoHandshakeMessageSent(
      const CryptoHandshakeMessage& message);
  void OnConnectionClosed(QuicErrorCode error, bool from_peer);
  void OnSuccessfulVersionNegotiation(const QuicVersion& version);

 private:
  // Do a factory get for an "Ack_" or "Nack_" style histogram for recording
  // packet loss stats.
  base::HistogramBase* GetAckHistogram(const char* ack_or_nack) const;
  // Do a factory get for a histogram to record a 6-packet loss-sequence as a
  // sample. The histogram will record the 64 distinct possible combinations.
  // |which_6| is used to adjust the name of the histogram to distinguish the
  // first 6 packets in a connection, vs. some later 6 packets.
  base::HistogramBase* Get6PacketHistogram(const char*  which_6) const;
  // Do a factory get for a histogram to record cumulative stats across a 21
  // packet sequence.  |which_21| is used to adjust the name of the histogram
  // to distinguish the first 21 packets' loss data, vs. some later 21 packet
  // sequences' loss data.
  base::HistogramBase* Get21CumulativeHistogram(const char*  which_21) const;
  // Add samples associated with a |bit_mask_21_packets| to the given histogram
  // that was provided by Get21CumulativeHistogram().  The LSB of that mask
  // corresponds to the oldest packet packet sequence number in the series of 21
  // packets.  A bit value of 0 indicates that a packet was never received, and
  // a 1 indicates the packet was received.
  static void AddTo21CumulativeHistogram(base::HistogramBase* histogram,
                                         int bit_mask_21_packets);

  // At destruction time, this records results of |pacaket_received_| into
  // histograms for specific connection types.
  void RecordLossHistograms() const;

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
  // Number of times a truncated ACK frame was sent.
  size_t num_truncated_acks_sent_;
  // Number of times a truncated ACK frame was received.
  size_t num_truncated_acks_received_;
  // The kCADR value provided by the server in ServerHello.
  IPEndPoint client_address_;
  // Vector of inital packets status' indexed by packet sequence numbers, where
  // false means never received.  Zero is not a valid packet sequence number, so
  // that offset is never used, and we'll track 150 packets.
  std::bitset<151> packets_received_;
  // The available type of connection (WiFi, 3G, etc.) when connection was first
  // used.
  const char* const connection_description_;
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionLogger);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_LOGGER_H_
