// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/bidirectional_stream.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kBodyData[] = "Body data";
const size_t kBodyDataSize = arraysize(kBodyData);
// Size of the buffer to be allocated for each read.
const size_t kReadBufferSize = 4096;

// Delegate that reads data but does not send any data.
class TestDelegateBase : public BidirectionalStream::Delegate {
 public:
  TestDelegateBase(IOBuffer* read_buf, int read_buf_len)
      : TestDelegateBase(read_buf,
                         read_buf_len,
                         make_scoped_ptr(new base::Timer(false, false))) {}

  TestDelegateBase(IOBuffer* read_buf,
                   int read_buf_len,
                   scoped_ptr<base::Timer> timer)
      : read_buf_(read_buf),
        read_buf_len_(read_buf_len),
        timer_(std::move(timer)),
        loop_(nullptr),
        error_(OK),
        on_data_read_count_(0),
        on_data_sent_count_(0),
        do_not_start_read_(false),
        run_until_completion_(false),
        not_expect_callback_(false) {}

  ~TestDelegateBase() override {}

  void OnHeadersSent() override { CHECK(!not_expect_callback_); }

  void OnHeadersReceived(const SpdyHeaderBlock& response_headers) override {
    CHECK(!not_expect_callback_);

    response_headers_ = response_headers;
    if (!do_not_start_read_)
      StartOrContinueReading();
  }

  void OnDataRead(int bytes_read) override {
    CHECK(!not_expect_callback_);

    ++on_data_read_count_;
    CHECK_GE(bytes_read, OK);
    data_received_.append(read_buf_->data(), bytes_read);
    if (!do_not_start_read_)
      StartOrContinueReading();
  }

  void OnDataSent() override {
    CHECK(!not_expect_callback_);

    ++on_data_sent_count_;
  }

  void OnTrailersReceived(const SpdyHeaderBlock& trailers) override {
    CHECK(!not_expect_callback_);

    trailers_ = trailers;
    if (run_until_completion_)
      loop_->Quit();
  }

  void OnFailed(int error) override {
    CHECK(!not_expect_callback_);
    CHECK_EQ(OK, error_);
    CHECK_NE(OK, error);

    error_ = error;
    if (run_until_completion_)
      loop_->Quit();
  }

  void Start(scoped_ptr<BidirectionalStreamRequestInfo> request_info,
             HttpNetworkSession* session) {
    stream_.reset(new BidirectionalStream(std::move(request_info), session,
                                          this, std::move(timer_)));
    if (run_until_completion_)
      loop_->Run();
  }

  void SendData(IOBuffer* data, int length, bool end_of_stream) {
    not_expect_callback_ = true;
    stream_->SendData(data, length, end_of_stream);
    not_expect_callback_ = false;
  }

  // Starts or continues reading data from |stream_| until no more bytes
  // can be read synchronously.
  void StartOrContinueReading() {
    int rv = ReadData();
    while (rv > 0) {
      rv = ReadData();
    }
    if (run_until_completion_ && rv == 0)
      loop_->Quit();
  }

  // Calls ReadData on the |stream_| and updates internal states.
  int ReadData() {
    not_expect_callback_ = true;
    int rv = stream_->ReadData(read_buf_.get(), read_buf_len_);
    not_expect_callback_ = false;
    if (rv > 0)
      data_received_.append(read_buf_->data(), rv);
    return rv;
  }

  // Cancels |stream_|.
  void CancelStream() { stream_->Cancel(); }

  // Deletes |stream_|.
  void DeleteStream() { stream_.reset(); }

  NextProto GetProtocol() const { return stream_->GetProtocol(); }

  int64_t GetTotalReceivedBytes() const {
    return stream_->GetTotalReceivedBytes();
  }

  int64_t GetTotalSentBytes() const { return stream_->GetTotalSentBytes(); }

  // Const getters for internal states.
  const std::string& data_received() const { return data_received_; }
  int error() const { return error_; }
  const SpdyHeaderBlock& response_headers() const { return response_headers_; }
  const SpdyHeaderBlock& trailers() const { return trailers_; }
  int on_data_read_count() const { return on_data_read_count_; }
  int on_data_sent_count() const { return on_data_sent_count_; }

  // Sets whether the delegate should automatically start reading.
  void set_do_not_start_read(bool do_not_start_read) {
    do_not_start_read_ = do_not_start_read;
  }
  // Sets whether the delegate should wait until the completion of the stream.
  void SetRunUntilCompletion(bool run_until_completion) {
    run_until_completion_ = run_until_completion;
    loop_.reset(new base::RunLoop);
  }

 protected:
  // Quits |loop_|.
  void QuitLoop() { loop_->Quit(); }

 private:
  scoped_ptr<BidirectionalStream> stream_;
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;
  scoped_ptr<base::Timer> timer_;
  std::string data_received_;
  scoped_ptr<base::RunLoop> loop_;
  SpdyHeaderBlock response_headers_;
  SpdyHeaderBlock trailers_;
  int error_;
  int on_data_read_count_;
  int on_data_sent_count_;
  bool do_not_start_read_;
  bool run_until_completion_;
  // This is to ensure that delegate callback is not invoked synchronously when
  // calling into |stream_|.
  bool not_expect_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegateBase);
};

// A delegate that deletes the stream in a particular callback.
class CancelOrDeleteStreamDelegate : public TestDelegateBase {
 public:
  // Specifies in which callback the stream can be deleted.
  enum Phase {
    ON_HEADERS_RECEIVED,
    ON_DATA_READ,
    ON_TRAILERS_RECEIVED,
    ON_FAILED,
  };

  CancelOrDeleteStreamDelegate(IOBuffer* buf,
                               int buf_len,
                               Phase phase,
                               bool do_cancel)
      : TestDelegateBase(buf, buf_len), phase_(phase), do_cancel_(do_cancel) {}
  ~CancelOrDeleteStreamDelegate() override {}

  void OnHeadersReceived(const SpdyHeaderBlock& response_headers) override {
    TestDelegateBase::OnHeadersReceived(response_headers);
    if (phase_ == ON_HEADERS_RECEIVED) {
      CancelOrDelete();
      QuitLoop();
    }
  }

  void OnDataSent() override { NOTREACHED(); }

  void OnDataRead(int bytes_read) override {
    if (phase_ == ON_HEADERS_RECEIVED) {
      NOTREACHED();
      return;
    }
    TestDelegateBase::OnDataRead(bytes_read);
    if (phase_ == ON_DATA_READ) {
      CancelOrDelete();
      QuitLoop();
    }
  }

  void OnTrailersReceived(const SpdyHeaderBlock& trailers) override {
    if (phase_ == ON_HEADERS_RECEIVED || phase_ == ON_DATA_READ) {
      NOTREACHED();
      return;
    }
    TestDelegateBase::OnTrailersReceived(trailers);
    if (phase_ == ON_TRAILERS_RECEIVED) {
      CancelOrDelete();
      QuitLoop();
    }
  }

  void OnFailed(int error) override {
    if (phase_ != ON_FAILED) {
      NOTREACHED();
      return;
    }
    TestDelegateBase::OnFailed(error);
    CancelOrDelete();
    QuitLoop();
  }

 private:
  void CancelOrDelete() {
    if (do_cancel_) {
      CancelStream();
    } else {
      DeleteStream();
    }
  }

  // Indicates in which callback the delegate should cancel or delete the
  // stream.
  Phase phase_;
  // Indicates whether to cancel or delete the stream.
  bool do_cancel_;

  DISALLOW_COPY_AND_ASSIGN(CancelOrDeleteStreamDelegate);
};

// A Timer that does not start a delayed task unless the timer is fired.
class MockTimer : public base::MockTimer {
 public:
  MockTimer() : base::MockTimer(false, false) {}
  ~MockTimer() override {}

  void Start(const tracked_objects::Location& posted_from,
             base::TimeDelta delay,
             const base::Closure& user_task) override {
    // Sets a maximum delay, so the timer does not fire unless it is told to.
    base::TimeDelta infinite_delay = base::TimeDelta::Max();
    base::MockTimer::Start(posted_from, infinite_delay, user_task);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTimer);
};

}  // namespace

class BidirectionalStreamTest : public testing::TestWithParam<bool> {
 public:
  BidirectionalStreamTest()
      : spdy_util_(kProtoHTTP2, false),
        session_deps_(kProtoHTTP2),
        ssl_data_(SSLSocketDataProvider(ASYNC, OK)) {
    ssl_data_.SetNextProto(kProtoHTTP2);
    ssl_data_.cert = ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem");
  }

 protected:
  void TearDown() override {
    if (sequenced_data_) {
      EXPECT_TRUE(sequenced_data_->AllReadDataConsumed());
      EXPECT_TRUE(sequenced_data_->AllWriteDataConsumed());
    }
  }

  // Initializes the session using SequencedSocketData.
  void InitSession(MockRead* reads,
                   size_t reads_count,
                   MockWrite* writes,
                   size_t writes_count,
                   const SpdySessionKey& key) {
    ASSERT_TRUE(ssl_data_.cert.get());
    session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_data_);
    sequenced_data_.reset(
        new SequencedSocketData(reads, reads_count, writes, writes_count));
    session_deps_.socket_factory->AddSocketDataProvider(sequenced_data_.get());
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ = CreateSecureSpdySession(http_session_.get(), key, BoundNetLog());
  }

  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;
  scoped_ptr<SequencedSocketData> sequenced_data_;
  scoped_ptr<HttpNetworkSession> http_session_;

 private:
  SSLSocketDataProvider ssl_data_;
  base::WeakPtr<SpdySession> session_;
};

TEST_F(BidirectionalStreamTest, CreateInsecureStream) {
  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("http://www.example.org/");

  TestDelegateBase delegate(nullptr, 0);
  HttpNetworkSession::Params params =
      SpdySessionDependencies::CreateSessionParams(&session_deps_);
  scoped_ptr<HttpNetworkSession> session(new HttpNetworkSession(params));
  delegate.SetRunUntilCompletion(true);
  delegate.Start(std::move(request_info), session.get());

  EXPECT_EQ(ERR_DISALLOWED_URL_SCHEME, delegate.error());
}

// Simulates user calling ReadData after END_STREAM has been received in
// BidirectionalStreamSpdyJob.
TEST_F(BidirectionalStreamTest, TestReadDataAfterClose) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.example.org", false, 1, LOWEST));
  // Empty DATA frame with an END_STREAM flag.
  scoped_ptr<SpdyFrame> end_stream(
      spdy_util_.ConstructSpdyBodyFrame(1, nullptr, 0, true));
  MockWrite writes[] = {
      CreateMockWrite(*req.get(), 0),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(kExtraResponseHeaders, 1, 1));

  scoped_ptr<SpdyFrame> body_frame(spdy_util_.ConstructSpdyBodyFrame(1, false));
  // Last body frame has END_STREAM flag set.
  scoped_ptr<SpdyFrame> last_body_frame(
      spdy_util_.ConstructSpdyBodyFrame(1, true));

  MockRead reads[] = {
      CreateMockRead(*resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(*body_frame, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause.
      CreateMockRead(*body_frame, 5),
      CreateMockRead(*last_body_frame, 6),
      MockRead(SYNCHRONOUS, 0, 7),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.org/");
  request_info->end_stream_on_headers = true;
  request_info->priority = LOWEST;

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  // Create a MockTimer. Retain a raw pointer since the underlying
  // BidirectionalStreamJob owns it.
  MockTimer* timer = new MockTimer();
  scoped_ptr<TestDelegateBase> delegate(new TestDelegateBase(
      read_buffer.get(), kReadBufferSize, make_scoped_ptr(timer)));
  delegate->set_do_not_start_read(true);

  delegate->Start(std::move(request_info), http_session_.get());

  // Write request, and deliver response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_FALSE(timer->IsRunning());
  // ReadData returns asynchronously because no data is buffered.
  int rv = delegate->ReadData();
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Deliver a DATA frame.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  timer->Fire();
  // Asynchronous completion callback is invoke.
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(kUploadDataSize * 1,
            static_cast<int>(delegate->data_received().size()));

  // Deliver the rest. Note that user has not called a second ReadData.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  // ReadData now. Read should complete synchronously.
  rv = delegate->ReadData();
  EXPECT_EQ(kUploadDataSize * 2, rv);
  rv = delegate->ReadData();
  EXPECT_EQ(OK, rv);  // EOF.

  const SpdyHeaderBlock response_headers = delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, TestInterleaveReadDataAndSendData) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      "https://www.example.org", 1, kBodyDataSize * 3, LOWEST, nullptr, 0));
  scoped_ptr<SpdyFrame> data_frame1(
      framer.CreateDataFrame(1, kBodyData, kBodyDataSize, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> data_frame2(
      framer.CreateDataFrame(1, kBodyData, kBodyDataSize, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> data_frame3(
      framer.CreateDataFrame(1, kBodyData, kBodyDataSize, DATA_FLAG_FIN));
  MockWrite writes[] = {
      CreateMockWrite(*req, 0), CreateMockWrite(*data_frame1, 3),
      CreateMockWrite(*data_frame2, 6), CreateMockWrite(*data_frame3, 9),
  };

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(nullptr, 0, 1));
  scoped_ptr<SpdyFrame> response_body_frame1(
      spdy_util_.ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> response_body_frame2(
      spdy_util_.ConstructSpdyBodyFrame(1, true));

  MockRead reads[] = {
      CreateMockRead(*resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(*response_body_frame1, 4),
      MockRead(ASYNC, ERR_IO_PENDING, 5),  // Force a pause.
      CreateMockRead(*response_body_frame2, 7),
      MockRead(ASYNC, ERR_IO_PENDING, 8),  // Force a pause.
      MockRead(ASYNC, 0, 10),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "POST";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                        base::SizeTToString(kBodyDataSize * 3));

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  MockTimer* timer = new MockTimer();
  scoped_ptr<TestDelegateBase> delegate(new TestDelegateBase(
      read_buffer.get(), kReadBufferSize, make_scoped_ptr(timer)));
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Send the request and receive response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_FALSE(timer->IsRunning());

  // Send a DATA frame.
  scoped_refptr<StringIOBuffer> buf(
      new StringIOBuffer(std::string(kBodyData, kBodyDataSize)));

  // Send a DATA frame.
  delegate->SendData(buf.get(), buf->size(), false);
  // ReadData and it should return asynchronously because no data is buffered.
  int rv = delegate->ReadData();
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Deliver a DATA frame, and fire the timer.
  sequenced_data_->Resume();
  sequenced_data_->RunUntilPaused();
  timer->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate->on_data_sent_count());
  EXPECT_EQ(1, delegate->on_data_read_count());

  // Send a DATA frame.
  delegate->SendData(buf.get(), buf->size(), false);
  // ReadData and it should return asynchronously because no data is buffered.
  rv = delegate->ReadData();
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Deliver a DATA frame, and fire the timer.
  sequenced_data_->Resume();
  sequenced_data_->RunUntilPaused();
  timer->Fire();
  base::RunLoop().RunUntilIdle();
  // Last DATA frame is read. Server half closes.
  EXPECT_EQ(2, delegate->on_data_read_count());
  EXPECT_EQ(2, delegate->on_data_sent_count());

  // Send the last body frame. Client half closes.
  delegate->SendData(buf.get(), buf->size(), true);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, delegate->on_data_sent_count());

  // OnClose is invoked since both sides are closed.
  rv = delegate->ReadData();
  EXPECT_EQ(OK, rv);

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(2, delegate->on_data_read_count());
  EXPECT_EQ(3, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            delegate->GetTotalReceivedBytes());
}

// Tests that BidirectionalStreamSpdyJob::OnClose will complete any remaining
// read even if the read queue is empty.
TEST_F(BidirectionalStreamTest, TestCompleteAsyncRead) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.example.org", false, 1, LOWEST));
  // Empty DATA frame with an END_STREAM flag.
  scoped_ptr<SpdyFrame> end_stream(
      spdy_util_.ConstructSpdyBodyFrame(1, nullptr, 0, true));

  MockWrite writes[] = {CreateMockWrite(*req.get(), 0)};

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(nullptr, 0, 1));

  scoped_ptr<SpdyFrame> response_body_frame(
      spdy_util_.ConstructSpdyBodyFrame(1, nullptr, 0, true));

  MockRead reads[] = {
      CreateMockRead(*resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(*response_body_frame, 3), MockRead(SYNCHRONOUS, 0, 4),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  MockTimer* timer = new MockTimer();
  scoped_ptr<TestDelegateBase> delegate(new TestDelegateBase(
      read_buffer.get(), kReadBufferSize, make_scoped_ptr(timer)));
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Write request, and deliver response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_FALSE(timer->IsRunning());

  // ReadData should return asynchronously because no data is buffered.
  int rv = delegate->ReadData();
  EXPECT_EQ(ERR_IO_PENDING, rv);
  // Deliver END_STREAM.
  // OnClose should trigger completion of the remaining read.
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(0u, delegate->data_received().size());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, TestBuffering) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.example.org", false, 1, LOWEST));
  // Empty DATA frame with an END_STREAM flag.
  scoped_ptr<SpdyFrame> end_stream(
      spdy_util_.ConstructSpdyBodyFrame(1, nullptr, 0, true));

  MockWrite writes[] = {CreateMockWrite(*req.get(), 0)};

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(kExtraResponseHeaders, 1, 1));

  scoped_ptr<SpdyFrame> body_frame(spdy_util_.ConstructSpdyBodyFrame(1, false));
  // Last body frame has END_STREAM flag set.
  scoped_ptr<SpdyFrame> last_body_frame(
      spdy_util_.ConstructSpdyBodyFrame(1, true));

  MockRead reads[] = {
      CreateMockRead(*resp, 1),
      CreateMockRead(*body_frame, 2),
      CreateMockRead(*body_frame, 3),
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause.
      CreateMockRead(*last_body_frame, 5),
      MockRead(SYNCHRONOUS, 0, 6),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  MockTimer* timer = new MockTimer();
  scoped_ptr<TestDelegateBase> delegate(new TestDelegateBase(
      read_buffer.get(), kReadBufferSize, make_scoped_ptr(timer)));
  delegate->Start(std::move(request_info), http_session_.get());
  // Deliver two DATA frames together.
  sequenced_data_->RunUntilPaused();
  EXPECT_TRUE(timer->IsRunning());
  timer->Fire();
  base::RunLoop().RunUntilIdle();
  // This should trigger |more_read_data_pending_| to execute the task at a
  // later time, and Delegate::OnReadComplete should not have been called.
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(0, delegate->on_data_read_count());

  // Fire the timer now, the two DATA frame should be combined into one
  // single Delegate::OnReadComplete callback.
  timer->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(kUploadDataSize * 2,
            static_cast<int>(delegate->data_received().size()));

  // Deliver last DATA frame and EOF. There will be an additional
  // Delegate::OnReadComplete callback.
  sequenced_data_->Resume();
  EXPECT_EQ(2, delegate->on_data_read_count());
  EXPECT_EQ(kUploadDataSize * 3,
            static_cast<int>(delegate->data_received().size()));

  const SpdyHeaderBlock response_headers = delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, TestBufferingWithTrailers) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.example.org", false, 1, LOWEST));
  // Empty DATA frame with an END_STREAM flag.
  scoped_ptr<SpdyFrame> end_stream(
      spdy_util_.ConstructSpdyBodyFrame(1, nullptr, 0, true));

  MockWrite writes[] = {
      CreateMockWrite(*req.get(), 0),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(kExtraResponseHeaders, 1, 1));

  scoped_ptr<SpdyFrame> body_frame(spdy_util_.ConstructSpdyBodyFrame(1, false));

  const char* const kTrailers[] = {"foo", "bar"};
  scoped_ptr<SpdyFrame> trailers(
      spdy_util_.ConstructSpdyHeaderFrame(1, kTrailers, 1, true));

  MockRead reads[] = {
      CreateMockRead(*resp, 1),           CreateMockRead(*body_frame, 2),
      CreateMockRead(*body_frame, 3),     CreateMockRead(*body_frame, 4),
      MockRead(ASYNC, ERR_IO_PENDING, 5),  // Force a pause.
      CreateMockRead(*trailers, 6),       MockRead(SYNCHRONOUS, 0, 7),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  MockTimer* timer = new MockTimer();
  scoped_ptr<TestDelegateBase> delegate(new TestDelegateBase(
      read_buffer.get(), kReadBufferSize, make_scoped_ptr(timer)));

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  delegate->Start(std::move(request_info), http_session_.get());
  // Deliver all three DATA frames together.
  sequenced_data_->RunUntilPaused();
  EXPECT_TRUE(timer->IsRunning());
  timer->Fire();
  base::RunLoop().RunUntilIdle();
  // This should trigger |more_read_data_pending_| to execute the task at a
  // later time, and Delegate::OnReadComplete should not have been called.
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(0, delegate->on_data_read_count());

  // Deliver trailers. Remaining read should be completed, since OnClose is
  // called right after OnTrailersReceived. The three DATA frames should be
  // delivered in a single OnReadCompleted callback.
  sequenced_data_->Resume();
  EXPECT_EQ(1, delegate->on_data_read_count());
  EXPECT_EQ(kUploadDataSize * 3,
            static_cast<int>(delegate->data_received().size()));
  const SpdyHeaderBlock response_headers = delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ("bar", delegate->trailers().find("foo")->second);
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(CountWriteBytes(writes, arraysize(writes)),
            delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, CancelStreamAfterSendData) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      "https://www.example.org", 1, kBodyDataSize * 3, LOWEST, nullptr, 0));
  scoped_ptr<SpdyFrame> data_frame(
      framer.CreateDataFrame(1, kBodyData, kBodyDataSize, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));

  MockWrite writes[] = {
      CreateMockWrite(*req, 0), CreateMockWrite(*data_frame, 3),
      CreateMockWrite(*rst, 5),
  };

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(nullptr, 0, 1));
  scoped_ptr<SpdyFrame> response_body_frame(
      spdy_util_.ConstructSpdyBodyFrame(1, false));

  MockRead reads[] = {
      CreateMockRead(*resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      MockRead(ASYNC, ERR_IO_PENDING, 4),  // Force a pause.
      MockRead(ASYNC, 0, 6),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "POST";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                        base::SizeTToString(kBodyDataSize * 3));

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  scoped_ptr<TestDelegateBase> delegate(
      new TestDelegateBase(read_buffer.get(), kReadBufferSize));
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Send the request and receive response headers.
  sequenced_data_->RunUntilPaused();
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());

  // Send a DATA frame.
  scoped_refptr<StringIOBuffer> buf(
      new StringIOBuffer(std::string(kBodyData, kBodyDataSize)));
  delegate->SendData(buf.get(), buf->size(), false);
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();
  // Cancel the stream.
  delegate->CancelStream();
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  EXPECT_EQ(0, delegate->on_data_read_count());
  // EXPECT_EQ(1, delegate->on_data_send_count());
  // OnDataSent may or may not have been invoked.
  // Calling after stream is canceled gives kProtoUnknown.
  EXPECT_EQ(kProtoUnknown, delegate->GetProtocol());
  EXPECT_EQ(0, delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
}

TEST_F(BidirectionalStreamTest, CancelStreamDuringReadData) {
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      "https://www.example.org", 1, kBodyDataSize * 3, LOWEST, nullptr, 0));
  scoped_ptr<SpdyFrame> data_frame(
      framer.CreateDataFrame(1, kBodyData, kBodyDataSize, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));

  MockWrite writes[] = {
      CreateMockWrite(*req, 0), CreateMockWrite(*rst, 4),
  };

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(nullptr, 0, 1));
  scoped_ptr<SpdyFrame> response_body_frame(
      spdy_util_.ConstructSpdyBodyFrame(1, false));

  MockRead reads[] = {
      CreateMockRead(*resp, 1),
      MockRead(ASYNC, ERR_IO_PENDING, 2),  // Force a pause.
      CreateMockRead(*response_body_frame, 3), MockRead(ASYNC, 0, 5),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "POST";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                        base::SizeTToString(kBodyDataSize * 3));

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  scoped_ptr<TestDelegateBase> delegate(
      new TestDelegateBase(read_buffer.get(), kReadBufferSize));
  delegate->set_do_not_start_read(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Send the request and receive response headers.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("200", delegate->response_headers().find(":status")->second);
  // Cancel the stream after ReadData returns ERR_IO_PENDING.
  int rv = delegate->ReadData();
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  delegate->CancelStream();
  sequenced_data_->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  // Calling after stream is canceled gives kProtoUnknown.
  EXPECT_EQ(kProtoUnknown, delegate->GetProtocol());
  EXPECT_EQ(0, delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
}

// Receiving a header with uppercase ASCII will result in a protocol error,
// which should be propagated via Delegate::OnFailed.
TEST_F(BidirectionalStreamTest, PropagateProtocolError) {
  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      "https://www.example.org", 1, kBodyDataSize * 3, LOW, nullptr, 0));
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));

  MockWrite writes[] = {
      CreateMockWrite(*req, 0), CreateMockWrite(*rst, 2),
  };

  const char* const kExtraHeaders[] = {"X-UpperCase", "yes"};
  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(kExtraHeaders, 1, 1));

  MockRead reads[] = {
      CreateMockRead(*resp, 1), MockRead(ASYNC, 0, 3),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "POST";
  request_info->url = GURL("https://www.example.org/");
  request_info->extra_headers.SetHeader(net::HttpRequestHeaders::kContentLength,
                                        base::SizeTToString(kBodyDataSize * 3));

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  scoped_ptr<TestDelegateBase> delegate(
      new TestDelegateBase(read_buffer.get(), kReadBufferSize));
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, delegate->error());
  EXPECT_EQ(delegate->response_headers().end(),
            delegate->response_headers().find(":status"));
  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(kProtoHTTP2, delegate->GetProtocol());
  // BidirectionalStreamSpdyStreamJob does not count the bytes sent for |rst|
  // because it is sent after SpdyStream::Delegate::OnClose is called.
  EXPECT_EQ(CountWriteBytes(writes, 1), delegate->GetTotalSentBytes());
  EXPECT_EQ(CountReadBytes(reads, arraysize(reads)),
            delegate->GetTotalReceivedBytes());
}

INSTANTIATE_TEST_CASE_P(CancelOrDeleteTests,
                        BidirectionalStreamTest,
                        ::testing::Values(true, false));

TEST_P(BidirectionalStreamTest, CancelOrDeleteStreamDuringOnHeadersReceived) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.example.org", false, 1, LOWEST));

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*req, 0), CreateMockWrite(*rst, 2),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(kExtraResponseHeaders, 1, 1));

  MockRead reads[] = {
      CreateMockRead(*resp, 1), MockRead(ASYNC, 0, 3),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  scoped_ptr<CancelOrDeleteStreamDelegate> delegate(
      new CancelOrDeleteStreamDelegate(
          read_buffer.get(), kReadBufferSize,
          CancelOrDeleteStreamDelegate::Phase::ON_HEADERS_RECEIVED,
          GetParam()));
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Makes sure delegate does not get called.
  base::RunLoop().RunUntilIdle();
  const SpdyHeaderBlock response_headers = delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ(0u, delegate->data_received().size());
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(0, delegate->on_data_read_count());

  // If stream is destroyed, do not call into stream.
  if (!GetParam())
    return;
  EXPECT_EQ(0, delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
  EXPECT_EQ(kProtoUnknown, delegate->GetProtocol());
}

TEST_P(BidirectionalStreamTest, CancelOrDeleteStreamDuringOnDataRead) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.example.org", false, 1, LOWEST));

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*req, 0), CreateMockWrite(*rst, 3),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(kExtraResponseHeaders, 1, 1));

  scoped_ptr<SpdyFrame> response_body_frame(
      spdy_util_.ConstructSpdyBodyFrame(1, false));

  MockRead reads[] = {
      CreateMockRead(*resp, 1), CreateMockRead(*response_body_frame, 2),
      MockRead(ASYNC, 0, 4),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  scoped_ptr<CancelOrDeleteStreamDelegate> delegate(
      new CancelOrDeleteStreamDelegate(
          read_buffer.get(), kReadBufferSize,
          CancelOrDeleteStreamDelegate::Phase::ON_DATA_READ, GetParam()));
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Makes sure delegate does not get called.
  base::RunLoop().RunUntilIdle();
  const SpdyHeaderBlock response_headers = delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ(kUploadDataSize * 1,
            static_cast<int>(delegate->data_received().size()));
  EXPECT_EQ(0, delegate->on_data_sent_count());

  // If stream is destroyed, do not call into stream.
  if (!GetParam())
    return;
  EXPECT_EQ(0, delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
  EXPECT_EQ(kProtoUnknown, delegate->GetProtocol());
}

TEST_P(BidirectionalStreamTest, CancelOrDeleteStreamDuringOnTrailersReceived) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.example.org", false, 1, LOWEST));

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(*req, 0), CreateMockWrite(*rst, 4),
  };

  const char* const kExtraResponseHeaders[] = {"header-name", "header-value"};

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(kExtraResponseHeaders, 1, 1));

  scoped_ptr<SpdyFrame> response_body_frame(
      spdy_util_.ConstructSpdyBodyFrame(1, false));

  const char* const kTrailers[] = {"foo", "bar"};
  scoped_ptr<SpdyFrame> trailers(
      spdy_util_.ConstructSpdyHeaderFrame(1, kTrailers, 1, true));

  MockRead reads[] = {
      CreateMockRead(*resp, 1), CreateMockRead(*response_body_frame, 2),
      CreateMockRead(*trailers, 3), MockRead(ASYNC, 0, 5),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  scoped_ptr<CancelOrDeleteStreamDelegate> delegate(
      new CancelOrDeleteStreamDelegate(
          read_buffer.get(), kReadBufferSize,
          CancelOrDeleteStreamDelegate::Phase::ON_TRAILERS_RECEIVED,
          GetParam()));
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Makes sure delegate does not get called.
  base::RunLoop().RunUntilIdle();
  const SpdyHeaderBlock response_headers = delegate->response_headers();
  EXPECT_EQ("200", response_headers.find(":status")->second);
  EXPECT_EQ("header-value", response_headers.find("header-name")->second);
  EXPECT_EQ("bar", delegate->trailers().find("foo")->second);
  EXPECT_EQ(0, delegate->on_data_sent_count());
  // OnDataRead may or may not have been fired before the stream is
  // canceled/deleted.

  // If stream is destroyed, do not call into stream.
  if (!GetParam())
    return;
  EXPECT_EQ(0, delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
  EXPECT_EQ(kProtoUnknown, delegate->GetProtocol());
}

TEST_P(BidirectionalStreamTest, CancelOrDeleteStreamDuringOnFailed) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyGet("https://www.example.org", false, 1, LOWEST));

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));

  MockWrite writes[] = {
      CreateMockWrite(*req, 0), CreateMockWrite(*rst, 2),
  };

  const char* const kExtraHeaders[] = {"X-UpperCase", "yes"};
  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(kExtraHeaders, 1, 1));

  MockRead reads[] = {
      CreateMockRead(*resp, 1), MockRead(ASYNC, 0, 3),
  };

  HostPortPair host_port_pair("www.example.org", 443);
  SpdySessionKey key(host_port_pair, ProxyServer::Direct(),
                     PRIVACY_MODE_DISABLED);
  InitSession(reads, arraysize(reads), writes, arraysize(writes), key);

  scoped_ptr<BidirectionalStreamRequestInfo> request_info(
      new BidirectionalStreamRequestInfo);
  request_info->method = "GET";
  request_info->url = GURL("https://www.example.org/");
  request_info->priority = LOWEST;
  request_info->end_stream_on_headers = true;

  scoped_refptr<IOBuffer> read_buffer(new IOBuffer(kReadBufferSize));
  scoped_ptr<CancelOrDeleteStreamDelegate> delegate(
      new CancelOrDeleteStreamDelegate(
          read_buffer.get(), kReadBufferSize,
          CancelOrDeleteStreamDelegate::Phase::ON_FAILED, GetParam()));
  delegate->SetRunUntilCompletion(true);
  delegate->Start(std::move(request_info), http_session_.get());
  // Makes sure delegate does not get called.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(delegate->response_headers().end(),
            delegate->response_headers().find(":status"));
  EXPECT_EQ(0, delegate->on_data_sent_count());
  EXPECT_EQ(0, delegate->on_data_read_count());
  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, delegate->error());

  // If stream is destroyed, do not call into stream.
  if (!GetParam())
    return;
  EXPECT_EQ(0, delegate->GetTotalSentBytes());
  EXPECT_EQ(0, delegate->GetTotalReceivedBytes());
  EXPECT_EQ(kProtoUnknown, delegate->GetProtocol());
}

}  // namespace net
