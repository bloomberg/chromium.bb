// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Tests exercising the Chrome Plugin API.

#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_plugin_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/test/chrome_plugin/test_chrome_plugin.h"
#include "chrome/test/test_url_request_context_getter.h"
#include "content/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");
const char kPluginFilename[] = "test_chrome_plugin.dll";
const int kResponseBufferSize = 4096;

class ChromePluginTest : public testing::Test,
                         public net::URLRequest::Delegate {
 public:
  ChromePluginTest()
      : io_thread_(BrowserThread::IO, &message_loop_),
        request_(NULL),
        response_buffer_(new net::IOBuffer(kResponseBufferSize)),
        plugin_(NULL),
        expected_payload_(NULL),
        request_context_getter_(new TestURLRequestContextGetter()) {
    test_funcs_.test_make_request = NULL;
  }

  // Loads/unloads the chrome test plugin.
  void LoadPlugin();
  void UnloadPlugin();

  // Runs the test and expects the given payload as a response.  If expectation
  // is NULL, the request is expected to fail.
  void RunTest(const GURL& url, const TestResponsePayload* expected_payload);

  // net::URLRequest::Delegate implementations
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

  // Helper called when the net::URLRequest is done.
  void OnURLRequestComplete();

  // testing::Test
  virtual void SetUp() {
    LoadPlugin();
    net::URLRequest::RegisterProtocolFactory("test",
                                             &net::URLRequestTestJob::Factory);

    // We need to setup a default request context in order to issue HTTP
    // requests.
    DCHECK(!Profile::GetDefaultRequestContext());
    Profile::set_default_request_context(request_context_getter_.get());
  }
  virtual void TearDown() {
    UnloadPlugin();
    net::URLRequest::RegisterProtocolFactory("test", NULL);

    Profile::set_default_request_context(NULL);

    // Clear the request before flushing the message loop since killing the
    // request can result in the generation of more tasks.
    request_.reset();

    // Flush the message loop to make Purify happy.
    message_loop_.RunAllPending();
  }
 protected:
  MessageLoopForIO message_loop_;
  BrowserThread io_thread_;

  // Note: we use net::URLRequest (instead of URLFetcher) because this allows
  // the request to be intercepted.
  scoped_ptr<net::URLRequest> request_;
  scoped_refptr<net::IOBuffer> response_buffer_;
  std::string response_data_;

  ChromePluginLib* plugin_;
  TestFuncParams::PluginFuncs test_funcs_;
  const TestResponsePayload* expected_payload_;
  scoped_refptr<URLRequestContextGetter> request_context_getter_;
};

static void STDCALL CPT_Complete(CPRequest* request, bool success,
                                 const std::string& raw_headers,
                                 const std::string& body) {
  GURL url(request->url);
  if (url == GURL(kChromeTestPluginPayloads[0].url)) {
    // This URL should fail, because the plugin should not have intercepted it.
    EXPECT_FALSE(success);
    MessageLoop::current()->Quit();
    return;
  }

  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(raw_headers));
  EXPECT_TRUE(success);
  EXPECT_EQ(200, headers->response_code());

  if (url == net::URLRequestTestJob::test_url_1()) {
    EXPECT_EQ(net::URLRequestTestJob::test_data_1(), body);
  } else if (url == net::URLRequestTestJob::test_url_2()) {
    EXPECT_EQ(net::URLRequestTestJob::test_data_2(), body);
  } else if (url.spec().find("echo") != std::string::npos) {
    EXPECT_EQ(kChromeTestPluginPostData, body);
  }

  MessageLoop::current()->Quit();
}

static void STDCALL CPT_InvokeLater(TestFuncParams::CallbackFunc callback,
                                    void* callback_data, int delay_ms) {
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      NewRunnableFunction(callback, callback_data), delay_ms);
}

void ChromePluginTest::LoadPlugin() {
  FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  path = path.AppendASCII(kPluginFilename);
  plugin_ = ChromePluginLib::Create(path, GetCPBrowserFuncsForBrowser());
  ASSERT_TRUE(plugin_);

  // Exchange test APIs with the plugin.
  TestFuncParams params;
  params.bfuncs.test_complete = CPT_Complete;
  params.bfuncs.invoke_later = CPT_InvokeLater;
  EXPECT_EQ(CPERR_SUCCESS, plugin_->CP_Test(&params));
  test_funcs_ = params.pfuncs;
}

void ChromePluginTest::UnloadPlugin() {
  ChromePluginLib::UnloadAllPlugins();
  plugin_ = NULL;
}

void ChromePluginTest::RunTest(const GURL& url,
                               const TestResponsePayload* expected_payload) {
  expected_payload_ = expected_payload;

  response_data_.clear();
  request_.reset(new net::URLRequest(url, this));
  request_->set_context(new TestURLRequestContext());
  request_->Start();

  MessageLoop::current()->Run();
}

void ChromePluginTest::OnResponseStarted(net::URLRequest* request) {
  DCHECK(request == request_);

  int bytes_read = 0;
  if (request_->status().is_success())
    request_->Read(response_buffer_, kResponseBufferSize, &bytes_read);
  OnReadCompleted(request_.get(), bytes_read);
}

void ChromePluginTest::OnReadCompleted(net::URLRequest* request,
                                       int bytes_read) {
  DCHECK(request == request_);

  do {
    if (!request_->status().is_success() || bytes_read <= 0)
      break;
    response_data_.append(response_buffer_->data(), bytes_read);
  } while (request_->Read(response_buffer_, kResponseBufferSize, &bytes_read));

  if (!request_->status().is_io_pending()) {
    OnURLRequestComplete();
  }
}

void ChromePluginTest::OnURLRequestComplete() {
  if (expected_payload_) {
    EXPECT_TRUE(request_->status().is_success());

    EXPECT_EQ(expected_payload_->status, request_->GetResponseCode());
    if (expected_payload_->mime_type) {
      std::string mime_type;
      EXPECT_TRUE(request_->response_headers()->GetMimeType(&mime_type));
      EXPECT_EQ(expected_payload_->mime_type, mime_type);
    }
    if (expected_payload_->body) {
      EXPECT_EQ(expected_payload_->body, response_data_);
    }
  } else {
    EXPECT_FALSE(request_->status().is_success());
  }

  MessageLoop::current()->Quit();
  // If MessageLoop::current() != main_loop_, it will be shut down when the
  // main loop returns and this thread subsequently goes out of scope.
}

// Tests that the plugin can intercept URLs.
TEST_F(ChromePluginTest, DoesIntercept) {
  for (size_t i = 0; i < arraysize(kChromeTestPluginPayloads); ++i) {
    RunTest(GURL(kChromeTestPluginPayloads[i].url),
            &kChromeTestPluginPayloads[i]);
  }
}

// Tests that non-intercepted URLs are handled normally.
TEST_F(ChromePluginTest, DoesNotIntercept) {
  TestResponsePayload about_blank = {
    "about:blank",
    false,
    -1,
    NULL,
    ""
  };
  RunTest(GURL(about_blank.url), &about_blank);
}

// Tests that unloading the plugin correctly unregisters URL interception.
TEST_F(ChromePluginTest, UnregisterIntercept) {
  UnloadPlugin();

  RunTest(GURL(kChromeTestPluginPayloads[0].url), NULL);
}

static void ProcessAllPendingMessages() {
  while (net::URLRequestTestJob::ProcessOnePendingMessage());
}

// Tests that the plugin can issue a GET request and receives the data when
// it comes back synchronously.
TEST_F(ChromePluginTest, CanMakeGETRequestSync) {
  // test_url_1 has a synchronous response
  EXPECT_EQ(CPERR_SUCCESS, test_funcs_.test_make_request(
      "GET", net::URLRequestTestJob::test_url_1()));

  // Note: we must add this task after we make the request, so that
  // net::URLRequestTestJob's StartAsync task is added and run first.
  MessageLoop::current()->PostTask(FROM_HERE,
      NewRunnableFunction(&ProcessAllPendingMessages));
  MessageLoop::current()->Run();
}

// Tests that the plugin can issue a GET request and receives the data when
// it comes back asynchronously.
TEST_F(ChromePluginTest, CanMakeGETRequestAsync) {
  // test_url_2 has an asynchronous response
  EXPECT_EQ(CPERR_SUCCESS, test_funcs_.test_make_request(
        "GET", net::URLRequestTestJob::test_url_2()));

  // Note: we must add this task after we make the request, so that
  // net::URLRequestTestJob's StartAsync task is added and run first.
  MessageLoop::current()->PostTask(FROM_HERE,
      NewRunnableFunction(&ProcessAllPendingMessages));
  MessageLoop::current()->Run();
}

// Tests that the plugin can issue a POST request.
TEST_F(ChromePluginTest, CanMakePOSTRequest) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  GURL url = test_server.GetURL("echo");

  EXPECT_EQ(CPERR_SUCCESS, test_funcs_.test_make_request("POST", url));

  // Note: we must add this task after we make the request, so that
  // net::URLRequestTestJob's StartAsync task is added and run first.
  MessageLoop::current()->PostTask(FROM_HERE,
      NewRunnableFunction(&ProcessAllPendingMessages));
  MessageLoop::current()->Run();
}

// Tests that the plugin does not intercept its own requests.
TEST_F(ChromePluginTest, DoesNotInterceptOwnRequest) {
  const TestResponsePayload& payload = kChromeTestPluginPayloads[0];

  EXPECT_EQ(CPERR_SUCCESS, test_funcs_.test_make_request(
      "GET", GURL(payload.url)));

  MessageLoop::current()->Run();
}

}  // namespace
