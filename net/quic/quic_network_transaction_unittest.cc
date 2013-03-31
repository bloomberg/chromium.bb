// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/mock_client_socket_pool_manager.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace {

// This is the expected return from a current server advertising QUIC.
static const char kQuicAlternateProtocolHttpHeader[] =
    "Alternate-Protocol: 443:quic/1\r\n\r\n";

// Returns a vector of NPN protocol strings for negotiating QUIC.
std::vector<std::string> QuicNextProtos() {
  std::vector<std::string> protos;
  protos.push_back("http/1.1");
  protos.push_back("quic/1");
  return protos;
}

}  // namespace

namespace net {
namespace test {

class QuicNetworkTransactionTest : public PlatformTest {
 protected:
  QuicNetworkTransactionTest()
      : clock_(new MockClock),
        ssl_config_service_(new SSLConfigServiceDefaults),
        proxy_service_(ProxyService::CreateDirect()),
        auth_handler_factory_(
            HttpAuthHandlerFactory::CreateDefault(&host_resolver_)) {
  }

  virtual void SetUp() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    MessageLoop::current()->RunUntilIdle();
  }

  virtual void TearDown() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    // Empty the current queue.
    MessageLoop::current()->RunUntilIdle();
    PlatformTest::TearDown();
    NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    MessageLoop::current()->RunUntilIdle();
    HttpStreamFactory::set_use_alternate_protocols(false);
    HttpStreamFactory::SetNextProtos(std::vector<std::string>());
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
    header.fec_flag = false;
    header.fec_entropy_flag = false;
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
    header.fec_entropy_flag = false;
    header.fec_group = 0;

    QuicAckFrame ack(largest_received, QuicTime::Zero(), least_unacked);

    QuicCongestionFeedbackFrame feedback;
    feedback.type = kTCP;
    feedback.tcp.accumulated_number_of_lost_packets = 0;
    feedback.tcp.receive_window = 256000;

    QuicFramer framer(kQuicVersion1,
                      QuicDecrypter::Create(kNULL),
                      QuicEncrypter::Create(kNULL),
                      QuicTime::Zero(),
                      false);
    QuicFrames frames;
    frames.push_back(QuicFrame(&ack));
    frames.push_back(QuicFrame(&feedback));
    scoped_ptr<QuicPacket> packet(
        framer.ConstructFrameDataPacket(header, frames).packet);
    return scoped_ptr<QuicEncryptedPacket>(
        framer.EncryptPacket(header.packet_sequence_number, *packet));
  }

  std::string GetRequestString(const std::string& method,
                               const std::string& path) {
    SpdyHeaderBlock headers;
    headers[":method"] = method;
    headers[":host"] = "www.google.com";
    headers[":path"] = path;
    headers[":scheme"] = "http";
    headers[":version"] = "HTTP/1.1";
    return SerializeHeaderBlock(headers);
  }

  std::string GetResponseString(const std::string& status,
                                const std::string& body) {
    SpdyHeaderBlock headers;
    headers[":status"] = status;
    headers[":version"] = "HTTP/1.1";
    headers["content-type"] = "text/plain";
    return SerializeHeaderBlock(headers) + body;
  }

  std::string SerializeHeaderBlock(const SpdyHeaderBlock& headers) {
    size_t len = SpdyFramer::GetSerializedLength(3, &headers);
    SpdyFrameBuilder builder(len);
    SpdyFramer::WriteHeaderBlock(&builder, 3, &headers);
    scoped_ptr<SpdyFrame> frame(builder.take());
    return std::string(frame->data(), len);
  }

  // Returns a newly created packet to send kData on stream 1.
  QuicEncryptedPacket* ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    InitializeHeader(sequence_number, should_include_version);
    QuicStreamFrame frame(3, fin, offset, data);
    return ConstructPacket(header_, QuicFrame(&frame)).release();
  }

  scoped_ptr<QuicEncryptedPacket> ConstructPacket(
      const QuicPacketHeader& header,
      const QuicFrame& frame) {
    QuicFramer framer(kQuicVersion1,
                      QuicDecrypter::Create(kNULL),
                      QuicEncrypter::Create(kNULL),
                      QuicTime::Zero(),
                      false);
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer.ConstructFrameDataPacket(header, frames).packet);
    return scoped_ptr<QuicEncryptedPacket>(
        framer.EncryptPacket(header.packet_sequence_number, *packet));
  }

  void InitializeHeader(QuicPacketSequenceNumber sequence_number,
                        bool should_include_version) {
    header_.public_header.guid = random_generator_.RandUint64();
    header_.public_header.reset_flag = false;
    header_.public_header.version_flag = should_include_version;
    header_.packet_sequence_number = sequence_number;
    header_.fec_group = 0;
    header_.entropy_flag = false;
    header_.fec_flag = false;
    header_.fec_entropy_flag = false;
  }

  void CreateSession() {
    params_.enable_quic = true;
    params_.quic_clock = clock_;
    params_.quic_random = &random_generator_;
    params_.client_socket_factory = &socket_factory_;
    params_.quic_crypto_client_stream_factory = &crypto_client_stream_factory_;
    params_.host_resolver = &host_resolver_;
    params_.cert_verifier = &cert_verifier_;
    params_.proxy_service = proxy_service_.get();
    params_.ssl_config_service = ssl_config_service_.get();
    params_.http_auth_handler_factory = auth_handler_factory_.get();
    params_.http_server_properties = &http_server_properties;

    session_ = new HttpNetworkSession(params_);
  }

  QuicPacketHeader header_;
  scoped_refptr<HttpNetworkSession> session_;
  MockClientSocketFactory socket_factory_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  MockClock* clock_;  // Owned by QuicStreamFactory after CreateSession.
  MockHostResolver host_resolver_;
  MockCertVerifier cert_verifier_;
  scoped_refptr<SSLConfigServiceDefaults> ssl_config_service_;
  scoped_ptr<ProxyService> proxy_service_;
  scoped_ptr<HttpAuthHandlerFactory> auth_handler_factory_;
  MockRandom random_generator_;
  HttpServerPropertiesImpl http_server_properties;
  HttpNetworkSession::Params params_;
};

TEST_F(QuicNetworkTransactionTest, ForceQuic) {
  params_.origin_port_to_force_quic_on = 80;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  scoped_ptr<QuicEncryptedPacket> data(
      ConstructDataPacket(1, true, true, 0, GetRequestString("GET", "/")));
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0));

  MockWrite quic_writes[] = {
    MockWrite(SYNCHRONOUS, data->data(), data->length()),
    MockWrite(SYNCHRONOUS, ack->data(), ack->length()),
  };

  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(
          1, false, true, 0, GetResponseString("200 OK", "hello!")));
  MockRead quic_reads[] = {
    MockRead(SYNCHRONOUS, resp->data(), resp->length()),
    MockRead(ASYNC, OK),  // EOF
  };

  DelayedSocketData quic_data(
      1,  // wait for one write to finish before reading.
      quic_reads, arraysize(quic_reads),
      quic_writes, arraysize(quic_writes));

  socket_factory_.AddSocketDataProvider(&quic_data);

  // The non-alternate protocol job needs to hang in order to guarantee that the
  // alternate-protocol job will "win".
  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider hanging_non_alternate_protocol_socket(
      NULL, 0, NULL, 0);
  hanging_non_alternate_protocol_socket.set_connect_data(
      never_finishing_connect);
  socket_factory_.AddSocketDataProvider(
      &hanging_non_alternate_protocol_socket);

  TestCompletionCallback callback;

  CreateSession();
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_EQ(HttpResponseInfo::CONNECTION_INFO_QUIC1_SPDY3,
            response->connection_info);

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
}

TEST_F(QuicNetworkTransactionTest, DoNotForceQuicForHttps) {
  // Attempt to "force" quic on 443, which will not be honored.
  params_.origin_port_to_force_quic_on = 443;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider data(
      data_reads, arraysize(data_reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  CreateSession();
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);
}

TEST_F(QuicNetworkTransactionTest, UseAlternateProtocolForQuic) {
  HttpStreamFactory::set_use_alternate_protocols(true);
  HttpStreamFactory::SetNextProtos(QuicNextProtos());

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead(kQuicAlternateProtocolHttpHeader),
    MockRead("hello world"),
    MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
    MockRead(ASYNC, OK)
  };

  StaticSocketDataProvider first_transaction(
      data_reads, arraysize(data_reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&first_transaction);


  scoped_ptr<QuicEncryptedPacket> data(
      ConstructDataPacket(1, true, true, 0, GetRequestString("GET", "/")));
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0));

  MockWrite quic_writes[] = {
    MockWrite(SYNCHRONOUS, data->data(), data->length()),
    MockWrite(SYNCHRONOUS, ack->data(), ack->length()),
  };

  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(
          1, false, true, 0, GetResponseString("200 OK", "hello!")));
  MockRead quic_reads[] = {
    MockRead(SYNCHRONOUS, resp->data(), resp->length()),
    MockRead(ASYNC, OK),  // EOF
  };

  DelayedSocketData quic_data(
      1,  // wait for one write to finish before reading.
      quic_reads, arraysize(quic_reads),
      quic_writes, arraysize(quic_writes));

  socket_factory_.AddSocketDataProvider(&quic_data);

  // The non-alternate protocol job needs to hang in order to guarantee that the
  // alternate-protocol job will "win".
  MockConnect never_finishing_connect(SYNCHRONOUS, ERR_IO_PENDING);
  StaticSocketDataProvider hanging_non_alternate_protocol_socket(
      NULL, 0, NULL, 0);
  hanging_non_alternate_protocol_socket.set_connect_data(
      never_finishing_connect);
  socket_factory_.AddSocketDataProvider(
      &hanging_non_alternate_protocol_socket);

  TestCompletionCallback callback;

  CreateSession();
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session_));

  rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_TRUE(response->was_fetched_via_spdy);
  EXPECT_TRUE(response->was_npn_negotiated);
  EXPECT_EQ("quic/1+spdy/3", response->npn_negotiated_protocol);

  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
}

TEST_F(QuicNetworkTransactionTest, DontUseAlternateProtocolForQuicHttps) {
  HttpStreamFactory::set_use_alternate_protocols(true);
  HttpStreamFactory::SetNextProtos(QuicNextProtos());

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-length: 11\r\n"),
    MockRead(kQuicAlternateProtocolHttpHeader),
    MockRead("hello world"),

    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-length: 6\r\n"),
    MockRead(kQuicAlternateProtocolHttpHeader),
    MockRead("hello!"),
  };

  StaticSocketDataProvider first_transaction(
      data_reads, arraysize(data_reads), NULL, 0);
  socket_factory_.AddSocketDataProvider(&first_transaction);
  SSLSocketDataProvider ssl(ASYNC, OK);
  socket_factory_.AddSSLSocketDataProvider(&ssl);

  TestCompletionCallback callback;

  CreateSession();
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(DEFAULT_PRIORITY, session_));

  int rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello world", response_data);

  trans.reset(new HttpNetworkTransaction(DEFAULT_PRIORITY, session_));

  rv = trans->Start(&request, callback.callback(), BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  response = trans->GetResponseInfo();
  ASSERT_TRUE(response != NULL);
  ASSERT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());
  EXPECT_FALSE(response->was_fetched_via_spdy);

  ASSERT_EQ(OK, ReadTransaction(trans.get(), &response_data));
  EXPECT_EQ("hello!", response_data);
}

}  // namespace test
}  // namespace net
