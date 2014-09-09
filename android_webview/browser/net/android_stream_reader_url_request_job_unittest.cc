// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/input_stream.h"
#include "android_webview/browser/net/android_stream_reader_url_request_job.h"
#include "android_webview/browser/net/aw_url_request_job_factory.h"
#include "android_webview/browser/net/input_stream_reader.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "net/base/request_priority.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using android_webview::InputStream;
using android_webview::InputStreamReader;
using net::TestDelegate;
using net::TestJobInterceptor;
using net::TestNetworkDelegate;
using net::TestURLRequestContext;
using net::URLRequest;
using testing::DoAll;
using testing::Ge;
using testing::Gt;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::Test;
using testing::WithArg;
using testing::WithArgs;
using testing::_;

// Some of the classes will DCHECK on a null InputStream (which is desirable).
// The workaround is to use this class. None of the methods need to be
// implemented as the mock InputStreamReader should never forward calls to the
// InputStream.
class NotImplInputStream : public InputStream {
 public:
  NotImplInputStream() {}
  virtual ~NotImplInputStream() {}
  virtual bool BytesAvailable(int* bytes_available) const OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual bool Skip(int64_t n, int64_t* bytes_skipped) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual bool Read(net::IOBuffer* dest, int length, int* bytes_read) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
};

// Required in order to create an instance of AndroidStreamReaderURLRequestJob.
class StreamReaderDelegate :
    public AndroidStreamReaderURLRequestJob::Delegate {
 public:
  StreamReaderDelegate() {}

  virtual scoped_ptr<InputStream> OpenInputStream(
      JNIEnv* env,
      const GURL& url) OVERRIDE {
    return make_scoped_ptr<InputStream>(new NotImplInputStream());
  }

  virtual void OnInputStreamOpenFailed(net::URLRequest* request,
                                       bool* restart) OVERRIDE {
    *restart = false;
  }

  virtual bool GetMimeType(JNIEnv* env,
                           net::URLRequest* request,
                           android_webview::InputStream* stream,
                           std::string* mime_type) OVERRIDE {
    return false;
  }

  virtual bool GetCharset(JNIEnv* env,
                          net::URLRequest* request,
                          android_webview::InputStream* stream,
                          std::string* charset) OVERRIDE {
    return false;
  }

  virtual void AppendResponseHeaders(
      JNIEnv* env,
      net::HttpResponseHeaders* headers) OVERRIDE {
    // no-op
  }
};

class NullStreamReaderDelegate : public StreamReaderDelegate {
 public:
  NullStreamReaderDelegate() {}

  virtual scoped_ptr<InputStream> OpenInputStream(
      JNIEnv* env,
      const GURL& url) OVERRIDE {
    return make_scoped_ptr<InputStream>(NULL);
  }
};

class HeaderAlteringStreamReaderDelegate : public NullStreamReaderDelegate {
 public:
  HeaderAlteringStreamReaderDelegate() {}

  virtual void AppendResponseHeaders(
      JNIEnv* env,
      net::HttpResponseHeaders* headers) OVERRIDE {
    headers->ReplaceStatusLine(kStatusLine);
    std::string headerLine(kCustomHeaderName);
    headerLine.append(": ");
    headerLine.append(kCustomHeaderValue);
    headers->AddHeader(headerLine);
  }

  static const int kResponseCode;
  static const char* kStatusLine;
  static const char* kCustomHeaderName;
  static const char* kCustomHeaderValue;
};

const int HeaderAlteringStreamReaderDelegate::kResponseCode = 401;
const char* HeaderAlteringStreamReaderDelegate::kStatusLine =
    "HTTP/1.1 401 Gone";
const char* HeaderAlteringStreamReaderDelegate::kCustomHeaderName =
    "X-Test-Header";
const char* HeaderAlteringStreamReaderDelegate::kCustomHeaderValue =
    "TestHeaderValue";

class MockInputStreamReader : public InputStreamReader {
 public:
  MockInputStreamReader() : InputStreamReader(new NotImplInputStream()) {}
  ~MockInputStreamReader() {}

  MOCK_METHOD1(Seek, int(const net::HttpByteRange& byte_range));
  MOCK_METHOD2(ReadRawData, int(net::IOBuffer* buffer, int buffer_size));
};


class TestStreamReaderJob : public AndroidStreamReaderURLRequestJob {
 public:
  TestStreamReaderJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      scoped_ptr<Delegate> delegate,
      scoped_ptr<InputStreamReader> stream_reader)
      : AndroidStreamReaderURLRequestJob(request,
                                         network_delegate,
                                         delegate.Pass()),
        stream_reader_(stream_reader.Pass()) {
    message_loop_proxy_ = base::MessageLoopProxy::current();
  }

  virtual scoped_ptr<InputStreamReader> CreateStreamReader(
      InputStream* stream) OVERRIDE {
    return stream_reader_.Pass();
  }
 protected:
  virtual ~TestStreamReaderJob() {}

  virtual base::TaskRunner* GetWorkerThreadRunner() OVERRIDE {
    return message_loop_proxy_.get();
  }

  scoped_ptr<InputStreamReader> stream_reader_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};

class AndroidStreamReaderURLRequestJobTest : public Test {
 public:
  AndroidStreamReaderURLRequestJobTest() {}

 protected:
  virtual void SetUp() {
    context_.set_job_factory(&factory_);
    context_.set_network_delegate(&network_delegate_);
    req_ = context_.CreateRequest(GURL("content://foo"),
                                  net::DEFAULT_PRIORITY,
                                  &url_request_delegate_,
                                  NULL);
    req_->set_method("GET");
  }

  void SetRange(net::URLRequest* req, int first_byte, int last_byte) {
    net::HttpRequestHeaders headers;
    headers.SetHeader(net::HttpRequestHeaders::kRange,
                      net::HttpByteRange::Bounded(
                          first_byte, last_byte).GetHeaderValue());
    req->SetExtraRequestHeaders(headers);
  }

  void SetUpTestJob(scoped_ptr<InputStreamReader> stream_reader) {
    SetUpTestJob(stream_reader.Pass(),
                 make_scoped_ptr(new StreamReaderDelegate())
                     .PassAs<AndroidStreamReaderURLRequestJob::Delegate>());
  }

  void SetUpTestJob(scoped_ptr<InputStreamReader> stream_reader,
                    scoped_ptr<AndroidStreamReaderURLRequestJob::Delegate>
                        stream_reader_delegate) {
    TestStreamReaderJob* test_stream_reader_job =
        new TestStreamReaderJob(
            req_.get(),
            &network_delegate_,
            stream_reader_delegate.Pass(),
            stream_reader.Pass());
    // The Interceptor is owned by the |factory_|.
    TestJobInterceptor* protocol_handler = new TestJobInterceptor;
    protocol_handler->set_main_intercept_job(test_stream_reader_job);
    bool set_protocol = factory_.SetProtocolHandler("http", protocol_handler);
    DCHECK(set_protocol);

    protocol_handler = new TestJobInterceptor;
    protocol_handler->set_main_intercept_job(test_stream_reader_job);
    set_protocol = factory_.SetProtocolHandler("content", protocol_handler);
    DCHECK(set_protocol);
  }

  base::MessageLoopForIO loop_;
  TestURLRequestContext context_;
  android_webview::AwURLRequestJobFactory factory_;
  TestDelegate url_request_delegate_;
  TestNetworkDelegate network_delegate_;
  scoped_ptr<URLRequest> req_;
};

TEST_F(AndroidStreamReaderURLRequestJobTest, ReadEmptyStream) {
  scoped_ptr<StrictMock<MockInputStreamReader> > stream_reader(
      new StrictMock<MockInputStreamReader>());
  {
    InSequence s;
    EXPECT_CALL(*stream_reader, Seek(_))
        .WillOnce(Return(0));
    EXPECT_CALL(*stream_reader, ReadRawData(NotNull(), Gt(0)))
        .WillOnce(Return(0));
  }

  SetUpTestJob(stream_reader.PassAs<InputStreamReader>());

  req_->Start();

  // The TestDelegate will quit the message loop on request completion.
  base::MessageLoop::current()->Run();

  EXPECT_FALSE(url_request_delegate_.request_failed());
  EXPECT_EQ(1, network_delegate_.completed_requests());
  EXPECT_EQ(0, network_delegate_.error_count());
  EXPECT_EQ(200, req_->GetResponseCode());
}

TEST_F(AndroidStreamReaderURLRequestJobTest, ReadWithNullStream) {
  SetUpTestJob(scoped_ptr<InputStreamReader>(),
               make_scoped_ptr(new NullStreamReaderDelegate())
                   .PassAs<AndroidStreamReaderURLRequestJob::Delegate>());
  req_->Start();

  // The TestDelegate will quit the message loop on request completion.
  base::MessageLoop::current()->Run();

  // The request_failed() method is named confusingly but all it checks is
  // whether the request got as far as calling NotifyHeadersComplete.
  EXPECT_FALSE(url_request_delegate_.request_failed());
  EXPECT_EQ(1, network_delegate_.completed_requests());
  // A null input stream shouldn't result in an error. See crbug.com/180950.
  EXPECT_EQ(0, network_delegate_.error_count());
  EXPECT_EQ(404, req_->GetResponseCode());
}

TEST_F(AndroidStreamReaderURLRequestJobTest, ModifyHeadersAndStatus) {
  SetUpTestJob(scoped_ptr<InputStreamReader>(),
               make_scoped_ptr(new HeaderAlteringStreamReaderDelegate())
                   .PassAs<AndroidStreamReaderURLRequestJob::Delegate>());
  req_->Start();

  // The TestDelegate will quit the message loop on request completion.
  base::MessageLoop::current()->Run();

  // The request_failed() method is named confusingly but all it checks is
  // whether the request got as far as calling NotifyHeadersComplete.
  EXPECT_FALSE(url_request_delegate_.request_failed());
  EXPECT_EQ(1, network_delegate_.completed_requests());
  // A null input stream shouldn't result in an error. See crbug.com/180950.
  EXPECT_EQ(0, network_delegate_.error_count());
  EXPECT_EQ(HeaderAlteringStreamReaderDelegate::kResponseCode,
            req_->GetResponseCode());
  EXPECT_EQ(HeaderAlteringStreamReaderDelegate::kStatusLine,
            req_->response_headers()->GetStatusLine());
  EXPECT_TRUE(req_->response_headers()->HasHeader(
      HeaderAlteringStreamReaderDelegate::kCustomHeaderName));
  std::string header_value;
  EXPECT_TRUE(req_->response_headers()->EnumerateHeader(
      NULL, HeaderAlteringStreamReaderDelegate::kCustomHeaderName,
      &header_value));
  EXPECT_EQ(HeaderAlteringStreamReaderDelegate::kCustomHeaderValue,
            header_value);
}

TEST_F(AndroidStreamReaderURLRequestJobTest, ReadPartOfStream) {
  const int bytes_available = 128;
  const int offset = 32;
  const int bytes_to_read = bytes_available - offset;
  scoped_ptr<StrictMock<MockInputStreamReader> > stream_reader(
      new StrictMock<MockInputStreamReader>());
  {
    InSequence s;
    EXPECT_CALL(*stream_reader, Seek(_))
        .WillOnce(Return(bytes_available));
    EXPECT_CALL(*stream_reader, ReadRawData(NotNull(), Ge(bytes_to_read)))
        .WillOnce(Return(bytes_to_read/2));
    EXPECT_CALL(*stream_reader, ReadRawData(NotNull(), Ge(bytes_to_read)))
        .WillOnce(Return(bytes_to_read/2));
    EXPECT_CALL(*stream_reader, ReadRawData(NotNull(), Ge(bytes_to_read)))
        .WillOnce(Return(0));
  }

  SetUpTestJob(stream_reader.PassAs<InputStreamReader>());

  SetRange(req_.get(), offset, bytes_available);
  req_->Start();

  base::MessageLoop::current()->Run();

  EXPECT_FALSE(url_request_delegate_.request_failed());
  EXPECT_EQ(bytes_to_read, url_request_delegate_.bytes_received());
  EXPECT_EQ(1, network_delegate_.completed_requests());
  EXPECT_EQ(0, network_delegate_.error_count());
}

TEST_F(AndroidStreamReaderURLRequestJobTest,
       ReadStreamWithMoreAvailableThanActual) {
  const int bytes_available_reported = 190;
  const int bytes_available = 128;
  const int offset = 0;
  const int bytes_to_read = bytes_available - offset;
  scoped_ptr<StrictMock<MockInputStreamReader> > stream_reader(
      new StrictMock<MockInputStreamReader>());
  {
    InSequence s;
    EXPECT_CALL(*stream_reader, Seek(_))
        .WillOnce(Return(bytes_available_reported));
    EXPECT_CALL(*stream_reader, ReadRawData(NotNull(), Ge(bytes_to_read)))
        .WillOnce(Return(bytes_available));
    EXPECT_CALL(*stream_reader, ReadRawData(NotNull(), Ge(bytes_to_read)))
        .WillOnce(Return(0));
  }

  SetUpTestJob(stream_reader.PassAs<InputStreamReader>());

  SetRange(req_.get(), offset, bytes_available_reported);
  req_->Start();

  base::MessageLoop::current()->Run();

  EXPECT_FALSE(url_request_delegate_.request_failed());
  EXPECT_EQ(bytes_to_read, url_request_delegate_.bytes_received());
  EXPECT_EQ(1, network_delegate_.completed_requests());
  EXPECT_EQ(0, network_delegate_.error_count());
}

TEST_F(AndroidStreamReaderURLRequestJobTest, DeleteJobMidWaySeek) {
  const int offset = 20;
  const int bytes_available = 128;
  base::RunLoop loop;
  scoped_ptr<StrictMock<MockInputStreamReader> > stream_reader(
      new StrictMock<MockInputStreamReader>());
  EXPECT_CALL(*stream_reader, Seek(_))
      .WillOnce(DoAll(InvokeWithoutArgs(&loop, &base::RunLoop::Quit),
                      Return(bytes_available)));
  ON_CALL(*stream_reader, ReadRawData(_, _))
      .WillByDefault(Return(0));

  SetUpTestJob(stream_reader.PassAs<InputStreamReader>());

  SetRange(req_.get(), offset, bytes_available);
  req_->Start();

  loop.Run();

  EXPECT_EQ(0, network_delegate_.completed_requests());
  req_->Cancel();
  EXPECT_EQ(1, network_delegate_.completed_requests());
}

TEST_F(AndroidStreamReaderURLRequestJobTest, DeleteJobMidWayRead) {
  const int offset = 20;
  const int bytes_available = 128;
  base::RunLoop loop;
  scoped_ptr<StrictMock<MockInputStreamReader> > stream_reader(
      new StrictMock<MockInputStreamReader>());
  net::CompletionCallback read_completion_callback;
  EXPECT_CALL(*stream_reader, Seek(_))
      .WillOnce(Return(bytes_available));
  EXPECT_CALL(*stream_reader, ReadRawData(_, _))
      .WillOnce(DoAll(InvokeWithoutArgs(&loop, &base::RunLoop::Quit),
                      Return(bytes_available)));

  SetUpTestJob(stream_reader.PassAs<InputStreamReader>());

  SetRange(req_.get(), offset, bytes_available);
  req_->Start();

  loop.Run();

  EXPECT_EQ(0, network_delegate_.completed_requests());
  req_->Cancel();
  EXPECT_EQ(1, network_delegate_.completed_requests());
}
