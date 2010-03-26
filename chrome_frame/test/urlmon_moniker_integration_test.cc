// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "base/scoped_comptr_win.h"
#include "base/thread.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/test/test_server.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/urlmon_moniker_tests.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::_;
using testing::CreateFunctor;
using testing::Eq;
using testing::Invoke;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::Return;
using testing::WithArg;
using testing::WithArgs;

static int kUrlmonMonikerTimeoutSec = 5;

namespace {
const char kTestContent[] = "<html><head>"
    "<meta http-equiv=\"X-UA-Compatible\" content=\"chrome=1\" />"
    "</head><body>Test HTML content</body></html>";
}  // end namespace

class UrlmonMonikerTest : public testing::Test {
 protected:
  UrlmonMonikerTest() {
  }
};

TEST_F(UrlmonMonikerTest, MonikerPatch) {
  EXPECT_EQ(true, MonikerPatch::Initialize());
  EXPECT_EQ(true, MonikerPatch::Initialize());  // Should be ok to call twice.
  MonikerPatch::Uninitialize();
}

// Runs an HTTP server on a worker thread that has a message loop.
class RunTestServer : public base::Thread {
 public:
  RunTestServer()
      : base::Thread("TestServer"),
      default_response_("/", kTestContent),
      ready_(::CreateEvent(NULL, TRUE, FALSE, NULL)) {
  }

  bool Start() {
    bool ret = StartWithOptions(Options(MessageLoop::TYPE_UI, 0));
    if (ret) {
      message_loop()->PostTask(FROM_HERE,
          NewRunnableFunction(&RunTestServer::StartServer, this));
      wait_until_ready();
    }
    return ret;
  }

  static void StartServer(RunTestServer* me) {
    me->server_.reset(new test_server::SimpleWebServer(43210));
    me->server_->AddResponse(&me->default_response_);
    ::SetEvent(me->ready_);
  }

  bool wait_until_ready() {
    return ::WaitForSingleObject(ready_, kUrlmonMonikerTimeoutSec * 1000)
           == WAIT_OBJECT_0;
  }

 protected:
  scoped_ptr<test_server::SimpleWebServer> server_;
  test_server::SimpleResponse default_response_;
  ScopedHandle ready_;
};

// Helper class for running tests that rely on the NavigationManager.
class UrlmonMonikerTestManager {
 public:
  explicit UrlmonMonikerTestManager(const wchar_t* test_url) {
    mock_mgr_.RegisterThreadInstance();
    mock_mgr_.set_url(test_url);
    EXPECT_EQ(true, MonikerPatch::Initialize());
  }

  ~UrlmonMonikerTestManager() {
    MonikerPatch::Uninitialize();
    mock_mgr_.UnregisterThreadInstance();
  }

  chrome_frame_test::TimedMsgLoop& loop() {
    return loop_;
  }

  TestNavigationManager& nav_manager() {
    return mock_mgr_;
  }

 protected:
  TestNavigationManager mock_mgr_;
  chrome_frame_test::TimedMsgLoop loop_;
};

// Wraps the MockBindStatusCallbackImpl mock object and allows the user
// to specify expectations on the callback object.
class UrlmonMonikerTestCallback {
 public:
  explicit UrlmonMonikerTestCallback(UrlmonMonikerTestManager* mgr)
      : mgr_(mgr) {
  }

  ~UrlmonMonikerTestCallback() {
  }

  typedef enum GetBindInfoExpectations {
    EXPECT_NO_CALL,
    REQUEST_SYNCHRONOUS,
    REQUEST_ASYNCHRONOUS,
  } GET_BIND_INFO_EXPECTATION;

  // Sets gmock expectations for the IBindStatusCallback mock object.
  void SetCallbackExpectations(GetBindInfoExpectations bind_info_handling,
                               HRESULT data_available_response,
                               bool quit_loop_on_stop) {
    EXPECT_CALL(callback_, OnProgress(_, _, _, _))
        .WillRepeatedly(Return(S_OK));

    if (bind_info_handling == REQUEST_ASYNCHRONOUS) {
      EXPECT_CALL(callback_, GetBindInfo(_, _))
          .WillOnce(DoAll(
              WithArgs<0, 1>(
                  Invoke(&MockBindStatusCallbackImpl::SetAsyncBindInfo)),
              Return(S_OK)));
    } else if (bind_info_handling == REQUEST_SYNCHRONOUS) {
      EXPECT_CALL(callback_, GetBindInfo(_, _))
          .WillOnce(DoAll(
              WithArgs<0, 1>(
                  Invoke(&MockBindStatusCallbackImpl::SetSyncBindInfo)),
              Return(S_OK)));
    } else {
      DCHECK(bind_info_handling == EXPECT_NO_CALL);
    }

    EXPECT_CALL(callback_, OnStartBinding(_, _))
        .WillOnce(Return(S_OK));

    EXPECT_CALL(callback_, OnDataAvailable(_, _, _, _))
        .WillRepeatedly(Return(data_available_response));

    if (quit_loop_on_stop) {
      // When expecting asynchronous
      EXPECT_CALL(callback_, OnStopBinding(data_available_response, _))
          .WillOnce(DoAll(QUIT_LOOP(mgr_->loop()), Return(S_OK)));
    } else {
      EXPECT_CALL(callback_, OnStopBinding(data_available_response, _))
          .WillOnce(Return(S_OK));
    }
  }

  HRESULT CreateUrlMonikerAndBindToStorage(const wchar_t* url,
                                           IBindCtx** bind_ctx) {
    ScopedComPtr<IMoniker> moniker;
    HRESULT hr = CreateURLMoniker(NULL, url, moniker.Receive());
    EXPECT_TRUE(moniker != NULL);
    if (moniker) {
      ScopedComPtr<IBindCtx> context;
      ::CreateAsyncBindCtx(0, callback(), NULL, context.Receive());
      DCHECK(context);
      ScopedComPtr<IStream> stream;
      hr = moniker->BindToStorage(context, NULL, IID_IStream,
          reinterpret_cast<void**>(stream.Receive()));
      if (SUCCEEDED(hr) && bind_ctx)
        *bind_ctx = context.Detach();
    }
    return hr;
  }

  IBindStatusCallback* callback() {
    return &callback_;
  }

 protected:
  CComObjectStackEx<MockBindStatusCallbackImpl> callback_;
  UrlmonMonikerTestManager* mgr_;
};

// Tests synchronously binding to a moniker and downloading the target.
TEST_F(UrlmonMonikerTest, BindToStorageSynchronous) {
  const wchar_t test_url[] = L"http://localhost:43210/";
  UrlmonMonikerTestManager test(test_url);
  UrlmonMonikerTestCallback callback(&test);

  RunTestServer server_thread;
  EXPECT_TRUE(server_thread.Start());

  callback.SetCallbackExpectations(
      UrlmonMonikerTestCallback::REQUEST_SYNCHRONOUS, S_OK, false);

  ScopedComPtr<IBindCtx> bind_ctx;
  HRESULT hr = callback.CreateUrlMonikerAndBindToStorage(test_url,
                                                         bind_ctx.Receive());
  // The download should have happened synchronously, so we don't expect
  // MK_S_ASYNCHRONOUS or any errors.
  EXPECT_EQ(S_OK, hr);

  IBindCtx* release = bind_ctx.Detach();
  EXPECT_EQ(0, release->Release());

  server_thread.Stop();

  EXPECT_FALSE(test.nav_manager().HasRequestData());
}

// Tests asynchronously binding to a moniker and downloading the target.
TEST_F(UrlmonMonikerTest, BindToStorageAsynchronous) {
  const wchar_t test_url[] = L"http://localhost:43210/";
  UrlmonMonikerTestManager test(test_url);
  UrlmonMonikerTestCallback callback(&test);

  test_server::SimpleWebServer server(43210);
  test_server::SimpleResponse default_response("/", kTestContent);
  server.AddResponse(&default_response);

  callback.SetCallbackExpectations(
      UrlmonMonikerTestCallback::REQUEST_ASYNCHRONOUS, S_OK, true);

  ScopedComPtr<IBindCtx> bind_ctx;
  HRESULT hr = callback.CreateUrlMonikerAndBindToStorage(test_url,
                                                         bind_ctx.Receive());
  EXPECT_EQ(MK_S_ASYNCHRONOUS, hr);
  test.loop().RunFor(kUrlmonMonikerTimeoutSec);

  IBindCtx* release = bind_ctx.Detach();
  EXPECT_EQ(0, release->Release());

  EXPECT_FALSE(test.nav_manager().HasRequestData());
}

// Responds with the Chrome mime type.
class ResponseWithContentType : public test_server::SimpleResponse {
 public:
  ResponseWithContentType(const char* request_path,
                          const std::string& contents)
    : test_server::SimpleResponse(request_path, contents) {
  }
  virtual bool GetContentType(std::string* content_type) const {
    *content_type = WideToASCII(kChromeMimeType);
    return true;
  }
};

// Downloads a document asynchronously and then verifies that the downloaded
// contents were cached and the cache contents are correct.
// TODO(tommi): Fix and re-enable.
//  http://code.google.com/p/chromium/issues/detail?id=39415
TEST_F(UrlmonMonikerTest, BindToStorageSwitchContent) {
  const wchar_t test_url[] = L"http://localhost:43210/";
  UrlmonMonikerTestManager test(test_url);
  UrlmonMonikerTestCallback callback(&test);

  test_server::SimpleWebServer server(43210);
  ResponseWithContentType default_response("/", kTestContent);
  server.AddResponse(&default_response);

  callback.SetCallbackExpectations(
      UrlmonMonikerTestCallback::REQUEST_ASYNCHRONOUS, INET_E_TERMINATED_BIND,
      true);

  HRESULT hr = callback.CreateUrlMonikerAndBindToStorage(test_url, NULL);
  EXPECT_EQ(MK_S_ASYNCHRONOUS, hr);
  test.loop().RunFor(kUrlmonMonikerTimeoutSec);

  scoped_refptr<RequestData> request_data(
      test.nav_manager().GetActiveRequestData(test_url));
  EXPECT_TRUE(request_data != NULL);

  if (request_data) {
    EXPECT_EQ(request_data->GetCachedContentSize(),
              arraysize(kTestContent) - 1);
    ScopedComPtr<IStream> stream;
    request_data->GetResetCachedContentStream(stream.Receive());
    EXPECT_TRUE(stream != NULL);
    if (stream) {
      char buffer[0xffff];
      DWORD read = 0;
      stream->Read(buffer, sizeof(buffer), &read);
      EXPECT_EQ(read, arraysize(kTestContent) - 1);
      EXPECT_EQ(0, memcmp(buffer, kTestContent, read));
    }
  }
}

// Fetches content asynchronously first to cache it and then
// verifies that fetching the cached content the same way works as expected
// and happens synchronously.
TEST_F(UrlmonMonikerTest, BindToStorageCachedContent) {
  const wchar_t test_url[] = L"http://localhost:43210/";
  UrlmonMonikerTestManager test(test_url);
  UrlmonMonikerTestCallback callback(&test);

  test_server::SimpleWebServer server(43210);
  ResponseWithContentType default_response("/", kTestContent);
  server.AddResponse(&default_response);

  // First set of expectations.  Download the contents
  // asynchronously.  This should populate the cache so that
  // the second request should be served synchronously without
  // going to the server.
  callback.SetCallbackExpectations(
      UrlmonMonikerTestCallback::REQUEST_ASYNCHRONOUS, INET_E_TERMINATED_BIND,
      true);

  HRESULT hr = callback.CreateUrlMonikerAndBindToStorage(test_url, NULL);
  EXPECT_EQ(MK_S_ASYNCHRONOUS, hr);
  test.loop().RunFor(kUrlmonMonikerTimeoutSec);

  scoped_refptr<RequestData> request_data(
      test.nav_manager().GetActiveRequestData(test_url));
  EXPECT_TRUE(request_data != NULL);

  if (request_data) {
    // This time, just accept the content as normal.
    UrlmonMonikerTestCallback callback2(&test);
    callback2.SetCallbackExpectations(
        UrlmonMonikerTestCallback::EXPECT_NO_CALL, S_OK, false);
    hr = callback2.CreateUrlMonikerAndBindToStorage(test_url, NULL);
    // S_OK means that the operation completed synchronously.
    // Otherwise we'd get MK_S_ASYNCHRONOUS.
    EXPECT_EQ(S_OK, hr);
  }
}

