// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_stream.h"

#include <stdint.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/net_errors.h"
#include "net/base/socket_performance_watcher.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/http/transport_security_state.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_server_info.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_client_stream.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_packet_reader.h"
#include "net/quic/quic_write_blocked_list.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_packet_maker.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/test_task_runner.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net {
namespace test {
namespace {

const char kUploadData[] = "Really nifty data!";
const char kDefaultServerHostName[] = "www.google.com";
const uint16_t kDefaultServerPort = 80;

class TestQuicConnection : public QuicConnection {
 public:
  TestQuicConnection(const QuicVersionVector& versions,
                     QuicConnectionId connection_id,
                     IPEndPoint address,
                     QuicChromiumConnectionHelper* helper,
                     QuicPacketWriter* writer)
      : QuicConnection(connection_id,
                       address,
                       helper,
                       writer,
                       true /* owns_writer */,
                       Perspective::IS_CLIENT,
                       versions) {}

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }
};

// Subclass of QuicHttpStream that closes itself when the first piece of data
// is received.
class AutoClosingStream : public QuicHttpStream {
 public:
  explicit AutoClosingStream(
      const base::WeakPtr<QuicChromiumClientSession>& session)
      : QuicHttpStream(session) {}

  void OnHeadersAvailable(const SpdyHeaderBlock& headers,
                          size_t frame_len) override {
    Close(false);
  }

  void OnDataAvailable() override { Close(false); }
};

}  // namespace

class QuicHttpStreamPeer {
 public:
  static QuicChromiumClientStream* GetQuicChromiumClientStream(
      QuicHttpStream* stream) {
    return stream->stream_;
  }

  static bool WasHandshakeConfirmed(QuicHttpStream* stream) {
    return stream->was_handshake_confirmed_;
  }

  static void SetHandshakeConfirmed(QuicHttpStream* stream, bool confirmed) {
    stream->was_handshake_confirmed_ = confirmed;
  }
};

class QuicHttpStreamTest : public ::testing::TestWithParam<QuicVersion> {
 protected:
  static const bool kFin = true;
  static const bool kIncludeVersion = true;
  static const bool kIncludeCongestionFeedback = true;

  // Holds a packet to be written to the wire, and the IO mode that should
  // be used by the mock socket when performing the write.
  struct PacketToWrite {
    PacketToWrite(IoMode mode, QuicEncryptedPacket* packet)
        : mode(mode), packet(packet) {}
    PacketToWrite(IoMode mode, int rv) : mode(mode), packet(nullptr), rv(rv) {}
    IoMode mode;
    QuicEncryptedPacket* packet;
    int rv;
  };

  QuicHttpStreamTest()
      : net_log_(BoundNetLog()),
        use_closing_stream_(false),
        crypto_config_(CryptoTestUtils::ProofVerifierForTesting()),
        read_buffer_(new IOBufferWithSize(4096)),
        connection_id_(2),
        stream_id_(kClientDataStreamId1),
        maker_(GetParam(), connection_id_, &clock_, kDefaultServerHostName),
        random_generator_(0) {
    IPAddressNumber ip;
    CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
    peer_addr_ = IPEndPoint(ip, 443);
    self_addr_ = IPEndPoint(ip, 8435);
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(20));
  }

  ~QuicHttpStreamTest() {
    session_->CloseSessionOnError(ERR_ABORTED, QUIC_INTERNAL_ERROR);
    for (size_t i = 0; i < writes_.size(); i++) {
      delete writes_[i].packet;
    }
  }

  // Adds a packet to the list of expected writes.
  void AddWrite(scoped_ptr<QuicEncryptedPacket> packet) {
    writes_.push_back(PacketToWrite(SYNCHRONOUS, packet.release()));
  }

  void AddWrite(IoMode mode, int rv) {
    writes_.push_back(PacketToWrite(mode, rv));
  }

  // Returns the packet to be written at position |pos|.
  QuicEncryptedPacket* GetWrite(size_t pos) { return writes_[pos].packet; }

  bool AtEof() {
    return socket_data_->AllReadDataConsumed() &&
           socket_data_->AllWriteDataConsumed();
  }

  void ProcessPacket(scoped_ptr<QuicEncryptedPacket> packet) {
    connection_->ProcessUdpPacket(self_addr_, peer_addr_, *packet);
  }

  // Configures the test fixture to use the list of expected writes.
  void Initialize() {
    mock_writes_.reset(new MockWrite[writes_.size()]);
    for (size_t i = 0; i < writes_.size(); i++) {
      if (writes_[i].packet == nullptr) {
        mock_writes_[i] = MockWrite(writes_[i].mode, writes_[i].rv, i);
      } else {
        mock_writes_[i] = MockWrite(writes_[i].mode, writes_[i].packet->data(),
                                    writes_[i].packet->length());
      }
    };

    socket_data_.reset(new StaticSocketDataProvider(
        nullptr, 0, mock_writes_.get(), writes_.size()));

    MockUDPClientSocket* socket =
        new MockUDPClientSocket(socket_data_.get(), net_log_.net_log());
    socket->Connect(peer_addr_);
    runner_ = new TestTaskRunner(&clock_);
    send_algorithm_ = new MockSendAlgorithm();
    EXPECT_CALL(*send_algorithm_, InRecovery()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, InSlowStart()).WillRepeatedly(Return(false));
    EXPECT_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*send_algorithm_, RetransmissionDelay())
        .WillRepeatedly(Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, GetCongestionWindow())
        .WillRepeatedly(Return(kMaxPacketSize));
    EXPECT_CALL(*send_algorithm_, PacingRate())
        .WillRepeatedly(Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _))
        .WillRepeatedly(Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate())
        .WillRepeatedly(Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
    helper_.reset(new QuicChromiumConnectionHelper(runner_.get(), &clock_,
                                                   &random_generator_));
    connection_ = new TestQuicConnection(
        SupportedVersions(GetParam()), connection_id_, peer_addr_,
        helper_.get(), new QuicDefaultPacketWriter(socket));
    connection_->set_visitor(&visitor_);
    connection_->SetSendAlgorithm(send_algorithm_);
    session_.reset(new QuicChromiumClientSession(
        connection_, scoped_ptr<DatagramClientSocket>(socket),
        /*stream_factory=*/nullptr, &crypto_client_stream_factory_, &clock_,
        &transport_security_state_, make_scoped_ptr((QuicServerInfo*)nullptr),
        QuicServerId(kDefaultServerHostName, kDefaultServerPort,
                     PRIVACY_MODE_DISABLED),
        kQuicYieldAfterPacketsRead,
        QuicTime::Delta::FromMilliseconds(kQuicYieldAfterDurationMilliseconds),
        /*cert_verify_flags=*/0, DefaultQuicConfig(), &crypto_config_,
        "CONNECTION_UNKNOWN", base::TimeTicks::Now(), &promised_by_url_,
        base::ThreadTaskRunnerHandle::Get().get(),
        /*socket_performance_watcher=*/nullptr, nullptr));
    session_->Initialize();
    session_->GetCryptoStream()->CryptoConnect();
    EXPECT_TRUE(session_->IsCryptoHandshakeConfirmed());
    stream_.reset(use_closing_stream_
                      ? new AutoClosingStream(session_->GetWeakPtr())
                      : new QuicHttpStream(session_->GetWeakPtr()));
  }

  void SetRequest(const std::string& method,
                  const std::string& path,
                  RequestPriority priority) {
    request_headers_ = maker_.GetRequestHeaders(method, "http", path);
  }

  void SetResponse(const std::string& status, const std::string& body) {
    response_headers_ = maker_.GetResponseHeaders(status);
    response_data_ = body;
  }

  scoped_ptr<QuicEncryptedPacket> ConstructDataPacket(
      QuicPacketNumber packet_number,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    return maker_.MakeDataPacket(packet_number, stream_id_,
                                 should_include_version, fin, offset, data);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRequestHeadersPacket(
      QuicPacketNumber packet_number,
      bool fin,
      RequestPriority request_priority,
      size_t* spdy_headers_frame_length) {
    SpdyPriority priority =
        ConvertRequestPriorityToQuicPriority(request_priority);
    return maker_.MakeRequestHeadersPacket(
        packet_number, stream_id_, kIncludeVersion, fin, priority,
        request_headers_, spdy_headers_frame_length);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructResponseHeadersPacket(
      QuicPacketNumber packet_number,
      bool fin,
      size_t* spdy_headers_frame_length) {
    return maker_.MakeResponseHeadersPacket(
        packet_number, stream_id_, !kIncludeVersion, fin, response_headers_,
        spdy_headers_frame_length);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstStreamPacket(
      QuicPacketNumber packet_number) {
    return maker_.MakeRstPacket(
        packet_number, true, stream_id_,
        AdjustErrorForVersion(QUIC_RST_ACKNOWLEDGEMENT, GetParam()));
  }

  scoped_ptr<QuicEncryptedPacket> ConstructRstStreamCancelledPacket(
      QuicPacketNumber packet_number) {
    return maker_.MakeRstPacket(packet_number, !kIncludeVersion, stream_id_,
                                QUIC_STREAM_CANCELLED);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckAndRstStreamPacket(
      QuicPacketNumber packet_number) {
    return maker_.MakeAckAndRstPacket(packet_number, !kIncludeVersion,
                                      stream_id_, QUIC_STREAM_CANCELLED, 2, 1,
                                      !kIncludeCongestionFeedback);
  }

  scoped_ptr<QuicEncryptedPacket> ConstructAckPacket(
      QuicPacketNumber packet_number,
      QuicPacketNumber largest_received,
      QuicPacketNumber least_unacked) {
    return maker_.MakeAckPacket(packet_number, largest_received, least_unacked,
                                !kIncludeCongestionFeedback);
  }

  BoundNetLog net_log_;
  bool use_closing_stream_;
  MockSendAlgorithm* send_algorithm_;
  scoped_refptr<TestTaskRunner> runner_;
  scoped_ptr<MockWrite[]> mock_writes_;
  MockClock clock_;
  TestQuicConnection* connection_;
  scoped_ptr<QuicChromiumConnectionHelper> helper_;
  testing::StrictMock<MockConnectionVisitor> visitor_;
  scoped_ptr<QuicHttpStream> stream_;
  TransportSecurityState transport_security_state_;
  scoped_ptr<QuicChromiumClientSession> session_;
  QuicCryptoClientConfig crypto_config_;
  TestCompletionCallback callback_;
  HttpRequestInfo request_;
  HttpRequestHeaders headers_;
  HttpResponseInfo response_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  SpdyHeaderBlock request_headers_;
  SpdyHeaderBlock response_headers_;
  std::string request_data_;
  std::string response_data_;
  QuicPromisedByUrlMap promised_by_url_;

 private:
  const QuicConnectionId connection_id_;
  const QuicStreamId stream_id_;
  QuicTestPacketMaker maker_;
  IPEndPoint self_addr_;
  IPEndPoint peer_addr_;
  MockRandom random_generator_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  scoped_ptr<StaticSocketDataProvider> socket_data_;
  std::vector<PacketToWrite> writes_;
};

INSTANTIATE_TEST_CASE_P(Version,
                        QuicHttpStreamTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicHttpStreamTest, RenewStreamForAuth) {
  Initialize();
  EXPECT_EQ(nullptr, stream_->RenewStreamForAuth());
}

TEST_P(QuicHttpStreamTest, CanReuseConnection) {
  Initialize();
  EXPECT_FALSE(stream_->CanReuseConnection());
}

TEST_P(QuicHttpStreamTest, GetRequest) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_header_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY,
                                         &spdy_request_header_frame_length));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));

  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));

  SetResponse("404 Not Found", std::string());
  size_t spdy_response_header_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, kFin, &spdy_response_header_frame_length));

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(404, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));
  EXPECT_FALSE(response_.response_time.is_null());
  EXPECT_FALSE(response_.request_time.is_null());

  // There is no body, so this should return immediately.
  EXPECT_EQ(0,
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_header_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_header_frame_length),
            stream_->GetTotalReceivedBytes());
}

// Regression test for http://crbug.com/288128
TEST_P(QuicHttpStreamTest, GetRequestLargeResponse) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));

  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));

  SpdyHeaderBlock headers;
  headers[":status"] = "200 OK";
  headers[":version"] = "HTTP/1.1";
  headers["content-type"] = "text/plain";
  headers["big6"] = std::string(1000, 'x');  // Lots of x's.

  response_headers_ = headers;
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, kFin, &spdy_response_headers_frame_length));

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  EXPECT_EQ(0,
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length),
            stream_->GetTotalReceivedBytes());
}

// Regression test for http://crbug.com/409101
TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendRequest) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));

  session_->connection()->CloseConnection(QUIC_NO_ERROR, true);

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  EXPECT_EQ(0, stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, LogGranularQuicConnectionError) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  AddWrite(ConstructAckAndRstStreamPacket(2));
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));
  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));

  EXPECT_TRUE(QuicHttpStreamPeer::WasHandshakeConfirmed(stream_.get()));
  stream_->OnClose(QUIC_PEER_GOING_AWAY);

  NetErrorDetails details;
  EXPECT_EQ(QUIC_NO_ERROR, details.quic_connection_error);
  stream_->PopulateNetErrorDetails(&details);
  EXPECT_EQ(QUIC_PEER_GOING_AWAY, details.quic_connection_error);
}

TEST_P(QuicHttpStreamTest, DoNotLogGranularQuicErrorIfHandshakeNotConfirmed) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  AddWrite(ConstructAckAndRstStreamPacket(2));
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));
  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));

  // The test setup defaults handshake to be confirmed. Manually set
  // it to be not confirmed.
  // Granular errors shouldn't be reported if handshake not confirmed.
  QuicHttpStreamPeer::SetHandshakeConfirmed(stream_.get(), false);

  EXPECT_FALSE(QuicHttpStreamPeer::WasHandshakeConfirmed(stream_.get()));
  stream_->OnClose(QUIC_PEER_GOING_AWAY);

  NetErrorDetails details;
  EXPECT_EQ(QUIC_NO_ERROR, details.quic_connection_error);
  stream_->PopulateNetErrorDetails(&details);
  EXPECT_EQ(QUIC_NO_ERROR, details.quic_connection_error);
}

// Regression test for http://crbug.com/409871
TEST_P(QuicHttpStreamTest, SessionClosedBeforeReadResponseHeaders) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));

  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  session_->connection()->CloseConnection(QUIC_NO_ERROR, true);

  EXPECT_NE(OK, stream_->ReadResponseHeaders(callback_.callback()));

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendPostRequest) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  AddWrite(ConstructDataPacket(2, kIncludeVersion, kFin, 0, kUploadData));
  AddWrite(ConstructAckPacket(3, 3, 1));

  Initialize();

  std::vector<scoped_ptr<UploadElementReader>> element_readers;
  element_readers.push_back(make_scoped_ptr(
      new UploadBytesElementReader(kUploadData, strlen(kUploadData))));
  ElementsUploadDataStream upload_data_stream(std::move(element_readers), 0);
  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(CompletionCallback()));

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack both packets in the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));

  // Send the response headers (but not the body).
  SetResponse("200 OK", std::string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have arrived, but they are delivered asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  ProcessPacket(ConstructDataPacket(3, false, kFin, 0, kResponseBody));
  // Since the body has already arrived, this should return immediately.
  EXPECT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length +
                                 strlen(kUploadData)),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody)),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequest) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t chunk_size = strlen(kUploadData);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  AddWrite(ConstructDataPacket(2, kIncludeVersion, !kFin, 0, kUploadData));
  AddWrite(
      ConstructDataPacket(3, kIncludeVersion, kFin, chunk_size, kUploadData));
  AddWrite(ConstructAckPacket(4, 3, 1));
  Initialize();

  ChunkedUploadDataStream upload_data_stream(0);
  upload_data_stream.AppendData(kUploadData, chunk_size, false);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  upload_data_stream.AppendData(kUploadData, chunk_size, true);
  EXPECT_EQ(OK, callback_.WaitForResult());

  // Ack both packets in the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));

  // Send the response headers (but not the body).
  SetResponse("200 OK", std::string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have arrived, but they are delivered asynchronously
  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  ProcessPacket(ConstructDataPacket(3, false, kFin, response_data_.length(),
                                    kResponseBody));

  // Since the body has already arrived, this should return immediately.
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length +
                                 strlen(kUploadData) * 2),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody)),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequestWithFinalEmptyDataPacket) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t chunk_size = strlen(kUploadData);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  AddWrite(ConstructDataPacket(2, kIncludeVersion, !kFin, 0, kUploadData));
  AddWrite(ConstructDataPacket(3, kIncludeVersion, kFin, chunk_size, ""));
  AddWrite(ConstructAckPacket(4, 3, 1));
  Initialize();

  ChunkedUploadDataStream upload_data_stream(0);
  upload_data_stream.AppendData(kUploadData, chunk_size, false);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  upload_data_stream.AppendData(nullptr, 0, true);
  EXPECT_EQ(OK, callback_.WaitForResult());

  ProcessPacket(ConstructAckPacket(1, 0, 0));

  // Send the response headers (but not the body).
  SetResponse("200 OK", std::string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have arrived, but they are delivered asynchronously
  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  ProcessPacket(ConstructDataPacket(3, false, kFin, response_data_.length(),
                                    kResponseBody));

  // The body has arrived, but it is delivered asynchronously
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length +
                                 strlen(kUploadData)),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody)),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SendChunkedPostRequestWithOneEmptyDataPacket) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  AddWrite(ConstructDataPacket(2, kIncludeVersion, kFin, 0, ""));
  AddWrite(ConstructAckPacket(3, 3, 1));
  Initialize();

  ChunkedUploadDataStream upload_data_stream(0);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  upload_data_stream.AppendData(nullptr, 0, true);
  EXPECT_EQ(OK, callback_.WaitForResult());

  ProcessPacket(ConstructAckPacket(1, 0, 0));

  // Send the response headers (but not the body).
  SetResponse("200 OK", std::string());
  size_t spdy_response_headers_frame_length;
  ProcessPacket(ConstructResponseHeadersPacket(
      2, !kFin, &spdy_response_headers_frame_length));

  // The headers have arrived, but they are delivered asynchronously
  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  ProcessPacket(ConstructDataPacket(3, false, kFin, response_data_.length(),
                                    kResponseBody));

  // The body has arrived, but it is delivered asynchronously
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  EXPECT_EQ(static_cast<int64_t>(spdy_response_headers_frame_length +
                                 strlen(kResponseBody)),
            stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, DestroyedEarly) {
  SetRequest("GET", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  AddWrite(ConstructAckAndRstStreamPacket(2));
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));
  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response with a body.
  SetResponse("404 OK", "hello world!");
  // In the course of processing this packet, the QuicHttpStream close itself.
  ProcessPacket(ConstructResponseHeadersPacket(2, kFin, nullptr));

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  // Zero since the stream is closed before processing the headers.
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, Priority) {
  SetRequest("GET", "/", MEDIUM);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, kFin, MEDIUM,
                                         &spdy_request_headers_frame_length));
  AddWrite(ConstructAckAndRstStreamPacket(2));
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, MEDIUM, net_log_,
                                          callback_.callback()));

  // Check that priority is highest.
  QuicChromiumClientStream* reliable_stream =
      QuicHttpStreamPeer::GetQuicChromiumClientStream(stream_.get());
  DCHECK(reliable_stream);
  DCHECK_EQ(kV3HighestPriority, reliable_stream->Priority());

  EXPECT_EQ(OK,
            stream_->SendRequest(headers_, &response_, callback_.callback()));

  // Check that priority has now dropped back to MEDIUM.
  DCHECK_EQ(MEDIUM,
            ConvertQuicPriorityToRequestPriority(reliable_stream->Priority()));

  // Ack the request.
  ProcessPacket(ConstructAckPacket(1, 0, 0));
  EXPECT_EQ(ERR_IO_PENDING, stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response with a body.
  SetResponse("404 OK", "hello world!");
  // In the course of processing this packet, the QuicHttpStream close itself.
  ProcessPacket(ConstructResponseHeadersPacket(2, kFin, nullptr));

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_TRUE(AtEof());

  // QuicHttpStream::GetTotalSent/ReceivedBytes currently only includes the
  // headers and payload.
  EXPECT_EQ(static_cast<int64_t>(spdy_request_headers_frame_length),
            stream_->GetTotalSentBytes());
  // Zero since the stream is closed before processing the headers.
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

// Regression test for http://crbug.com/294870
TEST_P(QuicHttpStreamTest, CheckPriorityWithNoDelegate) {
  SetRequest("GET", "/", MEDIUM);
  use_closing_stream_ = true;

  AddWrite(ConstructRstStreamPacket(1));

  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, MEDIUM, net_log_,
                                          callback_.callback()));

  // Check that priority is highest.
  QuicChromiumClientStream* reliable_stream =
      QuicHttpStreamPeer::GetQuicChromiumClientStream(stream_.get());
  DCHECK(reliable_stream);
  QuicChromiumClientStream::Delegate* delegate = reliable_stream->GetDelegate();
  DCHECK(delegate);
  DCHECK_EQ(kV3HighestPriority, reliable_stream->Priority());

  // Set Delegate to nullptr and make sure Priority returns highest
  // priority.
  reliable_stream->SetDelegate(nullptr);
  DCHECK_EQ(kV3HighestPriority, reliable_stream->Priority());
  reliable_stream->SetDelegate(delegate);

  EXPECT_EQ(0, stream_->GetTotalSentBytes());
  EXPECT_EQ(0, stream_->GetTotalReceivedBytes());
}

TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendHeadersComplete) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  AddWrite(SYNCHRONOUS, ERR_FAILED);
  Initialize();

  ChunkedUploadDataStream upload_data_stream(0);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR,
            stream_->SendRequest(headers_, &response_, callback_.callback()));
}

TEST_P(QuicHttpStreamTest, SessionClosedBeforeSendBodyComplete) {
  SetRequest("POST", "/", DEFAULT_PRIORITY);
  size_t spdy_request_headers_frame_length;
  AddWrite(ConstructRequestHeadersPacket(1, !kFin, DEFAULT_PRIORITY,
                                         &spdy_request_headers_frame_length));
  AddWrite(SYNCHRONOUS, ERR_FAILED);
  Initialize();

  ChunkedUploadDataStream upload_data_stream(0);
  size_t chunk_size = strlen(kUploadData);
  upload_data_stream.AppendData(kUploadData, chunk_size, false);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(
                    TestCompletionCallback().callback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY, net_log_,
                                          callback_.callback()));
  ASSERT_EQ(ERR_QUIC_PROTOCOL_ERROR,
            stream_->SendRequest(headers_, &response_, callback_.callback()));
}

}  // namespace test
}  // namespace net
