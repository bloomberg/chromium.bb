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
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;
using std::vector;

namespace net {
namespace test {

class QuicStreamFactoryPeer {
 public:
  static QuicCryptoClientConfig* GetOrCreateCryptoConfig(
      QuicStreamFactory* factory,
      const HostPortProxyPair& host_port_proxy_pair) {
    return factory->GetOrCreateCryptoConfig(host_port_proxy_pair);
  }

  static bool HasActiveSession(QuicStreamFactory* factory,
                               const HostPortProxyPair& host_port_proxy_pair) {
    return factory->HasActiveSession(host_port_proxy_pair);
  }

  static QuicClientSession* GetActiveSession(
      QuicStreamFactory* factory,
      const HostPortProxyPair& host_port_proxy_pair) {
    DCHECK(factory->HasActiveSession(host_port_proxy_pair));
    return factory->active_sessions_[host_port_proxy_pair];
  }

  static bool IsLiveSession(QuicStreamFactory* factory,
                            QuicClientSession* session) {
    for (QuicStreamFactory::SessionSet::iterator it =
             factory->all_sessions_.begin();
         it != factory->all_sessions_.end(); ++it) {
      if (*it == session)
        return true;
    }
    return false;
  }
};

class QuicStreamFactoryTest : public ::testing::Test {
 protected:
  QuicStreamFactoryTest()
      : random_generator_(0),
        clock_(new MockClock()),
        factory_(&host_resolver_, &socket_factory_,
                 base::WeakPtr<HttpServerProperties>(),
                 &crypto_client_stream_factory_,
                 &random_generator_, clock_, kDefaultMaxPacketSize),
        host_port_proxy_pair_(HostPortPair("www.google.com", 443),
                              ProxyServer::Direct()),
        is_https_(false),
        cert_verifier_(CertVerifier::CreateDefault()) {
    factory_.set_require_confirmation(false);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstPacket(
      QuicPacketSequenceNumber num,
      QuicStreamId stream_id) {
    QuicPacketHeader header;
    header.public_header.guid = random_generator_.RandUint64();
    header.public_header.reset_flag = false;
    header.public_header.version_flag = true;
    header.packet_sequence_number = num;
    header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
    header.entropy_flag = false;
    header.fec_flag = false;
    header.fec_group = 0;

    QuicRstStreamFrame rst(stream_id, QUIC_STREAM_CANCELLED);
    return scoped_ptr<QuicEncryptedPacket>(
        ConstructPacket(header, QuicFrame(&rst)));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketSequenceNumber largest_received,
      QuicPacketSequenceNumber least_unacked) {
    QuicPacketHeader header;
    header.public_header.guid = random_generator_.RandUint64();
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

    QuicFramer framer(QuicSupportedVersions(), QuicTime::Zero(), false);
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
    header.public_header.guid = random_generator_.RandUint64();
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
    QuicFramer framer(QuicSupportedVersions(), QuicTime::Zero(), false);
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

TEST_F(QuicStreamFactoryTest, FailedCreate) {
  MockConnect connect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  DeterministicSocketData socket_data(NULL, 0, NULL, 0);
  socket_data.set_connect_data(connect);
  socket_factory_.AddSocketDataProvider(&socket_data);
  socket_data.StopAfter(1);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, is_https_,
                                            cert_verifier_.get(), net_log_,
                                            callback_.callback()));

  EXPECT_EQ(ERR_ADDRESS_IN_USE, callback_.WaitForResult());
}

TEST_F(QuicStreamFactoryTest, Goaway) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  DeterministicSocketData socket_data(reads, arraysize(reads), NULL, 0);
  socket_data.StopAfter(1);
  socket_factory_.AddSocketDataProvider(&socket_data);
  DeterministicSocketData socket_data2(reads, arraysize(reads), NULL, 0);
  socket_data2.StopAfter(1);
  socket_factory_.AddSocketDataProvider(&socket_data2);

  QuicStreamRequest request(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request.Request(host_port_proxy_pair_, is_https_,
                                            cert_verifier_.get(), net_log_,
                                            callback_.callback()));

  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream = request.ReleaseStream();
  EXPECT_TRUE(stream.get());

  // Mark the session as going away.  Ensure that while it is still alive
  // that it is no longer active.
  QuicClientSession* session = QuicStreamFactoryPeer::GetActiveSession(
      &factory_, host_port_proxy_pair_);
  factory_.OnSessionGoingAway(session);
  EXPECT_EQ(true, QuicStreamFactoryPeer::IsLiveSession(&factory_, session));
  EXPECT_FALSE(QuicStreamFactoryPeer::HasActiveSession(&factory_,
                                                       host_port_proxy_pair_));
  EXPECT_EQ(NULL, factory_.CreateIfSessionExists(host_port_proxy_pair_,
                                                 net_log_).get());

  // Create a new request for the same destination and verify that a
  // new session is created.
  QuicStreamRequest request2(&factory_);
  EXPECT_EQ(ERR_IO_PENDING, request2.Request(host_port_proxy_pair_, is_https_,
                                             cert_verifier_.get(), net_log_,
                                             callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  scoped_ptr<QuicHttpStream> stream2 = request2.ReleaseStream();
  EXPECT_TRUE(stream2.get());

  EXPECT_TRUE(QuicStreamFactoryPeer::HasActiveSession(&factory_,
                                                      host_port_proxy_pair_));
  EXPECT_NE(session,
            QuicStreamFactoryPeer::GetActiveSession(
                &factory_, host_port_proxy_pair_));
  EXPECT_EQ(true, QuicStreamFactoryPeer::IsLiveSession(&factory_, session));

  stream2.reset();
  stream.reset();

  EXPECT_TRUE(socket_data.at_read_eof());
  EXPECT_TRUE(socket_data.at_write_eof());
}

TEST_F(QuicStreamFactoryTest, MaxOpenStream) {
  MockRead reads[] = {
    MockRead(ASYNC, OK, 0)  // EOF
  };
  scoped_ptr<QuicEncryptedPacket> rst(ConstructRstPacket(1, 3));
  MockWrite writes[] = {
    MockWrite(ASYNC, rst->data(), rst->length(), 1),
  };
  DeterministicSocketData socket_data(reads, arraysize(reads),
                                      writes, arraysize(writes));
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
  EXPECT_TRUE(factory_.require_confirmation());

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

TEST_F(QuicStreamFactoryTest, OnCertAdded) {
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

  // Add a cert and verify that stream saw the event.
  factory_.OnCertAdded(NULL);
  EXPECT_EQ(ERR_CERT_DATABASE_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_FALSE(factory_.require_confirmation());

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

TEST_F(QuicStreamFactoryTest, OnCACertChanged) {
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

  // Change the CA cert and verify that stream saw the event.
  factory_.OnCACertChanged(NULL);
  EXPECT_EQ(ERR_CERT_DATABASE_CHANGED,
            stream->ReadResponseHeaders(callback_.callback()));
  EXPECT_FALSE(factory_.require_confirmation());

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

TEST_F(QuicStreamFactoryTest, SharedCryptoConfig) {
  vector<string> cannoncial_suffixes;
  cannoncial_suffixes.push_back(string(".c.youtube.com"));
  cannoncial_suffixes.push_back(string(".googlevideo.com"));

  for (unsigned i = 0; i < cannoncial_suffixes.size(); ++i) {
    string r1_host_name("r1");
    string r2_host_name("r2");
    r1_host_name.append(cannoncial_suffixes[i]);
    r2_host_name.append(cannoncial_suffixes[i]);

    HostPortProxyPair host_port_proxy_pair1(HostPortPair(r1_host_name, 80),
                                            ProxyServer::Direct());

    QuicCryptoClientConfig* crypto_config1 =
        QuicStreamFactoryPeer::GetOrCreateCryptoConfig(&factory_,
                                                       host_port_proxy_pair1);
    DCHECK(crypto_config1);
    QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config1->LookupOrCreate(host_port_proxy_pair1.first.host());
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r1_host_name);
    cached1->SetProofValid();

    HostPortProxyPair host_port_proxy_pair2(HostPortPair(r2_host_name, 80),
                                            ProxyServer::Direct());
    QuicCryptoClientConfig* crypto_config2 =
        QuicStreamFactoryPeer::GetOrCreateCryptoConfig(&factory_,
                                                       host_port_proxy_pair2);
    DCHECK(crypto_config2);
    QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config2->LookupOrCreate(host_port_proxy_pair2.first.host());
    EXPECT_EQ(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->proof_valid());
  }
}

TEST_F(QuicStreamFactoryTest, CryptoConfigWhenProofIsInvalid) {
  vector<string> cannoncial_suffixes;
  cannoncial_suffixes.push_back(string(".c.youtube.com"));
  cannoncial_suffixes.push_back(string(".googlevideo.com"));

  for (unsigned i = 0; i < cannoncial_suffixes.size(); ++i) {
    string r3_host_name("r3");
    string r4_host_name("r4");
    r3_host_name.append(cannoncial_suffixes[i]);
    r4_host_name.append(cannoncial_suffixes[i]);

    HostPortProxyPair host_port_proxy_pair1(HostPortPair(r3_host_name, 80),
                                            ProxyServer::Direct());

    QuicCryptoClientConfig* crypto_config1 =
        QuicStreamFactoryPeer::GetOrCreateCryptoConfig(&factory_,
                                                       host_port_proxy_pair1);
    DCHECK(crypto_config1);
    QuicCryptoClientConfig::CachedState* cached1 =
        crypto_config1->LookupOrCreate(host_port_proxy_pair1.first.host());
    EXPECT_FALSE(cached1->proof_valid());
    EXPECT_TRUE(cached1->source_address_token().empty());

    // Mutate the cached1 to have different data.
    // TODO(rtenneti): mutate other members of CachedState.
    cached1->set_source_address_token(r3_host_name);
    cached1->SetProofInvalid();

    HostPortProxyPair host_port_proxy_pair2(HostPortPair(r4_host_name, 80),
                                            ProxyServer::Direct());
    QuicCryptoClientConfig* crypto_config2 =
        QuicStreamFactoryPeer::GetOrCreateCryptoConfig(&factory_,
                                                       host_port_proxy_pair2);
    DCHECK(crypto_config2);
    QuicCryptoClientConfig::CachedState* cached2 =
        crypto_config2->LookupOrCreate(host_port_proxy_pair2.first.host());
    EXPECT_NE(cached1->source_address_token(), cached2->source_address_token());
    EXPECT_TRUE(cached2->source_address_token().empty());
    EXPECT_FALSE(cached2->proof_valid());
  }
}

}  // namespace test
}  // namespace net
