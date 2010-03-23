// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <atlbase.h>
#include <atlcom.h>
#include "app/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"

#include "chrome_frame/urlmon_url_request.h"
#include "chrome_frame/urlmon_url_request_private.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/http_server.h"
using testing::CreateFunctor;

const int kChromeFrameLongNavigationTimeoutInSeconds = 10;

class MockUrlDelegate : public PluginUrlRequestDelegate {
 public:
  MOCK_METHOD7(OnResponseStarted, void(int request_id, const char* mime_type,
      const char* headers, int size, base::Time last_modified,
      const std::string& redirect_url, int redirect_status));
  MOCK_METHOD3(OnReadComplete, void(int request_id, const void* buffer,
                                    int len));
  MOCK_METHOD2(OnResponseEnd, void(int request_id,
                                   const URLRequestStatus& status));

  static bool ImplementsThreadSafeReferenceCounting() {
    return false;
  }
  void AddRef() {}
  void Release() {}

  void PostponeReadRequest(chrome_frame_test::TimedMsgLoop* loop,
                   UrlmonUrlRequest* request, int bytes_to_read) {
    loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(this,
                          &MockUrlDelegate::Read, request, bytes_to_read), 0);
  }

 private:
  void Read(UrlmonUrlRequest* request, int bytes_to_read) {
    request->Read(bytes_to_read);
  }
};

// Simplest UrlmonUrlRequest. Retrieve a file from local web server.
TEST(UrlmonUrlRequestTest, Simple1) {
  MockUrlDelegate mock;
  ChromeFrameHTTPServer server;
  chrome_frame_test::TimedMsgLoop loop;
  win_util::ScopedCOMInitializer init_com;
  CComObjectStackEx<UrlmonUrlRequest> request;

  server.SetUp();
  request.AddRef();
  request.Initialize(&mock, 1,  // request_id
      server.Resolve(L"files/chrome_frame_window_open.html").spec(),
      "get",
      "",      // referrer
      "",      // extra request
      NULL,    // upload data
      true);   // frame busting

  testing::InSequence s;
  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_))
    .Times(1)
    .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
        &request, &UrlmonUrlRequest::Read, 512))));


  EXPECT_CALL(mock, OnReadComplete(1, testing::_, testing::Gt(0)))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::InvokeWithoutArgs(CreateFunctor(&mock,
        &MockUrlDelegate::PostponeReadRequest, &loop, &request, 64)));

  EXPECT_CALL(mock, OnResponseEnd(1, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  request.Start();
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  request.Release();
  server.TearDown();
}

// Same as Simple1 except we use the HEAD verb to fetch only the headers
// from the server.
TEST(UrlmonUrlRequestTest, Head) {
  MockUrlDelegate mock;
  ChromeFrameHTTPServer server;
  chrome_frame_test::TimedMsgLoop loop;
  win_util::ScopedCOMInitializer init_com;
  CComObjectStackEx<UrlmonUrlRequest> request;

  server.SetUp();
  request.AddRef();
  request.Initialize(&mock, 1,  // request_id
      server.Resolve(L"files/chrome_frame_window_open.html").spec(),
      "head",
      "",      // referrer
      "",      // extra request
      NULL,    // upload data
      true);   // frame busting

  testing::InSequence s;
  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_))
    .Times(1)
    .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
        &request, &UrlmonUrlRequest::Read, 512))));


  // For HEAD requests we don't expect content reads.
  EXPECT_CALL(mock, OnReadComplete(1, testing::_, testing::_)).Times(0);

  EXPECT_CALL(mock, OnResponseEnd(1, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  request.Start();
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  request.Release();
  server.TearDown();
}

TEST(UrlmonUrlRequestTest, UnreachableUrl) {
  MockUrlDelegate mock;
  chrome_frame_test::TimedMsgLoop loop;
  win_util::ScopedCOMInitializer init_com;
  CComObjectStackEx<UrlmonUrlRequest> request;
  ChromeFrameHTTPServer server;
  server.SetUp();
  GURL unreachable = server.Resolve(L"files/non_existing.html");
  server.TearDown();

  request.AddRef();
  request.Initialize(&mock, 1,  // request_id
      unreachable.spec(), "get",
      "",      // referrer
      "",      // extra request
      NULL,    // upload data
      true);   // frame busting

  EXPECT_CALL(mock, OnResponseEnd(1, testing::Property(
              &URLRequestStatus::os_error, net::ERR_TUNNEL_CONNECTION_FAILED)))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  request.Start();
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  request.Release();
}

TEST(UrlmonUrlRequestTest, ZeroLengthResponse) {
  MockUrlDelegate mock;
  ChromeFrameHTTPServer server;
  chrome_frame_test::TimedMsgLoop loop;
  win_util::ScopedCOMInitializer init_com;
  CComObjectStackEx<UrlmonUrlRequest> request;

  server.SetUp();
  request.AddRef();
  request.Initialize(&mock, 1,  // request_id
      server.Resolve(L"files/empty.html").spec(), "get",
      "",      // referrer
      "",      // extra request
      NULL,    // upload data
      true);   // frame busting

  // Expect headers
  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP(loop));

  request.Start();
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  EXPECT_FALSE(loop.WasTimedOut());

  // Should stay quiet, since we do not ask for anything for awhile.
  EXPECT_CALL(mock, OnResponseEnd(1, testing::_)).Times(0);
  loop.RunFor(3);

  // Invoke read. Only now the response end ("server closed the connection")
  // is supposed to be delivered.
  EXPECT_CALL(mock, OnResponseEnd(1, testing::Property(
                                     &URLRequestStatus::is_success, true)))
      .Times(1);
  request.Read(512);
  request.Release();
  server.TearDown();
}

// Simplest test - retrieve file from local web server.
TEST(UrlmonUrlRequestManagerTest, Simple1) {
  MockUrlDelegate mock;
  ChromeFrameHTTPServer server;
  chrome_frame_test::TimedMsgLoop loop;
  server.SetUp();
  scoped_ptr<UrlmonUrlRequestManager> mgr(new UrlmonUrlRequestManager());
  mgr->set_delegate(&mock);
  IPC::AutomationURLRequest r1 = {
      server.Resolve(L"files/chrome_frame_window_open.html").spec(), "get" };

  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                             testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::InvokeWithoutArgs(CreateFunctor(mgr.get(),
          &PluginUrlRequestManager::ReadUrlRequest, 0, 1, 512)));

  EXPECT_CALL(mock, OnReadComplete(1, testing::_, testing::Gt(0)))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::InvokeWithoutArgs(CreateFunctor(mgr.get(),
        &PluginUrlRequestManager::ReadUrlRequest, 0, 1, 2)));

  EXPECT_CALL(mock, OnResponseEnd(1, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  mgr->StartUrlRequest(0, 1, r1);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mgr.reset();
  server.TearDown();
}

TEST(UrlmonUrlRequestManagerTest, Abort1) {
  MockUrlDelegate mock;
  ChromeFrameHTTPServer server;
  chrome_frame_test::TimedMsgLoop loop;
  server.SetUp();
  scoped_ptr<UrlmonUrlRequestManager> mgr(new UrlmonUrlRequestManager());
  mgr->set_delegate(&mock);
  IPC::AutomationURLRequest r1 = {
      server.Resolve(L"files/chrome_frame_window_open.html").spec(), "get" };

  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                               testing::_, testing::_, testing::_))
    .Times(1)
    .WillOnce(testing::DoAll(
        testing::InvokeWithoutArgs(CreateFunctor(mgr.get(),
            &PluginUrlRequestManager::EndUrlRequest, 0, 1, URLRequestStatus())),
        testing::InvokeWithoutArgs(CreateFunctor(mgr.get(),
            &PluginUrlRequestManager::ReadUrlRequest, 0, 1, 2))));

  EXPECT_CALL(mock, OnReadComplete(1, testing::_, testing::Gt(0)))
    .Times(0);

  EXPECT_CALL(mock, OnResponseEnd(1, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  mgr->StartUrlRequest(0, 1, r1);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mgr.reset();
  server.TearDown();
}

