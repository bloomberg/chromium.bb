// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/buffered_resource_handler.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_response.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/fake_plugin_service.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

class TestResourceHandler : public ResourceHandler {
 public:
  TestResourceHandler() : ResourceHandler(nullptr) {}

  void SetController(ResourceController* controller) override {}

  bool OnUploadProgress(uint64 position, uint64 size) override {
    NOTREACHED();
    return false;
  }

  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override {
    NOTREACHED();
    return false;
  }

  bool OnResponseStarted(ResourceResponse* response, bool* defer) override {
    return false;
  }

  bool OnWillStart(const GURL& url, bool* defer) override {
    NOTREACHED();
    return false;
  }

  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override {
    NOTREACHED();
    return false;
  }

  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override {
    NOTREACHED();
    return false;
  }

  bool OnReadCompleted(int bytes_read, bool* defer) override {
    NOTREACHED();
    return false;
  }

  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override {
  }

  void OnDataDownloaded(int bytes_downloaded) override {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestResourceHandler);
};

class TestResourceDispatcherHost : public ResourceDispatcherHostImpl {
 public:
  explicit TestResourceDispatcherHost(bool stream_has_handler)
      : stream_has_handler_(stream_has_handler),
        intercepted_as_stream_(false) {}

  bool intercepted_as_stream() const { return intercepted_as_stream_; }

  scoped_ptr<ResourceHandler> CreateResourceHandlerForDownload(
      net::URLRequest* request,
      bool is_content_initiated,
      bool must_download,
      uint32 id,
      scoped_ptr<DownloadSaveInfo> save_info,
      const DownloadUrlParameters::OnStartedCallback& started_cb) override {
    return scoped_ptr<ResourceHandler>(new TestResourceHandler).Pass();
  }

  scoped_ptr<ResourceHandler> MaybeInterceptAsStream(
      net::URLRequest* request,
      ResourceResponse* response,
      std::string* payload) override {
    if (stream_has_handler_) {
      intercepted_as_stream_ = true;
      return scoped_ptr<ResourceHandler>(new TestResourceHandler).Pass();
    } else {
      return scoped_ptr<ResourceHandler>();
    }
  }

 private:
  // Whether the URL request should be intercepted as a stream.
  bool stream_has_handler_;

  // Whether the URL request has been intercepted as a stream.
  bool intercepted_as_stream_;
};

class TestResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  TestResourceDispatcherHostDelegate(bool must_download)
      : must_download_(must_download) {
  }

  bool ShouldForceDownloadResource(const GURL& url,
                                   const std::string& mime_type) override {
    return must_download_;
  }

 private:
  const bool must_download_;
};

class TestResourceController : public ResourceController {
 public:
  void Cancel() override {}

  void CancelAndIgnore() override {
    NOTREACHED();
  }

  void CancelWithError(int error_code) override {
    NOTREACHED();
  }

  void Resume() override {
    NOTREACHED();
  }
};

class BufferedResourceHandlerTest : public testing::Test {
 public:
  BufferedResourceHandlerTest() : stream_has_handler_(false) {}

  void set_stream_has_handler(bool stream_has_handler) {
    stream_has_handler_ = stream_has_handler;
  }

  bool TestStreamIsIntercepted(bool allow_download,
                               bool must_download,
                               ResourceType request_resource_type);

 private:
  // Whether the URL request should be intercepted as a stream.
  bool stream_has_handler_;

  TestBrowserThreadBundle thread_bundle_;
};

bool BufferedResourceHandlerTest::TestStreamIsIntercepted(
    bool allow_download,
    bool must_download,
    ResourceType request_resource_type) {
  net::URLRequestContext context;
  scoped_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr, nullptr));
  bool is_main_frame = request_resource_type == RESOURCE_TYPE_MAIN_FRAME;
  ResourceRequestInfo::AllocateForTesting(
      request.get(),
      request_resource_type,
      nullptr,          // context
      0,                // render_process_id
      0,                // render_view_id
      0,                // render_frame_id
      is_main_frame,    // is_main_frame
      false,            // parent_is_main_frame
      allow_download,   // allow_download
      true);            // is_async

  TestResourceDispatcherHost host(stream_has_handler_);
  TestResourceDispatcherHostDelegate host_delegate(must_download);
  host.SetDelegate(&host_delegate);

  FakePluginService plugin_service;
  scoped_ptr<ResourceHandler> buffered_handler(
      new BufferedResourceHandler(
          scoped_ptr<ResourceHandler>(new TestResourceHandler()).Pass(),
          &host,
          &plugin_service,
          request.get()));
  TestResourceController resource_controller;
  buffered_handler->SetController(&resource_controller);

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  // The MIME type isn't important but it shouldn't be empty.
  response->head.mime_type = "application/pdf";

  bool defer = false;
  buffered_handler->OnResponseStarted(response.get(), &defer);

  content::RunAllPendingInMessageLoop();

  return host.intercepted_as_stream();
}

// Test that stream requests are correctly intercepted under the right
// circumstances.
TEST_F(BufferedResourceHandlerTest, StreamHandling) {
  bool allow_download;
  bool must_download;
  ResourceType resource_type;

  // Ensure the stream is handled by MaybeInterceptAsStream in the
  // ResourceDispatcherHost.
  set_stream_has_handler(true);

  // Main frame request with no download allowed. Stream shouldn't be
  // intercepted.
  allow_download = false;
  must_download = false;
  resource_type = RESOURCE_TYPE_MAIN_FRAME;
  EXPECT_FALSE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));

  // Main frame request with download allowed. Stream should be intercepted.
  allow_download = true;
  must_download = false;
  resource_type = RESOURCE_TYPE_MAIN_FRAME;
  EXPECT_TRUE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));

  // Main frame request with download forced. Stream shouldn't be intercepted.
  allow_download = true;
  must_download = true;
  resource_type = RESOURCE_TYPE_MAIN_FRAME;
  EXPECT_FALSE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));

  // Sub-resource request with download not allowed. Stream shouldn't be
  // intercepted.
  allow_download = false;
  must_download = false;
  resource_type = RESOURCE_TYPE_SUB_RESOURCE;
  EXPECT_FALSE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));

  // Object request with download not allowed. Stream should be intercepted.
  allow_download = false;
  must_download = false;
  resource_type = RESOURCE_TYPE_OBJECT;
  EXPECT_TRUE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));

  // Test the cases where the stream isn't handled by MaybeInterceptAsStream
  // in the ResourceDispatcherHost.
  set_stream_has_handler(false);

  allow_download = false;
  must_download = false;
  resource_type = RESOURCE_TYPE_OBJECT;
  EXPECT_FALSE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));

  allow_download = true;
  must_download = false;
  resource_type = RESOURCE_TYPE_MAIN_FRAME;
  EXPECT_FALSE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));
}

}  // namespace

}  // namespace content
