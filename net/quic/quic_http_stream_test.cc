// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_stream.h"

#include <vector>

#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_response_headers.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_default_packet_writer.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_reliable_client_stream.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_crypto_client_stream_factory.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/test_task_runner.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/write_blocked_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net {
namespace test {
namespace {

const char kUploadData[] = "hello world!";

class TestQuicConnection : public QuicConnection {
 public:
  TestQuicConnection(QuicGuid guid,
                     IPEndPoint address,
                     QuicConnectionHelper* helper,
                     QuicPacketWriter* writer)
      : QuicConnection(guid, address, helper, writer, false,
                       QuicSupportedVersions()) {
  }

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }

  void SetReceiveAlgorithm(ReceiveAlgorithmInterface* receive_algorithm) {
    QuicConnectionPeer::SetReceiveAlgorithm(this, receive_algorithm);
  }
};

class TestReceiveAlgorithm : public ReceiveAlgorithmInterface {
 public:
  explicit TestReceiveAlgorithm(QuicCongestionFeedbackFrame* feedback)
      : feedback_(feedback) {
  }

  bool GenerateCongestionFeedback(
      QuicCongestionFeedbackFrame* congestion_feedback) {
    if (feedback_ == NULL) {
      return false;
    }
    *congestion_feedback = *feedback_;
    return true;
  }

  MOCK_METHOD4(RecordIncomingPacket,
               void(QuicByteCount, QuicPacketSequenceNumber, QuicTime, bool));

 private:
  MockClock clock_;
  QuicCongestionFeedbackFrame* feedback_;

  DISALLOW_COPY_AND_ASSIGN(TestReceiveAlgorithm);
};

// Subclass of QuicHttpStream that closes itself when the first piece of data
// is received.
class AutoClosingStream : public QuicHttpStream {
 public:
  explicit AutoClosingStream(const base::WeakPtr<QuicClientSession>& session)
      : QuicHttpStream(session) {
  }

  virtual int OnDataReceived(const char* data, int length) OVERRIDE {
    Close(false);
    return OK;
  }
};

}  // namespace

class QuicHttpStreamPeer {
 public:
  static QuicReliableClientStream* GetQuicReliableClientStream(
      QuicHttpStream* stream) {
    return stream->stream_;
  }
};

class QuicHttpStreamTest : public ::testing::TestWithParam<bool> {
 protected:
  const static bool kFin = true;
  // Holds a packet to be written to the wire, and the IO mode that should
  // be used by the mock socket when performing the write.
  struct PacketToWrite {
    PacketToWrite(IoMode mode, QuicEncryptedPacket* packet)
        : mode(mode),
          packet(packet) {
    }
    IoMode mode;
    QuicEncryptedPacket* packet;
  };

  QuicHttpStreamTest()
      : net_log_(BoundNetLog()),
        use_closing_stream_(false),
        read_buffer_(new IOBufferWithSize(4096)),
        guid_(2),
        framer_(QuicSupportedVersions(), QuicTime::Zero(), false),
        random_generator_(0),
        creator_(guid_, &framer_, &random_generator_, false) {
    IPAddressNumber ip;
    CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
    peer_addr_ = IPEndPoint(ip, 443);
    self_addr_ = IPEndPoint(ip, 8435);
  }

  ~QuicHttpStreamTest() {
    session_->CloseSessionOnError(ERR_ABORTED);
    for (size_t i = 0; i < writes_.size(); i++) {
      delete writes_[i].packet;
    }
  }

  // Adds a packet to the list of expected writes.
  void AddWrite(IoMode mode, QuicEncryptedPacket* packet) {
    writes_.push_back(PacketToWrite(mode, packet));
  }

  // Returns the packet to be written at position |pos|.
  QuicEncryptedPacket* GetWrite(size_t pos) {
    return writes_[pos].packet;
  }

  bool AtEof() {
    return socket_data_->at_read_eof() && socket_data_->at_write_eof();
  }

  void ProcessPacket(const QuicEncryptedPacket& packet) {
    connection_->ProcessUdpPacket(self_addr_, peer_addr_, packet);
  }

  // Configures the test fixture to use the list of expected writes.
  void Initialize() {
    mock_writes_.reset(new MockWrite[writes_.size()]);
    for (size_t i = 0; i < writes_.size(); i++) {
      mock_writes_[i] = MockWrite(writes_[i].mode,
                                  writes_[i].packet->data(),
                                  writes_[i].packet->length());
    };

    socket_data_.reset(new StaticSocketDataProvider(NULL, 0, mock_writes_.get(),
                                                    writes_.size()));

    MockUDPClientSocket* socket = new MockUDPClientSocket(socket_data_.get(),
                                                          net_log_.net_log());
    socket->Connect(peer_addr_);
    runner_ = new TestTaskRunner(&clock_);
    send_algorithm_ = new MockSendAlgorithm();
    receive_algorithm_ = new TestReceiveAlgorithm(NULL);
    EXPECT_CALL(*receive_algorithm_, RecordIncomingPacket(_, _, _, _)).
        Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_,
                OnPacketSent(_, _, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillRepeatedly(
        Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _, _)).
        WillRepeatedly(Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, SmoothedRtt()).WillRepeatedly(
        Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate()).WillRepeatedly(
        Return(QuicBandwidth::Zero()));
    EXPECT_CALL(*send_algorithm_, SetFromConfig(_, _)).Times(AnyNumber());
    helper_.reset(new QuicConnectionHelper(runner_.get(), &clock_,
                                           &random_generator_));
    writer_.reset(new QuicDefaultPacketWriter(socket));
    connection_ = new TestQuicConnection(guid_, peer_addr_, helper_.get(),
                                         writer_.get());
    connection_->set_visitor(&visitor_);
    connection_->SetSendAlgorithm(send_algorithm_);
    connection_->SetReceiveAlgorithm(receive_algorithm_);
    crypto_config_.SetDefaults();
    session_.reset(
        new QuicClientSession(connection_,
                              scoped_ptr<DatagramClientSocket>(socket),
                              writer_.Pass(), NULL,
                              &crypto_client_stream_factory_,
                              "www.google.com", DefaultQuicConfig(),
                              &crypto_config_, NULL));
    session_->GetCryptoStream()->CryptoConnect();
    EXPECT_TRUE(session_->IsCryptoHandshakeConfirmed());
    stream_.reset(use_closing_stream_ ?
                  new AutoClosingStream(session_->GetWeakPtr()) :
                  new QuicHttpStream(session_->GetWeakPtr()));
  }

  void SetRequestString(const std::string& method,
                        const std::string& path,
                        RequestPriority priority) {
    SpdyHeaderBlock headers;
    headers[":method"] = method;
    headers[":host"] = "www.google.com";
    headers[":path"] = path;
    headers[":scheme"] = "http";
    headers[":version"] = "HTTP/1.1";
    request_data_ = SerializeHeaderBlock(headers, true, priority);
  }

  void SetResponseString(const std::string& status, const std::string& body) {
    SpdyHeaderBlock headers;
    headers[":status"] = status;
    headers[":version"] = "HTTP/1.1";
    headers["content-type"] = "text/plain";
    response_data_ = SerializeHeaderBlock(headers, false, DEFAULT_PRIORITY) +
        body;
  }

  std::string SerializeHeaderBlock(const SpdyHeaderBlock& headers,
                                   bool write_priority,
                                   RequestPriority priority) {
    QuicSpdyCompressor compressor;
    if (write_priority) {
      return compressor.CompressHeadersWithPriority(
          ConvertRequestPriorityToQuicPriority(priority), headers);
    }
    return compressor.CompressHeaders(headers);
  }

  // Returns a newly created packet to send kData on stream 3.
  QuicEncryptedPacket* ConstructDataPacket(
      QuicPacketSequenceNumber sequence_number,
      bool should_include_version,
      bool fin,
      QuicStreamOffset offset,
      base::StringPiece data) {
    InitializeHeader(sequence_number, should_include_version);
    QuicStreamFrame frame(3, fin, offset,  MakeIOVector(data));
    return ConstructPacket(header_, QuicFrame(&frame));
  }

  // Returns a newly created packet to RST_STREAM stream 3.
  QuicEncryptedPacket* ConstructRstStreamPacket(
      QuicPacketSequenceNumber sequence_number) {
    InitializeHeader(sequence_number, false);
    QuicRstStreamFrame frame(3, QUIC_STREAM_CANCELLED);
    return ConstructPacket(header_, QuicFrame(&frame));
  }

  // Returns a newly created packet to send ack data.
  QuicEncryptedPacket* ConstructAckPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicPacketSequenceNumber largest_received,
      QuicPacketSequenceNumber least_unacked) {
    InitializeHeader(sequence_number, false);

    QuicAckFrame ack(largest_received, QuicTime::Zero(), least_unacked);
    ack.sent_info.entropy_hash = 0;
    ack.received_info.entropy_hash = 0;

    return ConstructPacket(header_, QuicFrame(&ack));
  }

  // Returns a newly created packet to send ack data.
  QuicEncryptedPacket* ConstructRstPacket(
      QuicPacketSequenceNumber sequence_number,
      QuicStreamId stream_id) {
    InitializeHeader(sequence_number, false);

    QuicRstStreamFrame rst(stream_id, QUIC_STREAM_NO_ERROR);
    return ConstructPacket(header_, QuicFrame(&rst));
  }

  BoundNetLog net_log_;
  bool use_closing_stream_;
  MockSendAlgorithm* send_algorithm_;
  TestReceiveAlgorithm* receive_algorithm_;
  scoped_refptr<TestTaskRunner> runner_;
  scoped_ptr<MockWrite[]> mock_writes_;
  MockClock clock_;
  TestQuicConnection* connection_;
  scoped_ptr<QuicConnectionHelper> helper_;
  testing::StrictMock<MockConnectionVisitor> visitor_;
  scoped_ptr<QuicHttpStream> stream_;
  scoped_ptr<QuicDefaultPacketWriter> writer_;
  scoped_ptr<QuicClientSession> session_;
  QuicCryptoClientConfig crypto_config_;
  TestCompletionCallback callback_;
  HttpRequestInfo request_;
  HttpRequestHeaders headers_;
  HttpResponseInfo response_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  std::string request_data_;
  std::string response_data_;

 private:
  void InitializeHeader(QuicPacketSequenceNumber sequence_number,
                        bool should_include_version) {
    header_.public_header.guid = guid_;
    header_.public_header.reset_flag = false;
    header_.public_header.version_flag = should_include_version;
    header_.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
    header_.packet_sequence_number = sequence_number;
    header_.fec_group = 0;
    header_.entropy_flag = false;
    header_.fec_flag = false;
  }

  QuicEncryptedPacket* ConstructPacket(const QuicPacketHeader& header,
                                       const QuicFrame& frame) {
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer_.BuildUnsizedDataPacket(header_, frames).packet);
    return framer_.EncryptPacket(
        ENCRYPTION_NONE, header.packet_sequence_number, *packet);
  }

  const QuicGuid guid_;
  QuicFramer framer_;
  IPEndPoint self_addr_;
  IPEndPoint peer_addr_;
  MockRandom random_generator_;
  MockCryptoClientStreamFactory crypto_client_stream_factory_;
  QuicPacketCreator creator_;
  QuicPacketHeader header_;
  scoped_ptr<StaticSocketDataProvider> socket_data_;
  std::vector<PacketToWrite> writes_;
};

TEST_F(QuicHttpStreamTest, RenewStreamForAuth) {
  Initialize();
  EXPECT_EQ(NULL, stream_->RenewStreamForAuth());
}

TEST_F(QuicHttpStreamTest, CanFindEndOfResponse) {
  Initialize();
  EXPECT_TRUE(stream_->CanFindEndOfResponse());
}

TEST_F(QuicHttpStreamTest, IsConnectionReusable) {
  Initialize();
  EXPECT_FALSE(stream_->IsConnectionReusable());
}

TEST_F(QuicHttpStreamTest, GetRequest) {
  SetRequestString("GET", "/", DEFAULT_PRIORITY);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0,
                                            request_data_));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Ack the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0, 0));
  ProcessPacket(*ack);

  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response without a body.
  SetResponseString("404 Not Found", std::string());
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, kFin, 0, response_data_));
  ProcessPacket(*resp);

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(404, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  EXPECT_EQ(0, stream_->ReadResponseBody(read_buffer_.get(),
                                         read_buffer_->size(),
                                         callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

// Regression test for http://crbug.com/288128
TEST_F(QuicHttpStreamTest, GetRequestLargeResponse) {
  SetRequestString("GET", "/", DEFAULT_PRIORITY);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0,
                                            request_data_));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Ack the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0, 0));
  ProcessPacket(*ack);

  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  SpdyHeaderBlock headers;
  headers[":status"] = "200 OK";
  headers[":version"] = "HTTP/1.1";
  headers["content-type"] = "text/plain";
  headers["big6"] = std::string(10000, 'x');  // Lots of x's.

  std::string response = SpdyUtils::SerializeUncompressedHeaders(headers);
  EXPECT_LT(4096u, response.length());
  stream_->OnDataReceived(response.data(), response.length());
  stream_->OnClose(QUIC_NO_ERROR);

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  EXPECT_EQ(0, stream_->ReadResponseBody(read_buffer_.get(),
                                         read_buffer_->size(),
                                         callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicHttpStreamTest, GetRequestFullResponseInSinglePacket) {
  SetRequestString("GET", "/", DEFAULT_PRIORITY);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0, request_data_));
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Ack the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0, 0));
  ProcessPacket(*ack);

  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response with a body.
  SetResponseString("200 OK", "hello world!");
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, kFin, 0, response_data_));
  ProcessPacket(*resp);

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  // Since the body has already arrived, this should return immediately.
  EXPECT_EQ(12, stream_->ReadResponseBody(read_buffer_.get(),
                                          read_buffer_->size(),
                                          callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicHttpStreamTest, SendPostRequest) {
  SetRequestString("POST", "/", DEFAULT_PRIORITY);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, !kFin, 0, request_data_));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(2, true, kFin,
                                            request_data_.length(),
                                            kUploadData));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 3, 1));

  Initialize();

  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(
      new UploadBytesElementReader(kUploadData, strlen(kUploadData)));
  UploadDataStream upload_data_stream(element_readers.Pass(), 0);
  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(CompletionCallback()));

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Ack both packets in the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0, 0));
  ProcessPacket(*ack);

  // Send the response headers (but not the body).
  SetResponseString("200 OK", std::string());
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, !kFin, 0, response_data_));
  ProcessPacket(*resp);

  // Since the headers have already arrived, this should return immediately.
  EXPECT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  scoped_ptr<QuicEncryptedPacket> resp_body(
      ConstructDataPacket(3, false, kFin, response_data_.length(),
                          kResponseBody));
  ProcessPacket(*resp_body);

  // Since the body has already arrived, this should return immediately.
  EXPECT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicHttpStreamTest, SendChunkedPostRequest) {
  SetRequestString("POST", "/", DEFAULT_PRIORITY);
  size_t chunk_size = strlen(kUploadData);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, !kFin, 0, request_data_));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(2, true, !kFin,
                                            request_data_.length(),
                                            kUploadData));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(3, true, kFin,
                                            request_data_.length() + chunk_size,
                                            kUploadData));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(4, 3, 1));

  Initialize();

  UploadDataStream upload_data_stream(UploadDataStream::CHUNKED, 0);
  upload_data_stream.AppendChunk(kUploadData, chunk_size, false);

  request_.method = "POST";
  request_.url = GURL("http://www.google.com/");
  request_.upload_data_stream = &upload_data_stream;
  ASSERT_EQ(OK, request_.upload_data_stream->Init(CompletionCallback()));

  ASSERT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  ASSERT_EQ(ERR_IO_PENDING, stream_->SendRequest(headers_, &response_,
                                                 callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  upload_data_stream.AppendChunk(kUploadData, chunk_size, true);

  // Ack both packets in the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0, 0));
  ProcessPacket(*ack);

  // Send the response headers (but not the body).
  SetResponseString("200 OK", std::string());
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, !kFin, 0, response_data_));
  ProcessPacket(*resp);

  // Since the headers have already arrived, this should return immediately.
  ASSERT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers.get());
  EXPECT_EQ(200, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // Send the response body.
  const char kResponseBody[] = "Hello world!";
  scoped_ptr<QuicEncryptedPacket> resp_body(
      ConstructDataPacket(3, false, kFin, response_data_.length(),
                          kResponseBody));
  ProcessPacket(*resp_body);

  // Since the body has already arrived, this should return immediately.
  ASSERT_EQ(static_cast<int>(strlen(kResponseBody)),
            stream_->ReadResponseBody(read_buffer_.get(), read_buffer_->size(),
                                      callback_.callback()));

  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicHttpStreamTest, DestroyedEarly) {
  SetRequestString("GET", "/", DEFAULT_PRIORITY);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0, request_data_));
  AddWrite(SYNCHRONOUS, ConstructRstStreamPacket(2));
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, DEFAULT_PRIORITY,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Ack the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0, 0));
  ProcessPacket(*ack);
  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response with a body.
  SetResponseString("404 OK", "hello world!");
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, kFin, 0, response_data_));

  // In the course of processing this packet, the QuicHttpStream close itself.
  ProcessPacket(*resp);

  EXPECT_TRUE(AtEof());
}

TEST_F(QuicHttpStreamTest, Priority) {
  SetRequestString("GET", "/", MEDIUM);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0, request_data_));
  AddWrite(SYNCHRONOUS, ConstructRstStreamPacket(2));
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, MEDIUM,
                                          net_log_, callback_.callback()));

  // Check that priority is highest.
  QuicReliableClientStream* reliable_stream =
      QuicHttpStreamPeer::GetQuicReliableClientStream(stream_.get());
  DCHECK(reliable_stream);
  DCHECK_EQ(static_cast<QuicPriority>(kHighestPriority),
            reliable_stream->EffectivePriority());

  EXPECT_EQ(OK, stream_->SendRequest(headers_, &response_,
                                     callback_.callback()));
  EXPECT_EQ(&response_, stream_->GetResponseInfo());

  // Check that priority has now dropped back to MEDIUM.
  DCHECK_EQ(MEDIUM, ConvertQuicPriorityToRequestPriority(
      reliable_stream->EffectivePriority()));

  // Ack the request.
  scoped_ptr<QuicEncryptedPacket> ack(ConstructAckPacket(1, 0, 0));
  ProcessPacket(*ack);
  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadResponseHeaders(callback_.callback()));

  // Send the response with a body.
  SetResponseString("404 OK", "hello world!");
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, kFin, 0, response_data_));

  // In the course of processing this packet, the QuicHttpStream close itself.
  ProcessPacket(*resp);

  EXPECT_TRUE(AtEof());
}

// Regression test for http://crbug.com/294870
TEST_F(QuicHttpStreamTest, CheckPriorityWithNoDelegate) {
  SetRequestString("GET", "/", MEDIUM);
  use_closing_stream_ = true;
  Initialize();

  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_EQ(OK, stream_->InitializeStream(&request_, MEDIUM,
                                          net_log_, callback_.callback()));

  // Check that priority is highest.
  QuicReliableClientStream* reliable_stream =
      QuicHttpStreamPeer::GetQuicReliableClientStream(stream_.get());
  DCHECK(reliable_stream);
  QuicReliableClientStream::Delegate* delegate = reliable_stream->GetDelegate();
  DCHECK(delegate);
  DCHECK_EQ(static_cast<QuicPriority>(kHighestPriority),
            reliable_stream->EffectivePriority());

  // Set Delegate to NULL and make sure EffectivePriority returns highest
  // priority.
  reliable_stream->SetDelegate(NULL);
  DCHECK_EQ(static_cast<QuicPriority>(kHighestPriority),
            reliable_stream->EffectivePriority());
  reliable_stream->SetDelegate(delegate);
}

TEST_F(QuicHttpStreamTest, DontCompressHeadersWhenNotWritable) {
  SetRequestString("GET", "/", MEDIUM);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0, request_data_));

  Initialize();
  request_.method = "GET";
  request_.url = GURL("http://www.google.com/");

  EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _, _)).
      WillRepeatedly(Return(QuicTime::Delta::Infinite()));
  EXPECT_EQ(OK, stream_->InitializeStream(&request_, MEDIUM,
                                          net_log_, callback_.callback()));
  EXPECT_EQ(ERR_IO_PENDING, stream_->SendRequest(headers_, &response_,
                                                 callback_.callback()));

  // Verify that the headers have not been compressed and buffered in
  // the stream.
  QuicReliableClientStream* reliable_stream =
      QuicHttpStreamPeer::GetQuicReliableClientStream(stream_.get());
  EXPECT_FALSE(reliable_stream->HasBufferedData());
  EXPECT_FALSE(AtEof());

  EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _, _)).
      WillRepeatedly(Return(QuicTime::Delta::Zero()));

  // Data should flush out now.
  connection_->OnCanWrite();
  EXPECT_FALSE(reliable_stream->HasBufferedData());
  EXPECT_TRUE(AtEof());
}

}  // namespace test

}  // namespace net
