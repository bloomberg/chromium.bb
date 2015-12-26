// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/mime_type_resource_handler.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/webplugininfo.h"
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
        intercepted_as_stream_(false),
        intercepted_as_stream_count_(0) {}

  bool intercepted_as_stream() const { return intercepted_as_stream_; }

  scoped_ptr<ResourceHandler> CreateResourceHandlerForDownload(
      net::URLRequest* request,
      bool is_content_initiated,
      bool must_download,
      uint32_t id,
      scoped_ptr<DownloadSaveInfo> save_info,
      const DownloadUrlParameters::OnStartedCallback& started_cb) override {
    return scoped_ptr<ResourceHandler>(new TestResourceHandler);
  }

  scoped_ptr<ResourceHandler> MaybeInterceptAsStream(
      const base::FilePath& plugin_path,
      net::URLRequest* request,
      ResourceResponse* response,
      std::string* payload) override {
    intercepted_as_stream_count_++;
    if (stream_has_handler_) {
      intercepted_as_stream_ = true;
      return scoped_ptr<ResourceHandler>(new TestResourceHandler);
    } else {
      return scoped_ptr<ResourceHandler>();
    }
  }

  int intercepted_as_stream_count() const {
    return intercepted_as_stream_count_;
  }

 private:
  // Whether the URL request should be intercepted as a stream.
  bool stream_has_handler_;

  // Whether the URL request has been intercepted as a stream.
  bool intercepted_as_stream_;

  // Count of number of times MaybeInterceptAsStream function get called in a
  // test.
  int intercepted_as_stream_count_;
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

class TestFakePluginService : public FakePluginService {
 public:
  // If |is_plugin_stale| is true, GetPluginInfo will indicate the plugins are
  // stale until GetPlugins is called.
  TestFakePluginService(bool plugin_available, bool is_plugin_stale)
      : plugin_available_(plugin_available),
        is_plugin_stale_(is_plugin_stale) {}

  bool GetPluginInfo(int render_process_id,
                     int render_frame_id,
                     ResourceContext* context,
                     const GURL& url,
                     const GURL& page_url,
                     const std::string& mime_type,
                     bool allow_wildcard,
                     bool* is_stale,
                     WebPluginInfo* info,
                     std::string* actual_mime_type) override {
    *is_stale = is_plugin_stale_;
    if (!is_plugin_stale_ || !plugin_available_)
      return false;
    info->type = WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN;
    info->path = base::FilePath::FromUTF8Unsafe(
        std::string("chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/"));
    return true;
  }

  void GetPlugins(const GetPluginsCallback& callback) override {
    is_plugin_stale_ = false;
    std::vector<WebPluginInfo> plugins;
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, plugins));
  }

 private:
  const bool plugin_available_;
  bool is_plugin_stale_;

  DISALLOW_COPY_AND_ASSIGN(TestFakePluginService);
};

class MimeTypeResourceHandlerTest : public testing::Test {
 public:
  MimeTypeResourceHandlerTest()
      : stream_has_handler_(false),
        plugin_available_(false),
        plugin_stale_(false) {}

  void set_stream_has_handler(bool stream_has_handler) {
    stream_has_handler_ = stream_has_handler;
  }

  void set_plugin_available(bool plugin_available) {
    plugin_available_ = plugin_available;
  }

  void set_plugin_stale(bool plugin_stale) { plugin_stale_ = plugin_stale; }

  bool TestStreamIsIntercepted(bool allow_download,
                               bool must_download,
                               ResourceType request_resource_type);

 private:
  // Whether the URL request should be intercepted as a stream.
  bool stream_has_handler_;
  bool plugin_available_;
  bool plugin_stale_;

  TestBrowserThreadBundle thread_bundle_;
};

bool MimeTypeResourceHandlerTest::TestStreamIsIntercepted(
    bool allow_download,
    bool must_download,
    ResourceType request_resource_type) {
  net::URLRequestContext context;
  scoped_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
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
      true,             // is_async
      false);           // is_using_lofi

  TestResourceDispatcherHost host(stream_has_handler_);
  TestResourceDispatcherHostDelegate host_delegate(must_download);
  host.SetDelegate(&host_delegate);

  TestFakePluginService plugin_service(plugin_available_, plugin_stale_);
  scoped_ptr<ResourceHandler> mime_sniffing_handler(new MimeTypeResourceHandler(
      scoped_ptr<ResourceHandler>(new TestResourceHandler()), &host,
      &plugin_service, request.get()));
  TestResourceController resource_controller;
  mime_sniffing_handler->SetController(&resource_controller);

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  // The MIME type isn't important but it shouldn't be empty.
  response->head.mime_type = "application/pdf";

  bool defer = false;
  mime_sniffing_handler->OnResponseStarted(response.get(), &defer);

  content::RunAllPendingInMessageLoop();
  EXPECT_LT(host.intercepted_as_stream_count(), 2);
  return host.intercepted_as_stream();
}

// Test that stream requests are correctly intercepted under the right
// circumstances. Test is not relevent when plugins are disabled.
#if defined(ENABLE_PLUGINS)
TEST_F(MimeTypeResourceHandlerTest, StreamHandling) {
  bool allow_download;
  bool must_download;
  ResourceType resource_type;

  // Ensure the stream is handled by MaybeInterceptAsStream in the
  // ResourceDispatcherHost.
  set_stream_has_handler(true);
  set_plugin_available(true);

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

  // Plugin resource request with download not allowed. Stream shouldn't be
  // intercepted.
  allow_download = false;
  must_download = false;
  resource_type = RESOURCE_TYPE_PLUGIN_RESOURCE;
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

  // Test the cases where the stream handled by MaybeInterceptAsStream
  // with plugin not available. This is the case when intercepting streams for
  // the streamsPrivate extensions API.
  set_stream_has_handler(true);
  set_plugin_available(false);
  allow_download = false;
  must_download = false;
  resource_type = RESOURCE_TYPE_OBJECT;
  EXPECT_TRUE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));

  // Test the cases where the stream handled by MaybeInterceptAsStream
  // with plugin not available. This is the case when intercepting streams for
  // the streamsPrivate extensions API with stale plugin.
  set_plugin_stale(true);
  allow_download = false;
  must_download = false;
  resource_type = RESOURCE_TYPE_OBJECT;
  EXPECT_TRUE(
      TestStreamIsIntercepted(allow_download, must_download, resource_type));
}
#endif

}  // namespace

}  // namespace content
