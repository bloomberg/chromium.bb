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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace net {
namespace test {
namespace {

const char kUploadData[] = "hello world!";

class TestQuicConnection : public QuicConnection {
 public:
  TestQuicConnection(QuicGuid guid,
                     IPEndPoint address,
                     QuicConnectionHelper* helper)
      : QuicConnection(guid, address, helper, false) {
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
  explicit AutoClosingStream(QuicReliableClientStream* stream)
      : QuicHttpStream(stream) {
  }

  virtual int OnDataReceived(const char* data, int length) OVERRIDE {
    Close(false);
    return OK;
  }
};

}  // namespace

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
        framer_(kQuicVersion1,
                QuicDecrypter::Create(kNULL),
                QuicEncrypter::Create(kNULL),
                QuicTime::Zero(),
                false),
        creator_(guid_, &framer_, &random_, false) {
    IPAddressNumber ip;
    CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
    peer_addr_ = IPEndPoint(ip, 443);
    self_addr_ = IPEndPoint(ip, 8435);
  }

  ~QuicHttpStreamTest() {
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
    EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _)).
        WillRepeatedly(testing::Return(QuicTime::Delta::Zero()));
    helper_ = new QuicConnectionHelper(runner_.get(), &clock_,
                                       &random_generator_, socket);
    connection_ = new TestQuicConnection(guid_, peer_addr_, helper_);
    connection_->set_visitor(&visitor_);
    connection_->SetSendAlgorithm(send_algorithm_);
    connection_->SetReceiveAlgorithm(receive_algorithm_);
    session_.reset(new QuicClientSession(connection_, socket, NULL,
                                         &crypto_client_stream_factory_,
                                         "www.google.com", NULL));
    session_->GetCryptoStream()->CryptoConnect();
    EXPECT_TRUE(session_->IsCryptoHandshakeComplete());
    QuicReliableClientStream* stream =
        session_->CreateOutgoingReliableStream();
    stream_.reset(use_closing_stream_ ?
                  new AutoClosingStream(stream) :
                  new QuicHttpStream(stream));
  }

  void SetRequestString(const std::string& method, const std::string& path) {
    SpdyHeaderBlock headers;
    headers[":method"] = method;
    headers[":host"] = "www.google.com";
    headers[":path"] = path;
    headers[":scheme"] = "http";
    headers[":version"] = "HTTP/1.1";
    request_data_ = SerializeHeaderBlock(headers);
  }

  void SetResponseString(const std::string& status, const std::string& body) {
    SpdyHeaderBlock headers;
    headers[":status"] = status;
    headers[":version"] = "HTTP/1.1";
    headers["content-type"] = "text/plain";
    response_data_ = SerializeHeaderBlock(headers) + body;
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
  scoped_array<MockWrite> mock_writes_;
  MockClock clock_;
  MockRandom random_generator_;
  TestQuicConnection* connection_;
  QuicConnectionHelper* helper_;
  testing::StrictMock<MockConnectionVisitor> visitor_;
  scoped_ptr<QuicHttpStream> stream_;
  scoped_ptr<QuicClientSession> session_;
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
    header_.packet_sequence_number = sequence_number;
    header_.fec_group = 0;
    header_.fec_entropy_flag = false;
    header_.entropy_flag = false;
    header_.fec_flag = false;
  }

  QuicEncryptedPacket* ConstructPacket(const QuicPacketHeader& header,
                                       const QuicFrame& frame) {
    QuicFrames frames;
    frames.push_back(frame);
    scoped_ptr<QuicPacket> packet(
        framer_.ConstructFrameDataPacket(header_, frames).packet);
    return framer_.EncryptPacket(header.packet_sequence_number, *packet);
  }

  const QuicGuid guid_;
  QuicFramer framer_;
  IPEndPoint self_addr_;
  IPEndPoint peer_addr_;
  MockRandom random_;
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
  SetRequestString("GET", "/");
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0,
                                            request_data_));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(2, 2, 1));
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
  SetResponseString("404 Not Found", "");
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, kFin, 0, response_data_));
  ProcessPacket(*resp);

  // Now that the headers have been processed, the callback will return.
  EXPECT_EQ(OK, callback_.WaitForResult());
  ASSERT_TRUE(response_.headers != NULL);
  EXPECT_EQ(404, response_.headers->response_code());
  EXPECT_TRUE(response_.headers->HasHeaderValue("Content-Type", "text/plain"));

  // There is no body, so this should return immediately.
  EXPECT_EQ(0, stream_->ReadResponseBody(read_buffer_.get(),
                                         read_buffer_->size(),
                                         callback_.callback()));
  EXPECT_TRUE(stream_->IsResponseBodyComplete());
  EXPECT_TRUE(AtEof());
}

TEST_F(QuicHttpStreamTest, GetRequestFullResponseInSinglePacket) {
  SetRequestString("GET", "/");
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0, request_data_));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(2, 2, 1));
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
  ASSERT_TRUE(response_.headers != NULL);
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
  SetRequestString("POST", "/");
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, !kFin, 0, request_data_));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(2, true, kFin,
                                            request_data_.length(),
                                            kUploadData));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1));

  Initialize();

  ScopedVector<UploadElementReader> element_readers;
  element_readers.push_back(
      new UploadBytesElementReader(kUploadData, strlen(kUploadData)));
  UploadDataStream upload_data_stream(&element_readers, 0);
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
  SetResponseString("200 OK", "");
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, !kFin, 0, response_data_));
  ProcessPacket(*resp);

  // Since the headers have already arrived, this should return immediately.
  EXPECT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers != NULL);
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
  SetRequestString("POST", "/");
  size_t chunk_size = strlen(kUploadData);
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, !kFin, 0, request_data_));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(2, true, !kFin,
                                            request_data_.length(),
                                            kUploadData));
  AddWrite(SYNCHRONOUS, ConstructDataPacket(3, true, kFin,
                                            request_data_.length() + chunk_size,
                                            kUploadData));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(4, 2, 1));

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
  SetResponseString("200 OK", "");
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, !kFin, 0, response_data_));
  ProcessPacket(*resp);

  // Since the headers have already arrived, this should return immediately.
  ASSERT_EQ(OK, stream_->ReadResponseHeaders(callback_.callback()));
  ASSERT_TRUE(response_.headers != NULL);
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
  SetRequestString("GET", "/");
  AddWrite(SYNCHRONOUS, ConstructDataPacket(1, true, kFin, 0, request_data_));
  AddWrite(SYNCHRONOUS, ConstructRstPacket(2, 3));
  AddWrite(SYNCHRONOUS, ConstructAckPacket(3, 2, 1));
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
  const char kResponseHeaders[] = "HTTP/1.1 404 OK\r\n"
      "Content-Type: text/plain\r\n\r\nhello world!";
  scoped_ptr<QuicEncryptedPacket> resp(
      ConstructDataPacket(2, false, kFin, 0, kResponseHeaders));

  // In the course of processing this packet, the QuicHttpStream close itself.
  ProcessPacket(*resp);

  EXPECT_TRUE(AtEof());
}

}  // namespace test

}  // namespace net
