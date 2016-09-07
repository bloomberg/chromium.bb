// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/intercepting_resource_handler.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class TestResourceHandler : public ResourceHandler {
 public:
  // A test version of a ResourceHandler. |request_status| and
  // |final_bytes_read| will be updated when the response is complete with the
  // final status of the request received by the handler, and the total bytes
  // the handler saw.
  explicit TestResourceHandler(net::URLRequestStatus* request_status,
                               size_t* final_bytes_read)
      : TestResourceHandler(request_status,
                            final_bytes_read,
                            true /* on_response_started_result */,
                            true /* on_will_read_result */,
                            true /* on_read_completed_result */) {}

  // This constructor allows to specify return values for OnResponseStarted,
  // OnWillRead and OnReadCompleted.
  TestResourceHandler(net::URLRequestStatus* request_status,
                      size_t* final_bytes_read,
                      bool on_response_started_result,
                      bool on_will_read_result,
                      bool on_read_completed_result)
      : ResourceHandler(nullptr),
        buffer_(new net::IOBuffer(2048)),
        request_status_(request_status),
        final_bytes_read_(final_bytes_read),
        on_response_started_result_(on_response_started_result),
        on_will_read_result_(on_will_read_result),
        on_read_completed_result_(on_read_completed_result),
        bytes_read_(0),
        is_completed_(false) {
    memset(buffer_->data(), '\0', 2048);
  }

  ~TestResourceHandler() override {}

  void SetController(ResourceController* controller) override {}

  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override {
    NOTREACHED();
    return false;
  }

  bool OnResponseStarted(ResourceResponse* response, bool* defer) override {
    EXPECT_FALSE(is_completed_);
    return on_response_started_result_;
  }

  bool OnWillStart(const GURL& url, bool* defer) override {
    EXPECT_FALSE(is_completed_);
    return true;
  }

  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override {
    EXPECT_FALSE(is_completed_);
    *buf = buffer_;
    *buf_size = 2048;
    memset(buffer_->data(), '\0', 2048);
    return on_will_read_result_;
  }

  bool OnReadCompleted(int bytes_read, bool* defer) override {
    EXPECT_FALSE(is_completed_);
    EXPECT_LT(bytes_read, 2048);
    bytes_read_ += bytes_read;
    return on_read_completed_result_;
  }

  void OnResponseCompleted(const net::URLRequestStatus& status,
                           bool* defer) override {
    EXPECT_FALSE(is_completed_);
    is_completed_ = true;
    *request_status_ = status;
    *final_bytes_read_ = bytes_read_;
  }

  void OnDataDownloaded(int bytes_downloaded) override { NOTREACHED(); }

  scoped_refptr<net::IOBuffer> buffer() const { return buffer_; }

  size_t bytes_read() const { return bytes_read_; }

 private:
  scoped_refptr<net::IOBuffer> buffer_;
  net::URLRequestStatus* request_status_;
  size_t* final_bytes_read_;
  bool on_response_started_result_;
  bool on_will_read_result_;
  bool on_read_completed_result_;
  size_t bytes_read_;
  bool is_completed_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceHandler);
};

class InterceptingResourceHandlerTest : public testing::Test {
 public:
  InterceptingResourceHandlerTest() {}

 private:
  TestBrowserThreadBundle thread_bundle_;
};

// Tests that the handler behaves properly when it doesn't have to use an
// alternate next handler.
TEST_F(InterceptingResourceHandlerTest, NoSwitching) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  size_t old_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> old_handler(new TestResourceHandler(
      &old_handler_status, &old_handler_final_bytes_read));
  TestResourceHandler* old_test_handler = old_handler.get();
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const std::string kData = "The data";
  EXPECT_NE(kData, std::string(old_test_handler->buffer()->data()));

  ASSERT_EQ(read_buffer.get(), old_test_handler->buffer());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());
  memcpy(read_buffer->data(), kData.c_str(), kData.length());

  // The response is received. The handler should not change.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);

  // The read is replayed by the MimeSniffingResourceHandler. The data should
  // have been received by the old intercepting_handler.
  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData, std::string(old_test_handler->buffer()->data()));

  // Make sure another read behave as expected.
  buf_size = 0;
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));
  ASSERT_EQ(read_buffer.get(), old_test_handler->buffer());

  const std::string kData2 = "Data 2";
  EXPECT_NE(kData, std::string(old_test_handler->buffer()->data()));
  ASSERT_GT(static_cast<size_t>(buf_size), kData2.length());
  memcpy(read_buffer->data(), kData2.c_str(), kData2.length());

  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData2.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData2, std::string(old_test_handler->buffer()->data()));
  EXPECT_EQ(kData.length() + kData2.length(), old_test_handler->bytes_read());
}

// Tests that the data received is transmitted to the newly created
// ResourceHandler.
TEST_F(InterceptingResourceHandlerTest, HandlerSwitchNoPayload) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  size_t old_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> old_handler(new TestResourceHandler(
      &old_handler_status, &old_handler_final_bytes_read));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const std::string kData = "The data";
  ASSERT_EQ(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());
  memcpy(read_buffer->data(), kData.c_str(), kData.length());

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  size_t new_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> new_handler_scoped(
      new TestResourceHandler(&new_handler_status,
                              &new_handler_final_bytes_read));
  TestResourceHandler* new_test_handler = new_handler_scoped.get();
  intercepting_handler->UseNewHandler(std::move(new_handler_scoped),
                                      std::string());

  // The response is received. The new ResourceHandler should be used handle
  // the download.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);

  EXPECT_FALSE(old_handler_status.is_success());
  EXPECT_EQ(net::ERR_ABORTED, old_handler_status.error());
  EXPECT_EQ(0ul, old_handler_final_bytes_read);

  // It should not have received the download data yet.
  EXPECT_NE(kData, std::string(new_test_handler->buffer()->data()));

  // The read is replayed by the MimeSniffingResourceHandler. The data should
  // have been received by the new handler.
  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData, std::string(new_test_handler->buffer()->data()));

  // Make sure another read behaves as expected.
  buf_size = 0;
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));
  ASSERT_EQ(read_buffer.get(), new_test_handler->buffer());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());

  const std::string kData2 = "Data 2";
  EXPECT_NE(kData2, std::string(new_test_handler->buffer()->data()));
  memcpy(read_buffer->data(), kData2.c_str(), kData2.length());

  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData2.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData2, std::string(new_test_handler->buffer()->data()));
  EXPECT_EQ(kData.length() + kData2.length(), new_test_handler->bytes_read());
}

// Tests that the data received is transmitted to the newly created
// ResourceHandler and the specified payload to the old ResourceHandler.
TEST_F(InterceptingResourceHandlerTest, HandlerSwitchWithPayload) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  size_t old_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> old_handler_scoped(
      new TestResourceHandler(&old_handler_status,
                              &old_handler_final_bytes_read));
  TestResourceHandler* old_handler = old_handler_scoped.get();
  scoped_refptr<net::IOBuffer> old_buffer = old_handler->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler_scoped),
                                      request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const std::string kData = "The data";
  ASSERT_EQ(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());
  memcpy(read_buffer->data(), kData.c_str(), kData.length());

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  const std::string kPayload = "The payload";
  net::URLRequestStatus new_handler_status;
  size_t new_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> new_handler_scoped(
      new TestResourceHandler(&new_handler_status,
                              &new_handler_final_bytes_read));
  TestResourceHandler* new_test_handler = new_handler_scoped.get();
  intercepting_handler->UseNewHandler(std::move(new_handler_scoped), kPayload);

  // The old handler should not have received the payload yet.
  ASSERT_FALSE(old_handler->bytes_read());
  EXPECT_NE(kPayload, std::string(old_buffer->data()));

  // The response is received. The new ResourceHandler should be used to handle
  // the download.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);

  // The old handler should have received the payload.
  ASSERT_TRUE(old_handler_final_bytes_read == kPayload.size());
  EXPECT_EQ(kPayload, std::string(old_buffer->data()));

  EXPECT_TRUE(old_handler_status.is_success());
  EXPECT_EQ(net::OK, old_handler_status.error());

  // It should not have received the download data yet.
  EXPECT_NE(kData, std::string(new_test_handler->buffer()->data()));

  // The read is replayed by the MimeSniffingResourceHandler. The data should
  // have been received by the new handler.
  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData, std::string(new_test_handler->buffer()->data()));

  // Make sure another read behave as expected.
  buf_size = 0;
  const std::string kData2 = "Data 2";
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));
  ASSERT_EQ(read_buffer.get(), new_test_handler->buffer());
  ASSERT_GT(static_cast<size_t>(buf_size), kData.length());

  EXPECT_NE(kData2, std::string(new_test_handler->buffer()->data()));
  memcpy(read_buffer->data(), kData2.c_str(), kData2.length());

  EXPECT_TRUE(intercepting_handler->OnReadCompleted(kData2.length(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(kData2, std::string(new_test_handler->buffer()->data()));
  EXPECT_EQ(kData.length() + kData2.length(), new_test_handler->bytes_read());
}

// Tests that the handler behaves properly if the old handler fails will read.
TEST_F(InterceptingResourceHandlerTest, OldHandlerFailsWillRead) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  size_t old_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> old_handler(new TestResourceHandler(
      &old_handler_status, &old_handler_final_bytes_read,
      true,    // on_response_started
      false,   // on_will_read
      true));  // on_read_completed
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data. The old
  // handler should tell the caller to fail.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_FALSE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));
}

// Tests that the handler behaves properly if the new handler fails response
// started.
TEST_F(InterceptingResourceHandlerTest, NewHandlerFailsResponseStarted) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  size_t old_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> old_handler(new TestResourceHandler(
      &old_handler_status, &old_handler_final_bytes_read));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_EQ(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), sizeof(kData));
  memcpy(read_buffer->data(), kData, sizeof(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  size_t new_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> new_handler(new TestResourceHandler(
      &new_handler_status, &new_handler_final_bytes_read,
      false,   // on_response_started
      true,    // on_will_read
      true));  // on_read_completed
  intercepting_handler->UseNewHandler(std::move(new_handler), std::string());

  // The response is received. The new ResourceHandler should tell us to fail.
  EXPECT_FALSE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);
}

// Tests that the handler behaves properly if the new handler fails will read.
TEST_F(InterceptingResourceHandlerTest, NewHandlerFailsWillRead) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  size_t old_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> old_handler(new TestResourceHandler(
      &old_handler_status, &old_handler_final_bytes_read));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_EQ(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), sizeof(kData));
  memcpy(read_buffer->data(), kData, sizeof(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  size_t new_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> new_handler(new TestResourceHandler(
      &new_handler_status, &new_handler_final_bytes_read,
      true,    // on_response_started
      false,   // on_will_read
      true));  // on_read_completed
  intercepting_handler->UseNewHandler(std::move(new_handler), std::string());

  // The response is received. The new handler should not have been asked to
  // read yet.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(net::URLRequestStatus::CANCELED, old_handler_status.status());
  EXPECT_EQ(net::ERR_ABORTED, old_handler_status.error());

  // The read is replayed by the MimeSniffingResourceHandler. The new
  // handler should tell the caller to fail.
  EXPECT_FALSE(intercepting_handler->OnReadCompleted(sizeof(kData), &defer));
  EXPECT_FALSE(defer);
}

// Tests that the handler behaves properly if the new handler fails read
// completed.
TEST_F(InterceptingResourceHandlerTest, NewHandlerFailsReadCompleted) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  ResourceRequestInfo::AllocateForTesting(request.get(),
                                          RESOURCE_TYPE_MAIN_FRAME,
                                          nullptr,  // context
                                          0,        // render_process_id
                                          0,        // render_view_id
                                          0,        // render_frame_id
                                          true,     // is_main_frame
                                          false,    // parent_is_main_frame
                                          true,     // allow_download
                                          true,     // is_async
                                          false);   // is_using_lofi

  net::URLRequestStatus old_handler_status;
  size_t old_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> old_handler(new TestResourceHandler(
      &old_handler_status, &old_handler_final_bytes_read));
  scoped_refptr<net::IOBuffer> old_buffer = old_handler.get()->buffer();
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(std::move(old_handler), request.get()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  // Simulate the MimeSniffingResourceHandler buffering the data.
  scoped_refptr<net::IOBuffer> read_buffer;
  int buf_size = 0;
  bool defer = false;
  EXPECT_TRUE(intercepting_handler->OnWillStart(GURL(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_TRUE(intercepting_handler->OnWillRead(&read_buffer, &buf_size, -1));

  const char kData[] = "The data";
  ASSERT_EQ(read_buffer.get(), old_buffer.get());
  ASSERT_GT(static_cast<size_t>(buf_size), sizeof(kData));
  memcpy(read_buffer->data(), kData, sizeof(kData));

  // Simulate the MimeSniffingResourceHandler asking the
  // InterceptingResourceHandler to switch to a new handler.
  net::URLRequestStatus new_handler_status;
  size_t new_handler_final_bytes_read = 0;
  std::unique_ptr<TestResourceHandler> new_handler(new TestResourceHandler(
      &new_handler_status, &new_handler_final_bytes_read,
      true,     // on_response_started
      true,     // on_will_read
      false));  // on_read_completed
  intercepting_handler->UseNewHandler(std::move(new_handler), std::string());

  // The response is received.
  EXPECT_TRUE(intercepting_handler->OnResponseStarted(response.get(), &defer));
  EXPECT_FALSE(defer);
  EXPECT_EQ(net::URLRequestStatus::CANCELED, old_handler_status.status());
  EXPECT_EQ(net::ERR_ABORTED, old_handler_status.error());

  // The read is replayed by the MimeSniffingResourceHandler. The new handler
  // should tell the caller to fail.
  EXPECT_FALSE(intercepting_handler->OnReadCompleted(sizeof(kData), &defer));
  EXPECT_FALSE(defer);
}

}  // namespace

}  // namespace content
