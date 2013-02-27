// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include "base/run_loop.h"
#include "base/string_util.h"
#include "net/base/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

class QuicStreamFactoryTest : public ::testing::Test {
 protected:
  QuicStreamFactoryTest()
      : clock_(new MockClock()),
        factory_(&host_resolver_, &socket_factory_,
                 &random_generator_, clock_),
        host_port_proxy_pair_(HostPortPair("www.google.com", 443),
                              ProxyServer::Direct()) {
  }

  scoped_ptr<QuicEncryptedPacket> ConstructChlo() {
    const std::string& host = host_port_proxy_pair_.first.host();
    scoped_ptr<QuicPacket> chlo(ConstructClientHelloPacket(0xDEADBEEF,
                                                           clock_,
                                                           &random_generator_,
                                                           host));
    QuicFramer framer(QuicDecrypter::Create(kNULL),
                      QuicEncrypter::Create(kNULL));
    return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(1, *chlo));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructShlo() {
    scoped_ptr<QuicPacket> shlo(ConstructHandshakePacket(0xDEADBEEF, kSHLO));
    QuicFramer framer(QuicDecrypter::Create(kNULL),
                      QuicEncrypter::Create(kNULL));
    return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(1, *shlo));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstPacket(
      QuicPacketSequenceNumber num,
      QuicStreamId stream_id) {
    QuicPacketHeader header;
    header.public_header.guid = 0xDEADBEEF;
    header.public_header.reset_flag = false;
    header.public_header.version_flag = false;
    header.packet_sequence_number = num;
    header.entropy_flag = false;
    header.fec_entropy_flag = false;
    header.fec_flag = false;
    header.fec_group = 0;

    QuicRstStreamFrame rst(stream_id, QUIC_NO_ERROR);
    return scoped_ptr<QuicEncryptedPacket>(
        ConstructPacket(header, QuicFrame(&rst)));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketSequenceNumber largest_received,
      QuicPacketSequenceNumber least_unacked) {
    QuicPacketHeader header;
    header.public_header.guid = 0xDEADBEEF;
    header.public_header.reset_flag = false;
    header.public_header.version_flag = false;
    header.packet_sequence_number = 2;
    header.entropy_flag = false;
    header.fec_entropy_flag = false;
    header.fec_flag = false;
    header.fec_group = 0;

    QuicAckFrame ack(largest_received, least_unacked);
    QuicCongestionFeedbackFrame feedback;
    feedback.type = kTCP;
    feedback.tcp.accumulated_number_of_lost_packets = 0;
    feedback.tcp.receive_window = 16000;

    QuicFramer framer(QuicDecrypter::Create(kNULL),
                      QuicEncrypter::Create(kNULL));
    QuicFrames frames;
    frames.push_back(QuicFrame(&ack));
    frames.push_back(QuicFrame(&feedback));
    scoped_ptr<QuicPacket> packet(
        framer.ConstructFrameDataPacket(header, frames).packet);
    return scoped_ptr<QuicEncryptedPacket>(
        framer.EncryptPacket(header.packet_sequence_number, *packet));
  }

  // Returns a newly created packet to send congestion feedback data.
  scoped_ptr<QuicEncryptedPacket> ConstructFeedbackPacket(
      QuicPacketSequenceNumber sequence_number) {
    QuicPacketHeader header;
    header.public_header.guid = 0xDEADBEEF;
    header.public_header.reset_flag = false;
    header.public_header.version_flag = false;
    header.packet_sequence_number = sequence_number;
    header.entropy_flag = false;
    header.fec_entropy_flag = false;
    header.fec_flag = false;
    header.fec_group = 0;

    QuicCongestionFeedbackFrame frame;
    frame.type = kTCP;
    frame.tcp.accumulated_number_of_lost_packets = 0;
    frame.tcp.receive_window = 16000;

    return scoped_ptr<QuicEncryptedPacket>(
        ConstructPacket(header, QuicFrame(&frame)));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructPacket(
      const QuicPacketHeader& header,
      const QuicFrame& frame) {
    QuicFramer framer(QuicDecrypter::Create(kNULL),
                      QuicEncrypter::Create(kNULL));
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer.ConstructFrameDataPacket(header, frames).packet);
    return scoped_ptr<QuicEncryptedPacket>(
        framer.EncryptPacket(header.packet_sequence_number, *packet));
  }

  MockHostResolver host_resolver_;
  MockClientSocketFactory socket_factory_;
  MockRandom random_generator_;
  MockClock* clock_;  // Owned by factory_.
  QuicStreamFactory factory_;
  HostPortProxyPair host_port_proxy_pair_;
  BoundNetLog net_log_;
  TestCompletionCallback callback_;
};

TEST_F(QuicStreamFactoryTest, CreateIfSessionExists) {
  EXPECT_EQ(NULL, factory_.CreateIfSessionExists(host_port_proxy_pair_,
                                                 net_log_).get());
}

TEST_F(QuicStreamFactoryTest, Create) {
  scoped_ptr<QuicEncryptedPacket> chlo(ConstructChlo());
  scoped_ptr<QuicEncryptedPacket> rst3(ConstructRstPacket(2, 3));
  scoped_ptr<QuicEncryptedPacket> rst5(ConstructRstPacket(3, 5));
  scoped_ptr<QuicEncryptedPacket> rst7(ConstructRstPacket(4, 7));
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, chlo->data(), chlo->length()),
    MockWrite(SYNCHRONOUS, rst3->data(), rst3->length()),
    MockWrite(SYNCHRONOUS, rst5->data(), rst5->length()),
    MockWrite(SYNCHRONOUS, rst7->data(), rst7->length()),
  };
  scoped_ptr<QuicEncryptedPacket> shlo(ConstructShlo());
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, shlo->data(), shlo->length()),
    MockRead(ASYNC, OK),  // EOF
  };
  StaticSocketDataProvider socket_data(reads, arraysize(reads),
                                       writes, arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, net_log_,
                                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();

  // Will reset stream 3.
  stream = factory_.CreateIfSessionExists(host_port_proxy_pair_, net_log_);
  EXPECT_TRUE(stream.get());

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(host_port_proxy_pair_, net_log_,
                                 callback_.callback()));
  stream = request2.ReleaseStream();  // Will reset stream 5.
  stream.reset();  // Will reset stream 7.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_F(QuicStreamFactoryTest, CreateError) {
  StaticSocketDataProvider socket_data(NULL, 0, NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  host_resolver_.rules()->AddSimulatedFailure("www.google.com");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, net_log_,
                                            callback_.callback()));

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_F(QuicStreamFactoryTest, CancelCreate) {
  scoped_ptr<QuicEncryptedPacket> chlo(ConstructChlo());
  scoped_ptr<QuicEncryptedPacket> rst3(ConstructRstPacket(2, 3));

  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, chlo->data(), chlo->length()),
    MockWrite(SYNCHRONOUS, rst3->data(), rst3->length()),
  };
  scoped_ptr<QuicEncryptedPacket> shlo(ConstructShlo());
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, shlo->data(), shlo->length()),
    MockRead(ASYNC, OK),  // EOF
  };
  StaticSocketDataProvider socket_data(reads, arraysize(reads),
                                       writes, arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);
  {
    QuicStreamRequest request(&factory_);
    EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, net_log_,
                                              callback_.callback()));
  }

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  scoped_ptr<QuicHttpStream> stream(
      factory_.CreateIfSessionExists(host_port_proxy_pair_, net_log_));
  EXPECT_TRUE(stream.get());
  stream.reset();

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_F(QuicStreamFactoryTest, CloseAllSessions) {
  scoped_ptr<QuicEncryptedPacket> chlo(ConstructChlo());
  scoped_ptr<QuicEncryptedPacket> rst3(ConstructRstPacket(2, 3));
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, chlo->data(), chlo->length()),
  };
  scoped_ptr<QuicEncryptedPacket> shlo(ConstructShlo());
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, shlo->data(), shlo->length()),
    MockRead(ASYNC, OK),  // EOF
  };
  StaticSocketDataProvider socket_data(reads, arraysize(reads),
                                       writes, arraysize(writes));
  socket_factory_.AddSocketDataProvider(&socket_data);

  MockWrite writes2[] = {
    MockWrite(SYNCHRONOUS, chlo->data(), chlo->length()),
    MockWrite(SYNCHRONOUS, rst3->data(), rst3->length()),
  };
  MockRead reads2[] = {
    MockRead(SYNCHRONOUS, shlo->data(), shlo->length()),
    MockRead(ASYNC, OK),  // EOF
  };
  StaticSocketDataProvider socket_data2(reads2, arraysize(reads2),
                                        writes2, arraysize(writes2));
  socket_factory_.AddSocketDataProvider(&socket_data2);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, net_log_,
                                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();

  // Close the session and verify that stream saw the error.
  factory_.CloseAllSessions(ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED,
            stream->ReadResponseHeaders(callback_.callback()));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request2.Request(host_port_proxy_pair_, net_log_,
                                             callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
}

}  // namespace test
}  // namespace net
