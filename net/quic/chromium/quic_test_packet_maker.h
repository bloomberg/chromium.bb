// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides a simple interface for QUIC tests to create a variety of packets.

#ifndef NET_QUIC_CHROMIUM_QUIC_TEST_PACKET_MAKER_H_
#define NET_QUIC_CHROMIUM_QUIC_TEST_PACKET_MAKER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/request_priority.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/mock_random.h"
#include "net/third_party/spdy/core/spdy_framer.h"
#include "net/third_party/spdy/core/spdy_protocol.h"

namespace net {
namespace test {

class QuicTestPacketMaker {
 public:
  struct Http2StreamDependency {
    QuicStreamId stream_id;
    QuicStreamId parent_stream_id;
    spdy::SpdyPriority spdy_priority;
  };

  // |client_headers_include_h2_stream_dependency| affects the output of
  // the MakeRequestHeaders...() methods. If its value is true, then request
  // headers are constructed with the exclusive flag set to true and the parent
  // stream id set to the |parent_stream_id| param of MakeRequestHeaders...().
  // Otherwise, headers are constructed with the exclusive flag set to false and
  // the parent stream ID set to 0 (ignoring the |parent_stream_id| param).
  QuicTestPacketMaker(QuicTransportVersion version,
                      QuicConnectionId connection_id,
                      MockClock* clock,
                      const std::string& host,
                      Perspective perspective,
                      bool client_headers_include_h2_stream_dependency);
  ~QuicTestPacketMaker();

  void set_hostname(const std::string& host);
  std::unique_ptr<QuicReceivedPacket> MakeConnectivityProbingPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicByteCount packet_length);
  std::unique_ptr<QuicReceivedPacket> MakePingPacket(QuicPacketNumber num,
                                                     bool include_version);
  std::unique_ptr<QuicReceivedPacket> MakeAckAndPingPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicPacketNumber largest_received,
      QuicPacketNumber smallest_received,
      QuicPacketNumber least_unacked);
  std::unique_ptr<QuicReceivedPacket> MakeRstPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicStreamId stream_id,
      QuicRstStreamErrorCode error_code);

  std::unique_ptr<QuicReceivedPacket> MakeRstPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicStreamId stream_id,
      QuicRstStreamErrorCode error_code,
      size_t bytes_written);

  std::unique_ptr<QuicReceivedPacket> MakeAckAndRstPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicStreamId stream_id,
      QuicRstStreamErrorCode error_code,
      QuicPacketNumber largest_received,
      QuicPacketNumber smallest_received,
      QuicPacketNumber least_unacked,
      bool send_feedback);
  std::unique_ptr<QuicReceivedPacket> MakeAckAndRstPacket(
      QuicPacketNumber num,
      bool include_version,
      QuicStreamId stream_id,
      QuicRstStreamErrorCode error_code,
      QuicPacketNumber largest_received,
      QuicPacketNumber smallest_received,
      QuicPacketNumber least_unacked,
      bool send_feedback,
      size_t bytes_written);
  std::unique_ptr<QuicReceivedPacket> MakeAckAndConnectionClosePacket(
      QuicPacketNumber num,
      bool include_version,
      QuicTime::Delta delta_time_largest_observed,
      QuicPacketNumber largest_received,
      QuicPacketNumber smallest_received,
      QuicPacketNumber least_unacked,
      QuicErrorCode quic_error,
      const std::string& quic_error_details);
  std::unique_ptr<QuicReceivedPacket> MakeConnectionClosePacket(
      QuicPacketNumber num,
      bool include_version,
      QuicErrorCode quic_error,
      const std::string& quic_error_details);
  std::unique_ptr<QuicReceivedPacket> MakeGoAwayPacket(
      QuicPacketNumber num,
      QuicErrorCode error_code,
      std::string reason_phrase);
  std::unique_ptr<QuicReceivedPacket> MakeAckPacket(
      QuicPacketNumber packet_number,
      QuicPacketNumber largest_received,
      QuicPacketNumber smallest_received,
      QuicPacketNumber least_unacked,
      bool send_feedback);
  std::unique_ptr<QuicReceivedPacket> MakeAckPacket(
      QuicPacketNumber packet_number,
      QuicPacketNumber largest_received,
      QuicPacketNumber smallest_received,
      QuicPacketNumber least_unacked,
      bool send_feedback,
      QuicTime::Delta ack_delay_time);
  std::unique_ptr<QuicReceivedPacket> MakeDataPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      QuicStringPiece data);
  std::unique_ptr<QuicReceivedPacket> MakeForceHolDataPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicStreamOffset* offset,
      QuicStringPiece data);
  std::unique_ptr<QuicReceivedPacket> MakeMultipleDataFramesPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      const std::vector<std::string>& data_writes);
  std::unique_ptr<QuicReceivedPacket> MakeAckAndDataPacket(
      QuicPacketNumber packet_number,
      bool include_version,
      QuicStreamId stream_id,
      QuicPacketNumber largest_received,
      QuicPacketNumber smallest_received,
      QuicPacketNumber least_unacked,
      bool fin,
      QuicStreamOffset offset,
      QuicStringPiece data);

  std::unique_ptr<QuicReceivedPacket>
  MakeRequestHeadersAndMultipleDataFramesPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      QuicStreamId parent_stream_id,
      QuicStreamOffset* header_stream_offset,
      size_t* spdy_headers_frame_length,
      const std::vector<std::string>& data_writes);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<QuicReceivedPacket> MakeRequestHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length);

  std::unique_ptr<QuicReceivedPacket> MakeRequestHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length,
      QuicStreamOffset* offset);

  // Saves the serialized QUIC stream data in |stream_data|.
  std::unique_ptr<QuicReceivedPacket> MakeRequestHeadersPacketAndSaveData(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length,
      QuicStreamOffset* offset,
      std::string* stream_data);

  std::unique_ptr<QuicReceivedPacket> MakeRequestHeadersAndRstPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyPriority priority,
      spdy::SpdyHeaderBlock headers,
      QuicStreamId parent_stream_id,
      size_t* spdy_headers_frame_length,
      QuicStreamOffset* header_stream_offset,
      QuicRstStreamErrorCode error_code,
      size_t bytes_written);

  // Convenience method for calling MakeRequestHeadersPacket with nullptr for
  // |spdy_headers_frame_length|.
  std::unique_ptr<QuicReceivedPacket>
  MakeRequestHeadersPacketWithOffsetTracking(QuicPacketNumber packet_number,
                                             QuicStreamId stream_id,
                                             bool should_include_version,
                                             bool fin,
                                             spdy::SpdyPriority priority,
                                             spdy::SpdyHeaderBlock headers,
                                             QuicStreamId parent_stream_id,
                                             QuicStreamOffset* offset);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<QuicReceivedPacket> MakePushPromisePacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      QuicStreamId promised_stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyHeaderBlock headers,
      size_t* spdy_headers_frame_length,
      QuicStreamOffset* offset);

  // If |spdy_headers_frame_length| is non-null, it will be set to the size of
  // the SPDY headers frame created for this packet.
  std::unique_ptr<QuicReceivedPacket> MakeResponseHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyHeaderBlock headers,
      size_t* spdy_headers_frame_length,
      QuicStreamOffset* offset);

  std::unique_ptr<QuicReceivedPacket> MakeResponseHeadersPacket(
      QuicPacketNumber packet_number,
      QuicStreamId stream_id,
      bool should_include_version,
      bool fin,
      spdy::SpdyHeaderBlock headers,
      size_t* spdy_headers_frame_length);

  // Convenience method for calling MakeResponseHeadersPacket with nullptr for
  // |spdy_headers_frame_length|.
  std::unique_ptr<QuicReceivedPacket>
  MakeResponseHeadersPacketWithOffsetTracking(QuicPacketNumber packet_number,
                                              QuicStreamId stream_id,
                                              bool should_include_version,
                                              bool fin,
                                              spdy::SpdyHeaderBlock headers,
                                              QuicStreamOffset* offset);

  // Creates a packet containing the initial SETTINGS frame, and saves the
  // headers stream offset into |offset|.
  std::unique_ptr<QuicReceivedPacket> MakeInitialSettingsPacket(
      QuicPacketNumber packet_number,
      QuicStreamOffset* offset);

  // Same as above, but also saves the serialized QUIC stream data in
  // |stream_data|.
  std::unique_ptr<QuicReceivedPacket> MakeInitialSettingsPacketAndSaveData(
      QuicPacketNumber packet_number,
      QuicStreamOffset* offset,
      std::string* stream_data);

  std::unique_ptr<QuicReceivedPacket> MakePriorityPacket(
      QuicPacketNumber packet_number,
      bool should_include_version,
      QuicStreamId id,
      QuicStreamId parent_stream_id,
      spdy::SpdyPriority priority,
      QuicStreamOffset* offset);

  std::unique_ptr<QuicReceivedPacket> MakeAckAndMultiplePriorityFramesPacket(
      QuicPacketNumber packet_number,
      bool should_include_version,
      QuicPacketNumber largest_received,
      QuicPacketNumber smallest_received,
      QuicPacketNumber least_unacked,
      const std::vector<Http2StreamDependency>& priority_frames,
      QuicStreamOffset* offset);

  void SetEncryptionLevel(EncryptionLevel level);

  void SetLongHeaderType(QuicLongHeaderType type);

  spdy::SpdyHeaderBlock GetRequestHeaders(const std::string& method,
                                          const std::string& scheme,
                                          const std::string& path);

  spdy::SpdyHeaderBlock ConnectRequestHeaders(const std::string& host_port);

  spdy::SpdyHeaderBlock GetResponseHeaders(const std::string& status);

  spdy::SpdyHeaderBlock GetResponseHeaders(const std::string& status,
                                           const std::string& alt_svc);

 private:
  std::unique_ptr<QuicReceivedPacket> MakePacket(const QuicPacketHeader& header,
                                                 const QuicFrame& frame);
  std::unique_ptr<QuicReceivedPacket> MakeMultipleFramesPacket(
      const QuicPacketHeader& header,
      const QuicFrames& frames);

  void InitializeHeader(QuicPacketNumber packet_number,
                        bool should_include_version);

  spdy::SpdySerializedFrame MakeSpdyHeadersFrame(QuicStreamId stream_id,
                                                 bool fin,
                                                 spdy::SpdyPriority priority,
                                                 spdy::SpdyHeaderBlock headers,
                                                 QuicStreamId parent_stream_id);

  bool ShouldIncludeVersion(bool include_version) const;

  QuicPacketNumberLength GetPacketNumberLength() const;

  QuicTransportVersion version_;
  QuicConnectionId connection_id_;
  MockClock* clock_;  // Owned by QuicStreamFactory.
  std::string host_;
  spdy::SpdyFramer spdy_request_framer_;
  spdy::SpdyFramer spdy_response_framer_;
  MockRandom random_generator_;
  QuicPacketHeader header_;
  Perspective perspective_;
  EncryptionLevel encryption_level_;
  QuicLongHeaderType long_header_type_;

  // If true, generated request headers will include non-default HTTP2 stream
  // dependency info.
  bool client_headers_include_h2_stream_dependency_;

  DISALLOW_COPY_AND_ASSIGN(QuicTestPacketMaker);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_CHROMIUM_QUIC_TEST_PACKET_MAKER_H_
