// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/mime_sniffing_resource_handler.h"

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/loader/intercepting_resource_handler.h"
#include "content/browser/loader/mock_resource_loader.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/test_resource_handler.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/fake_plugin_service.h"
#include "net/url_request/url_request_context.h"
#include "ppapi/features/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

class TestResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  explicit TestResourceDispatcherHostDelegate(bool must_download)
      : must_download_(must_download) {}

  bool ShouldForceDownloadResource(const GURL& url,
                                   const std::string& mime_type) override {
    return must_download_;
  }

 private:
  const bool must_download_;
};

class TestResourceDispatcherHost : public ResourceDispatcherHostImpl {
 public:
  explicit TestResourceDispatcherHost(bool stream_has_handler)
      : stream_has_handler_(stream_has_handler),
        intercepted_as_stream_(false),
        intercepted_as_stream_count_(0),
        new_resource_handler_(nullptr) {}

  bool intercepted_as_stream() const { return intercepted_as_stream_; }

  std::unique_ptr<ResourceHandler> CreateResourceHandlerForDownload(
      net::URLRequest* request,
      bool is_content_initiated,
      bool must_download,
      bool is_new_request) override {
    return CreateNewResourceHandler();
  }

  std::unique_ptr<ResourceHandler> MaybeInterceptAsStream(
      const base::FilePath& plugin_path,
      net::URLRequest* request,
      ResourceResponse* response,
      std::string* payload) override {
    intercepted_as_stream_count_++;
    if (stream_has_handler_)
      intercepted_as_stream_ = true;
    return CreateNewResourceHandler();
  }

  int intercepted_as_stream_count() const {
    return intercepted_as_stream_count_;
  }

  TestResourceHandler* new_resource_handler() const {
    return new_resource_handler_;
  }

 private:
  std::unique_ptr<ResourceHandler> CreateNewResourceHandler() {
    std::unique_ptr<TestResourceHandler> new_resource_handler(
        new TestResourceHandler());
    new_resource_handler->set_on_response_started_result(false);
    new_resource_handler_ = new_resource_handler.get();
    return std::move(new_resource_handler);
  }

  // Whether the URL request should be intercepted as a stream.
  bool stream_has_handler_;

  // Whether the URL request has been intercepted as a stream.
  bool intercepted_as_stream_;

  // Count of number of times MaybeInterceptAsStream function get called in a
  // test.
  int intercepted_as_stream_count_;

  // The last alternative TestResourceHandler created by this
  // TestResourceDispatcherHost.
  TestResourceHandler* new_resource_handler_;
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
                     const url::Origin& main_frame_origin,
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, plugins));
  }

 private:
  const bool plugin_available_;
  bool is_plugin_stale_;

  DISALLOW_COPY_AND_ASSIGN(TestFakePluginService);
};

}  // namespace

class MimeSniffingResourceHandlerTest : public testing::Test {
 public:
  MimeSniffingResourceHandlerTest()
      : stream_has_handler_(false),
        plugin_available_(false),
        plugin_stale_(false) {}

  // Tests that the MimeSniffingHandler properly sets the accept field in the
  // header. Returns the accept header value.
  std::string TestAcceptHeaderSetting(ResourceType request_resource_type);
  std::string TestAcceptHeaderSettingWithURLRequest(
      ResourceType request_resource_type,
      net::URLRequest* request);

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

  // Tests the operation of the MimeSniffingHandler when it needs to buffer
  // data (example case: the response is text/plain).
  void TestHandlerSniffing(bool response_started,
                           bool defer_response_started,
                           bool will_read,
                           bool read_completed,
                           bool defer_read_completed);

  // Tests the operation of the MimeSniffingHandler when it doesn't buffer
  // data (example case: the response is text/html).
  void TestHandlerNoSniffing(bool response_started,
                             bool defer_response_started,
                             bool will_read,
                             bool read_completed,
                             bool defer_read_completed);

 private:
  // Whether the URL request should be intercepted as a stream.
  bool stream_has_handler_;
  bool plugin_available_;
  bool plugin_stale_;

  TestBrowserThreadBundle thread_bundle_;
};

std::string MimeSniffingResourceHandlerTest::TestAcceptHeaderSetting(
    ResourceType request_resource_type) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  return TestAcceptHeaderSettingWithURLRequest(request_resource_type,
                                               request.get());
}

std::string
MimeSniffingResourceHandlerTest::TestAcceptHeaderSettingWithURLRequest(
    ResourceType request_resource_type,
    net::URLRequest* request) {
  bool is_main_frame = request_resource_type == RESOURCE_TYPE_MAIN_FRAME;
  ResourceRequestInfo::AllocateForTesting(request, request_resource_type,
                                          nullptr,        // context
                                          0,              // render_process_id
                                          0,              // render_view_id
                                          0,              // render_frame_id
                                          is_main_frame,  // is_main_frame
                                          false,  // parent_is_main_frame
                                          false,  // allow_download
                                          true,   // is_async
                                          PREVIEWS_OFF);  // previews_state

  std::unique_ptr<TestResourceHandler> scoped_test_handler(
      new TestResourceHandler());
  scoped_test_handler->set_on_response_started_result(false);

  MimeSniffingResourceHandler mime_sniffing_handler(
      std::move(scoped_test_handler), nullptr, nullptr, nullptr, request,
      REQUEST_CONTEXT_TYPE_UNSPECIFIED);
  MockResourceLoader mock_loader(&mime_sniffing_handler);

  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader.OnWillStart(request->url()));

  std::string accept_header;
  request->extra_request_headers().GetHeader("Accept", &accept_header);
  return accept_header;
}

bool MimeSniffingResourceHandlerTest::TestStreamIsIntercepted(
    bool allow_download,
    bool must_download,
    ResourceType request_resource_type) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  bool is_main_frame = request_resource_type == RESOURCE_TYPE_MAIN_FRAME;
  ResourceRequestInfo::AllocateForTesting(request.get(), request_resource_type,
                                          nullptr,        // context
                                          0,              // render_process_id
                                          0,              // render_view_id
                                          0,              // render_frame_id
                                          is_main_frame,  // is_main_frame
                                          false,  // parent_is_main_frame
                                          allow_download,  // allow_download
                                          true,            // is_async
                                          PREVIEWS_OFF);   // previews_state

  TestResourceDispatcherHost host(stream_has_handler_);
  TestResourceDispatcherHostDelegate host_delegate(must_download);
  host.SetDelegate(&host_delegate);

  TestFakePluginService plugin_service(plugin_available_, plugin_stale_);

  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(base::MakeUnique<TestResourceHandler>(),
                                      nullptr));
  std::unique_ptr<TestResourceHandler> scoped_test_handler(
      new TestResourceHandler());
  scoped_test_handler->set_on_response_started_result(false);
  MimeSniffingResourceHandler mime_sniffing_handler(
      std::unique_ptr<ResourceHandler>(std::move(scoped_test_handler)), &host,
      &plugin_service, intercepting_handler.get(), request.get(),
      REQUEST_CONTEXT_TYPE_UNSPECIFIED);

  MockResourceLoader mock_loader(&mime_sniffing_handler);

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  // The MIME type isn't important but it shouldn't be empty.
  response->head.mime_type = "application/pdf";

  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader.OnWillStart(request->url()));

  mock_loader.OnResponseStarted(std::move(response));
  mock_loader.WaitUntilIdleOrCanceled();
  EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader.status());
  EXPECT_EQ(net::ERR_ABORTED, mock_loader.error_code());

  content::RunAllPendingInMessageLoop();
  EXPECT_LT(host.intercepted_as_stream_count(), 2);
  if (allow_download)
    EXPECT_TRUE(intercepting_handler->new_handler_for_testing());
  return host.intercepted_as_stream();
}

void MimeSniffingResourceHandlerTest::TestHandlerSniffing(
    bool response_started,
    bool defer_response_started,
    bool will_read,
    bool read_completed,
    bool defer_read_completed) {
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
                                          false,    // allow_download
                                          true,     // is_async
                                          PREVIEWS_OFF);  // previews_state

  TestResourceDispatcherHost host(false);
  TestResourceDispatcherHostDelegate host_delegate(false);
  host.SetDelegate(&host_delegate);

  TestFakePluginService plugin_service(plugin_available_, plugin_stale_);
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(base::MakeUnique<TestResourceHandler>(),
                                      nullptr));

  std::unique_ptr<TestResourceHandler> scoped_test_handler(
      new TestResourceHandler());
  scoped_test_handler->set_on_response_started_result(response_started);
  scoped_test_handler->set_defer_on_response_started(defer_response_started);
  scoped_test_handler->set_on_will_read_result(will_read);
  scoped_test_handler->set_on_read_completed_result(read_completed);
  scoped_test_handler->set_defer_on_read_completed(defer_read_completed);
  TestResourceHandler* test_handler = scoped_test_handler.get();
  MimeSniffingResourceHandler mime_sniffing_handler(
      std::move(scoped_test_handler), &host, &plugin_service,
      intercepting_handler.get(), request.get(),
      REQUEST_CONTEXT_TYPE_UNSPECIFIED);

  MockResourceLoader mock_loader(&mime_sniffing_handler);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader.OnWillStart(request->url()));

  // The response should be sniffed.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  response->head.mime_type.assign("text/plain");

  // Simulate the response starting. The MimeSniffingHandler should start
  // buffering, so the return value should always be true.
  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader.OnResponseStarted(std::move(response)));

  // Read some data to sniff the mime type. This will ask the next
  // ResourceHandler for a buffer.
  mock_loader.OnWillRead();

  if (!will_read) {
    EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader.status());
    EXPECT_EQ(net::ERR_ABORTED, mock_loader.error_code());

    EXPECT_EQ(1, test_handler->on_will_start_called());
    EXPECT_EQ(0, test_handler->on_request_redirected_called());
    EXPECT_EQ(0, test_handler->on_response_started_called());
    EXPECT_EQ(1, test_handler->on_will_read_called());
    EXPECT_EQ(0, test_handler->on_read_completed_called());

    // Process all messages to ensure proper test teardown.
    content::RunAllPendingInMessageLoop();
    return;
  }

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader.status());

  // Simulate an HTML page. The mime sniffer will identify the MimeType and
  // proceed with replay.
  const char kData[] = "!DOCTYPE html\n<head>\n<title>Foo</title>\n</head>";
  // Construct StringPiece manually, as the terminal null needs to be included,
  // so it's sniffed as binary (Not important that it's sniffed as binary, but
  // this gaurantees it's sniffed as something, without waiting for more data).
  mock_loader.OnReadCompleted(base::StringPiece(kData, sizeof(kData)));

  // If the next handler cancels the response start, the caller of
  // MimeSniffingHandler::OnReadCompleted should be notified immediately.
  if (!response_started) {
    EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader.status());
    EXPECT_EQ(net::ERR_ABORTED, mock_loader.error_code());

    EXPECT_EQ(1, test_handler->on_will_start_called());
    EXPECT_EQ(0, test_handler->on_request_redirected_called());
    EXPECT_EQ(1, test_handler->on_response_started_called());
    EXPECT_EQ(1, test_handler->on_will_read_called());
    EXPECT_EQ(0, test_handler->on_read_completed_called());

    // Process all messages to ensure proper test teardown.
    content::RunAllPendingInMessageLoop();
    return;
  }

  if (defer_response_started) {
    ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
              mock_loader.status());
    EXPECT_EQ(MimeSniffingResourceHandler::STATE_REPLAYING_RESPONSE_RECEIVED,
              mime_sniffing_handler.state_);
    test_handler->Resume();
    // MimeSniffingResourceHandler may not synchronously resume the request.
    base::RunLoop().RunUntilIdle();
  }

  // The body that was sniffed should be transmitted to the next handler. This
  // may cancel the request.
  if (!read_completed) {
    EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader.status());
    EXPECT_EQ(net::ERR_ABORTED, mock_loader.error_code());

    // Process all messages to ensure proper test teardown.
    content::RunAllPendingInMessageLoop();
    return;
  }

  EXPECT_EQ(MimeSniffingResourceHandler::STATE_STREAMING,
            mime_sniffing_handler.state_);

  // The request may be deferred by the next handler once the read is done.
  if (defer_read_completed) {
    ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
              mock_loader.status());
    test_handler->Resume();
    // MimeSniffingResourceHandler may not synchronously resume the request.
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(MockResourceLoader::Status::IDLE, mock_loader.status());
  EXPECT_EQ(MimeSniffingResourceHandler::STATE_STREAMING,
            mime_sniffing_handler.state_);

  EXPECT_EQ(1, test_handler->on_will_start_called());
  EXPECT_EQ(0, test_handler->on_request_redirected_called());
  EXPECT_EQ(1, test_handler->on_response_started_called());
  EXPECT_EQ(1, test_handler->on_will_read_called());
  EXPECT_EQ(1, test_handler->on_read_completed_called());

  // Process all messages to ensure proper test teardown.
  content::RunAllPendingInMessageLoop();
}

void MimeSniffingResourceHandlerTest::TestHandlerNoSniffing(
    bool response_started,
    bool defer_response_started,
    bool will_read,
    bool read_completed,
    bool defer_read_completed) {
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
                                          false,    // allow_download
                                          true,     // is_async
                                          PREVIEWS_OFF);  // previews_state

  TestResourceDispatcherHost host(false);
  TestResourceDispatcherHostDelegate host_delegate(false);
  host.SetDelegate(&host_delegate);

  TestFakePluginService plugin_service(plugin_available_, plugin_stale_);
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(base::MakeUnique<TestResourceHandler>(),
                                      nullptr));

  std::unique_ptr<TestResourceHandler> scoped_test_handler(
      new TestResourceHandler());
  scoped_test_handler->set_on_response_started_result(response_started);
  scoped_test_handler->set_defer_on_response_started(defer_response_started);
  scoped_test_handler->set_on_will_read_result(will_read);
  scoped_test_handler->set_on_read_completed_result(read_completed);
  scoped_test_handler->set_defer_on_read_completed(defer_read_completed);
  TestResourceHandler* test_handler = scoped_test_handler.get();
  MimeSniffingResourceHandler mime_sniffing_handler(
      std::move(scoped_test_handler), &host, &plugin_service,
      intercepting_handler.get(), request.get(),
      REQUEST_CONTEXT_TYPE_UNSPECIFIED);

  MockResourceLoader mock_loader(&mime_sniffing_handler);

  ASSERT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader.OnWillStart(request->url()));

  // The response should not be sniffed.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  response->head.mime_type.assign("text/html");

  // Simulate the response starting. There should be no need for buffering, so
  // the return value should be that of the next handler.
  mock_loader.OnResponseStarted(std::move(response));

  if (!response_started) {
    EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader.status());
    EXPECT_EQ(net::ERR_ABORTED, mock_loader.error_code());

    EXPECT_EQ(1, test_handler->on_will_start_called());
    EXPECT_EQ(0, test_handler->on_request_redirected_called());
    EXPECT_EQ(1, test_handler->on_response_started_called());
    EXPECT_EQ(0, test_handler->on_will_read_called());
    EXPECT_EQ(0, test_handler->on_read_completed_called());

    // Process all messages to ensure proper test teardown.
    content::RunAllPendingInMessageLoop();
    return;
  }

  if (defer_response_started) {
    ASSERT_EQ(MockResourceLoader::Status::CALLBACK_PENDING,
              mock_loader.status());
    EXPECT_EQ(MimeSniffingResourceHandler::STATE_REPLAYING_RESPONSE_RECEIVED,
              mime_sniffing_handler.state_);
    test_handler->Resume();
    // MimeSniffingResourceHandler may not synchronously resume the request.
    base::RunLoop().RunUntilIdle();
  }

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader.status());

  // The MimeSniffingResourceHandler should be acting as a pass-through
  // ResourceHandler.
  mock_loader.OnWillRead();

  if (!will_read) {
    EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader.status());
    EXPECT_EQ(net::ERR_ABORTED, mock_loader.error_code());

    EXPECT_EQ(1, test_handler->on_will_start_called());
    EXPECT_EQ(0, test_handler->on_request_redirected_called());
    EXPECT_EQ(1, test_handler->on_response_started_called());
    EXPECT_EQ(1, test_handler->on_will_read_called());
    EXPECT_EQ(0, test_handler->on_read_completed_called());

    // Process all messages to ensure proper test teardown.
    content::RunAllPendingInMessageLoop();
    return;
  }

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader.status());

  mock_loader.OnReadCompleted(std::string(2000, 'a'));

  EXPECT_EQ(1, test_handler->on_will_start_called());
  EXPECT_EQ(0, test_handler->on_request_redirected_called());
  EXPECT_EQ(1, test_handler->on_response_started_called());
  EXPECT_EQ(1, test_handler->on_will_read_called());
  EXPECT_EQ(1, test_handler->on_read_completed_called());

  if (!read_completed) {
    EXPECT_EQ(MockResourceLoader::Status::CANCELED, mock_loader.status());
    EXPECT_EQ(net::ERR_ABORTED, mock_loader.error_code());

    // Process all messages to ensure proper test teardown.
    content::RunAllPendingInMessageLoop();
    return;
  }

  if (mock_loader.status() == MockResourceLoader::Status::CALLBACK_PENDING) {
    test_handler->Resume();
    // MimeSniffingResourceHandler may not synchronously resume the request.
    base::RunLoop().RunUntilIdle();
  }

  ASSERT_EQ(MockResourceLoader::Status::IDLE, mock_loader.status());

  // Process all messages to ensure proper test teardown.
  content::RunAllPendingInMessageLoop();
}

// Test that the proper Accept: header is set based on the ResourceType
TEST_F(MimeSniffingResourceHandlerTest, AcceptHeaders) {
  EXPECT_EQ(
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
      "*/*;q=0.8",
      TestAcceptHeaderSetting(RESOURCE_TYPE_MAIN_FRAME));
  EXPECT_EQ(
      "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
      "*/*;q=0.8",
      TestAcceptHeaderSetting(RESOURCE_TYPE_SUB_FRAME));
  EXPECT_EQ("text/css,*/*;q=0.1",
            TestAcceptHeaderSetting(RESOURCE_TYPE_STYLESHEET));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_SCRIPT));
  EXPECT_EQ("image/webp,image/*,*/*;q=0.8",
            TestAcceptHeaderSetting(RESOURCE_TYPE_IMAGE));
  EXPECT_EQ("image/webp,image/*,*/*;q=0.8",
            TestAcceptHeaderSetting(RESOURCE_TYPE_FAVICON));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_FONT_RESOURCE));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_SUB_RESOURCE));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_OBJECT));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_MEDIA));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_WORKER));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_SHARED_WORKER));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_PREFETCH));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_XHR));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_PING));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_SERVICE_WORKER));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_CSP_REPORT));
  EXPECT_EQ("*/*", TestAcceptHeaderSetting(RESOURCE_TYPE_PLUGIN_RESOURCE));

  // Ensure that if an Accept header is already set, it is not overwritten.
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> request(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr));
  request->SetExtraRequestHeaderByName("Accept", "*", true);
  EXPECT_EQ("*", TestAcceptHeaderSettingWithURLRequest(RESOURCE_TYPE_XHR,
                                                       request.get()));
}

// Test that stream requests are correctly intercepted under the right
// circumstances. Test is not relevent when plugins are disabled.
#if BUILDFLAG(ENABLE_PLUGINS)
TEST_F(MimeSniffingResourceHandlerTest, StreamHandling) {
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

// Test that the MimeSniffingHandler operates properly when it doesn't sniff
// resources.
TEST_F(MimeSniffingResourceHandlerTest, NoSniffing) {
  // Test simple case.
  TestHandlerNoSniffing(
      true  /* response_started_succeeds */,
      false /* defer_response_started */,
      true  /* will_read_succeeds */,
      true  /* read_completed_succeeds */,
      false /* defer_read_completed */);

  // Test deferral in OnResponseStarted and/or in OnReadCompleted.
  TestHandlerNoSniffing(
      true  /* response_started_succeeds */,
      true  /* defer_response_started */,
      true  /* will_read_succeeds */,
      true  /* read_completed_succeeds */,
      false /* defer_read_completed */);
  TestHandlerNoSniffing(
      true  /* response_started_succeeds */,
      false /* defer_response_started */,
      true  /* will_read_succeeds */,
      true  /* read_completed_succeeds */,
      true  /* defer_read_completed */);
  TestHandlerNoSniffing(
      true  /* response_started_succeeds */,
      true  /* defer_response_started */,
      true  /* will_read_succeeds */,
      true  /* read_completed_succeeds */,
      true  /* defer_read_completed */);

  // Test cancel in OnResponseStarted, OnWillRead, OnReadCompleted.
  TestHandlerNoSniffing(
      false /* response_started_succeeds */,
      false /* defer_response_started */,
      false /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);
  TestHandlerNoSniffing(
      true  /* response_started_succeeds */,
      false /* defer_response_started */,
      false /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);
  TestHandlerNoSniffing(
      true  /* response_started_succeeds */,
      false /* defer_response_started */,
      true  /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);

  // Test cancel after OnResponseStarted deferral.
  TestHandlerNoSniffing(
      true  /* response_started_succeeds */,
      true  /* defer_response_started */,
      false /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);
  TestHandlerNoSniffing(
      true  /* response_started_succeeds */,
      true  /* defer_response_started */,
      true  /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);

  content::RunAllPendingInMessageLoop();
}

// Test that the MimeSniffingHandler operates properly when it sniffs
// resources.
TEST_F(MimeSniffingResourceHandlerTest, Sniffing) {
  // Test simple case.
  TestHandlerSniffing(
      true  /* response_started_succeeds */,
      false /* defer_response_started */,
      true  /* will_read_succeeds */,
      true  /* read_completed_succeeds */,
      false /* defer_read_completed */);

  // Test deferral in OnResponseStarted and/or in OnReadCompleted.
  TestHandlerSniffing(
      true  /* response_started_succeeds */,
      true  /* defer_response_started */,
      true  /* will_read_succeeds */,
      true  /* read_completed_succeeds */,
      false /* defer_read_completed */);
  TestHandlerSniffing(
      true  /* response_started_succeeds */,
      false /* defer_response_started */,
      true  /* will_read_succeeds */,
      true  /* read_completed_succeeds */,
      true  /* defer_read_completed */);
  TestHandlerSniffing(
      true  /* response_started_succeeds */,
      true  /* defer_response_started */,
      true  /* will_read_succeeds */,
      true  /* read_completed_succeeds */,
      true  /* defer_read_completed */);

  // Test cancel in OnResponseStarted, OnWillRead, OnReadCompleted.
  TestHandlerSniffing(
      false /* response_started_succeeds */,
      false /* defer_response_started */,
      false /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);
  TestHandlerSniffing(
      true  /* response_started_succeeds */,
      false /* defer_response_started */,
      false /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);
  TestHandlerSniffing(
      true  /* response_started_succeeds */,
      false /* defer_response_started */,
      true  /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);

  // Test cancel after OnResponseStarted deferral.
  TestHandlerSniffing(
      true  /* response_started_succeeds */,
      true  /* defer_response_started */,
      false /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);
  TestHandlerSniffing(
      true  /* response_started_succeeds */,
      true  /* defer_response_started */,
      true  /* will_read_succeeds */,
      false /* read_completed_succeeds */,
      false /* defer_read_completed */);

  content::RunAllPendingInMessageLoop();
}

// Tests that 304s do not trigger a change in handlers.
TEST_F(MimeSniffingResourceHandlerTest, 304Handling) {
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
                                          PREVIEWS_OFF);  // previews_state

  TestResourceDispatcherHost host(false);
  TestResourceDispatcherHostDelegate host_delegate(false);
  host.SetDelegate(&host_delegate);

  TestFakePluginService plugin_service(false, false);
  std::unique_ptr<ResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(base::MakeUnique<TestResourceHandler>(),
                                      nullptr));
  MimeSniffingResourceHandler mime_sniffing_handler(
      std::unique_ptr<ResourceHandler>(new TestResourceHandler()), &host,
      &plugin_service,
      static_cast<InterceptingResourceHandler*>(intercepting_handler.get()),
      request.get(), REQUEST_CONTEXT_TYPE_UNSPECIFIED);

  MockResourceLoader mock_loader(&mime_sniffing_handler);

  // Request starts.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader.OnWillStart(request->url()));

  // Simulate a 304 response.
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  // The MIME type isn't important but it shouldn't be empty.
  response->head.mime_type = "application/pdf";
  response->head.headers = new net::HttpResponseHeaders("HTTP/1.x 304 OK");

  // The response is received. No new ResourceHandler should be created to
  // handle the download.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader.OnResponseStarted(std::move(response)));
  EXPECT_FALSE(host.new_resource_handler());

  content::RunAllPendingInMessageLoop();
}

TEST_F(MimeSniffingResourceHandlerTest, FetchShouldDisableMimeSniffing) {
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
                                          false,    // allow_download
                                          true,     // is_async
                                          PREVIEWS_OFF);  // previews_state

  TestResourceDispatcherHost host(false);

  TestFakePluginService plugin_service(false, false);
  std::unique_ptr<InterceptingResourceHandler> intercepting_handler(
      new InterceptingResourceHandler(base::MakeUnique<TestResourceHandler>(),
                                      nullptr));

  std::unique_ptr<TestResourceHandler> scoped_test_handler(
      new TestResourceHandler());
  scoped_test_handler->set_on_response_started_result(false);
  MimeSniffingResourceHandler mime_sniffing_handler(
      std::move(scoped_test_handler), &host, &plugin_service,
      intercepting_handler.get(), request.get(), REQUEST_CONTEXT_TYPE_FETCH);

  MockResourceLoader mock_loader(&mime_sniffing_handler);

  // Request starts.
  EXPECT_EQ(MockResourceLoader::Status::IDLE,
            mock_loader.OnWillStart(request->url()));

  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  response->head.mime_type = "text/plain";

  // |mime_sniffing_handler->OnResponseStarted| should return false because
  // mime sniffing is disabled and the wrapped resource handler returns false
  // on OnResponseStarted.
  EXPECT_EQ(MockResourceLoader::Status::CANCELED,
            mock_loader.OnResponseStarted(std::move(response)));
  EXPECT_EQ(net::ERR_ABORTED, mock_loader.error_code());

  // Process all messages to ensure proper test teardown.
  content::RunAllPendingInMessageLoop();
}

}  // namespace content
