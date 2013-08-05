// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_factory.h"

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
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
                 &crypto_client_stream_factory_,
                 &random_generator_, clock_),
        host_port_proxy_pair_(HostPortPair("www.google.com", 443),
                              ProxyServer::Direct()),
        is_https_(false),
        cert_verifier_(CertVerifier::CreateDefault()) {
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstPacket(
      QuicPacketSequenceNumber num,
      QuicStreamId stream_id) {
    QuicPacketHeader header;
    header.public_header.guid = 0xDEADBEEF;
    header.public_header.reset_flag = false;
    header.public_header.version_flag = true;
    header.packet_sequence_number = num;
    header.entropy_flag = false;
    header.fec_flag = false;
    header.fec_group = 0;

    QuicRstStreamFrame rst(stream_id, QUIC_STREAM_NO_ERROR);
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
    header.fec_flag = false;
    header.fec_group = 0;

    QuicAckFrame ack(largest_received, QuicTime::Zero(), least_unacked);
    QuicCongestionFeedbackFrame feedback;
    feedback.type = kTCP;
    feedback.tcp.accumulated_number_of_lost_packets = 0;
    feedback.tcp.receive_window = 16000;

    QuicFramer framer(QuicVersionMax(), QuicTime::Zero(), false);
    QuicFrames frames;
    frames.push_back(QuicFrame(&ack));
    frames.push_back(QuicFrame(&feedback));
    scoped_ptr<QuicPacket> packet(
        framer.BuildUnsizedDataPacket(header, frames).packet);
    return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(
        ENCRYPTION_NONE, header.packet_sequence_number, *packet));
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
    QuicFramer framer(QuicVersionMax(), QuicTime::Zero(), false);
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer.BuildUnsizedDataPacket(header, frames).packet);
    return scoped_ptr<QuicEncryptedPacket>(framer.EncryptPacket(
        ENCRYPTION_NONE, header.packet_sequence_number, *packet));
  }

  MockHostResolver host_resolver_;
  DeterministicMockClientSocketFactory socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockRandom random_generator_;
  MockClock* clock_;  // Owned by factory_.
  QuicStreamFactory factory_;
  HostPortProxyPair host_port_proxy_pair_;
  bool is_https_;
  scoped_ptr<CertVerifier> cert_verifier_;
  BoundNetLog net_log_;
  TestCompletionCallback callback_;
};

TEST_F(QuicStreamFactoryTest, CreateIfSessionExists) {
  EXPECT_EQ(NULL, factory_.CreateIfSessionExists(host_port_proxy_pair_,
                                                 net_log_).get());
}

TEST_F(QuicStreamFactoryTest, Create) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, is_https_,
                                            cert_verifier_.get(), net_log_,
                                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  // Will reset stream 3.
  stream = factory_.CreateIfSessionExists(host_port_proxy_pair_, net_log_);
  EXPECT_TRUE(stream.get());

  // TODO(rtenneti): We should probably have a tests that HTTP and HTTPS result
  // in streams on different sessions.
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(OK, request2.Request(host_port_proxy_pair_, is_https_,
                                 cert_verifier_.get(), net_log_,
                                 callback_.callback()));
  stream = request2.ReleaseStream();  // Will reset stream 5.
  stream.reset();  // Will reset stream 7.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_F(QuicStreamFactoryTest, MaxOpenStream) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  HttpRequestInfo request_info;
  std::vector<QuicHttpStream*> streams;
  // The MockCryptoClientStream sets max_open_streams to be
  // 2 * kDefaultMaxStreamsPerConnection.
  for (size_t i = 0; i < 2 * kDefaultMaxStreamsPerConnection; i++) {
    QuicStreamRequest request(&factory_);
    int rv = request.Request(host_port_proxy_pair_, is_https_,
                             cert_verifier_.get(), net_log_,
                             callback_.callback());
    if (i == 0) {
      EXPECT_EQ(ERR_IO_PENDING, rv);
      EXPECT_EQ(OK, callback_.WaitForResult());
    } else {
      EXPECT_EQ(OK, rv);
    }
    scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
    EXPECT_TRUE(stream);
    EXPECT_EQ(OK, stream->InitializeStream(
        &request_info, DEFAULT_PRIORITY, net_log_, CompletionCallback()));
    streams.push_back(stream.release());
  }

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(OK, request.Request(host_port_proxy_pair_, is_https_,
                                cert_verifier_.get(), net_log_,
                                CompletionCallback()));
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream);
  EXPECT_EQ(ERR_IO_PENDING, stream->InitializeStream(
        &request_info, DEFAULT_PRIORITY, net_log_, callback_.callback()));

  // Close the first stream.
  streams.front()->Close(false);

  ASSERT_TRUE(callback_.have_result());

  EXPECT_EQ(OK, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  STLDeleteElements(&streams);
}

TEST_F(QuicStreamFactoryTest, CreateError) {
  DeterministicSocketData socket_data(NULL, 0, NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);

  host_resolver_.rules()->AddSimulatedFailure("www.google.com");

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, is_https_,
                                            cert_verifier_.get(), net_log_,
                                            callback_.callback()));

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback_.WaitForResult());

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_F(QuicStreamFactoryTest, CancelCreate) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  {
    QuicStreamRequest request(&factory_);
    EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, is_https_,
                                              cert_verifier_.get(), net_log_,
                                              callback_.callback()));
  }

  socket_data.StopAfter(1);
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
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  MockRead reads2[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data2(reads2, arraysize(reads2), NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, is_https_,
                                            cert_verifier_.get(), net_log_,
                                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info,
                                         DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  // Close the session and verify that stream saw the error.
  factory_.CloseAllSessions(ERR_INTERNET_DISCONNECTED);
  EXPECT_EQ(ERR_INTERNET_DISCONNECTED,
            stream->ReadResponseHeaders(callback_.callback()));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request2.Request(host_port_proxy_pair_, is_https_,
                                             cert_verifier_.get(), net_log_,
                                             callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  stream = request2.ReleaseStream();
  stream.reset();  // Will reset stream 3.

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
  EXPECT_TRUE(socket_data2.at_read_eof());
  EXPECT_TRUE(socket_data2.at_write_eof());
}

TEST_F(QuicStreamFactoryTest, OnIPAddressChanged) {
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  MockRead reads2[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData socket_data2(reads2, arraysize(reads2), NULL, 0);
  socket_factory_.AddSocketDataProvider(&socket_data2);
  socket_data2.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, is_https_,
                                            cert_verifier_.get(), net_log_,
                                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  HttpRequestInfo request_info;
  EXPECT_EQ(OK, stream->InitializeStream(&request_info,
                                         DEFAULT_PRIORITY,
                                         net_log_, CompletionCallback()));

  // Change the IP address and verify that stream saw the error.
  factory_.OnIPAddressChanged();
  EXPECT_EQ(ERR_NETWORK_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));

  // Now attempting to request a stream to the same origin should create
  // a new session.

  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request2.Request(host_port_proxy_pair_, is_https_,
                                             cert_verifier_.get(), net_log_,
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
