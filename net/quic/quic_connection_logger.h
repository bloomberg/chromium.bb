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

  // QuicConnectionDebugVisitorInterface
  virtual void OnPacketReceived(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet) OVERRIDE;
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
  virtual void OnRevivedPacket(const QuicPacketHeader& revived_header,
                               base::StringPiece payload) OVERRIDE;

 private:
  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(QuicConnectionLogger);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_LOGGER_H_
