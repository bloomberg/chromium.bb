// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides a simple interface for QUIC tests to create a variety of packets.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_TEST_PACKET_MAKER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_TEST_PACKET_MAKER_H_

#include "base/memory/scoped_ptr.h"
#include "net/base/request_priority.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

namespace net {
namespace test {

class QuicTestPacketMaker {
 public:
  QuicTestPacketMaker(QuicVersion version,
                      QuicConnectionId connection_id,
                      MockClock* clock,
                      const std::string& host);
  ~QuicTestPacketMaker();

  void set_hostname(const std::string& host);
  scoped_ptr<QuicEncryptedPacket> MakeRstPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicStreamId stream_id,
      QuicRstStreamErrorCode error_code);
  scoped_ptr<QuicEncryptedPacket> MakeAckAndRstPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicStreamId stream_id,
      QuicRstStreamErrorCode error_code,
      QuicPacketNumber largest_received,
      QuicPacketNumber least_unacked,
      bool send_feedback);
  scoped_ptr<QuicEncryptedPacket> MakeConnectionClosePacket(
      QuicPacketNumber num);
  scoped_ptr<QuicEncryptedPacket> MakeAckPacket(
      QuicPacketNumber packet_number,
      QuicPacketNumber largest_received,
      QuicPacketNumber least_unacked,
      bool send_feedback);
  scoped_ptr<QuicEncryptedPacket> MakeDataPacket(QuicPacketNumber packet_number,
                                                 QuicStreamId stream_id,
                                                 bool should_include_version,
                                                 bool fin,
                                                 QuicStreamOffset offset,
                                                 base::StringPiece data);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  scoped_ptr<QuicEncryptedPacket> MakeRequestHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicPriority priority,
      const SpdyHeaderBlock& headers,
      size_t* spdy_headers_frame_length);

  // Convenience method for calling MakeRequestHeadersPacket with nullptr for
  // |spdy_headers_frame_length|.
  scoped_ptr<QuicEncryptedPacket> MakeRequestHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicPriority priority,
      const SpdyHeaderBlock& headers);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  scoped_ptr<QuicEncryptedPacket> MakeResponseHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers,
      size_t* spdy_headers_frame_length);

  // Convenience method for calling MakeResponseHeadersPacket with nullptr for
  // |spdy_headers_frame_length|.
  scoped_ptr<QuicEncryptedPacket> MakeResponseHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      const SpdyHeaderBlock& headers);

  SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                    const std::string& scheme,
                                    const std::string& path);
  SpdyHeaderBlock GetResponseHeaders(const std::string& status);

 private:
  scoped_ptr<QuicEncryptedPacket> MakePacket(
      const QuicPacketHeader& header,
      const QuicFrame& frame);

  void InitializeHeader(QuicPacketNumber packet_number,
                        bool should_include_version);

  QuicVersion version_;
  QuicConnectionId connection_id_;
  MockClock* clock_;  // Owned by QuicStreamFactory.
  std::string host_;
  SpdyFramer spdy_request_framer_;
  SpdyFramer spdy_response_framer_;
  MockRandom random_generator_;
  QuicPacketHeader header_;

  DISALLOW_COPY_AND_ASSIGN(QuicTestPacketMaker);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_TEST_PACKET_MAKER_H_
