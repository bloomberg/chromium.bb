// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/common/automation_messages.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/test_server.h"
#include "chrome_frame/test/test_with_web_server.h"
#include "chrome_frame/urlmon_url_request.h"
#include "chrome_frame/urlmon_url_request_private.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::CreateFunctor;

const int kChromeFrameLongNavigationTimeoutInSeconds = 10;

static void AppendToStream(IStream* s, void* buffer, ULONG cb) {
  ULONG bytes_written;
  LARGE_INTEGER current_pos;
  LARGE_INTEGER zero = {0};
  // Remember current position.
  ASSERT_HRESULT_SUCCEEDED(s->Seek(zero, STREAM_SEEK_CUR,
      reinterpret_cast<ULARGE_INTEGER*>(&current_pos)));
  // Seek to the end.
  ASSERT_HRESULT_SUCCEEDED(s->Seek(zero, STREAM_SEEK_END, NULL));
  ASSERT_HRESULT_SUCCEEDED(s->Write(buffer, cb, &bytes_written));
  ASSERT_EQ(cb, bytes_written);
  // Seek to original position.
  ASSERT_HRESULT_SUCCEEDED(s->Seek(current_pos, STREAM_SEEK_SET, NULL));
}

class MockUrlDelegate : public PluginUrlRequestDelegate {
 public:
  MOCK_METHOD8(OnResponseStarted, void(int request_id, const char* mime_type,
      const char* headers, int size, base::Time last_modified,
      const std::string& redirect_url, int redirect_status,
      const net::HostPortPair& socket_address));
  MOCK_METHOD2(OnReadComplete, void(int request_id, const std::string& data));
  MOCK_METHOD2(OnResponseEnd, void(int request_id,
                                   const net::URLRequestStatus& status));
  MOCK_METHOD4(OnCookiesRetrieved, void(bool success, const GURL& url,
      const std::string& cookie, int cookie_id));

  static bool ImplementsThreadSafeReferenceCounting() {
    return false;
  }

  void PostponeReadRequest(chrome_frame_test::TimedMsgLoop* loop,
                   UrlmonUrlRequest* request, int bytes_to_read) {
    loop->PostTask(FROM_HERE,
                   base::Bind(&MockUrlDelegate::RequestRead,
                              base::Unretained(this), request, bytes_to_read));
  }

 private:
  void RequestRead(UrlmonUrlRequest* request, int bytes_to_read) {
    request->Read(bytes_to_read);
  }
};

// Simplest UrlmonUrlRequest. Retrieve a file from local web server.
TEST(UrlmonUrlRequestTest, Simple1) {
  MockUrlDelegate mock;
  chrome_frame_test::TimedMsgLoop loop;

  testing::StrictMock<MockWebServer> mock_server(1337, L"127.0.0.1",
      chrome_frame_test::GetTestDataFolder());
  mock_server.ExpectAndServeAnyRequests(CFInvocation(CFInvocation::NONE));

  base::win::ScopedCOMInitializer init_com;
  CComObjectStackEx<UrlmonUrlRequest> request;

  request.AddRef();
  request.Initialize(&mock, 1,  // request_id
      WideToUTF8(mock_server.Resolve(L"chrome_frame_window_open.html")),
      "get",
      "",      // referrer
      "",      // extra request
      NULL,    // upload data
      ResourceType::MAIN_FRAME,  // resource type
      true,
      0);   // frame busting

  testing::InSequence s;
  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_,
                                      testing::_))
    .Times(1)
    .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
        &request, &UrlmonUrlRequest::Read, 512))));


  EXPECT_CALL(mock, OnReadComplete(1, testing::Property(&std::string::size,
                                                        testing::Gt(0u))))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(testing::InvokeWithoutArgs(CreateFunctor(&mock,
        &MockUrlDelegate::PostponeReadRequest, &loop, &request, 64)));

  EXPECT_CALL(mock, OnResponseEnd(1, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  request.Start();
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  request.Release();
}

// Same as Simple1 except we use the HEAD verb to fetch only the headers
// from the server.
TEST(UrlmonUrlRequestTest, Head) {
  MockUrlDelegate mock;
  chrome_frame_test::TimedMsgLoop loop;
  // Use SimpleWebServer instead of the python server to support HEAD
  // requests.
  test_server::SimpleWebServer server(13337);
  test_server::SimpleResponse head_response("/head", "");
  server.AddResponse(&head_response);

  base::win::ScopedCOMInitializer init_com;
  CComObjectStackEx<UrlmonUrlRequest> request;

  request.AddRef();
  request.Initialize(&mock, 1,  // request_id
      "http://localhost:13337/head",
      "head",
      "",      // referrer
      "",      // extra request
      NULL,    // upload data
      ResourceType::MAIN_FRAME,  // resource type
      true,
      0);   // frame busting

  testing::InSequence s;
  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_,
                                      testing::_))
    .Times(1)
    .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
        &request, &UrlmonUrlRequest::Read, 512))));

  // For HEAD requests we don't expect content reads.
  EXPECT_CALL(mock, OnReadComplete(1, testing::_)).Times(0);

  EXPECT_CALL(mock, OnResponseEnd(1, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  request.Start();
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  request.Release();
}

TEST(UrlmonUrlRequestTest, UnreachableUrl) {
  MockUrlDelegate mock;
  chrome_frame_test::TimedMsgLoop loop;
  base::win::ScopedCOMInitializer init_com;
  CComObjectStackEx<UrlmonUrlRequest> request;

  testing::StrictMock<MockWebServer> mock_server(1337, L"127.0.0.1",
      chrome_frame_test::GetTestDataFolder());
  mock_server.ExpectAndServeAnyRequests(CFInvocation(CFInvocation::NONE));

  GURL unreachable(WideToUTF8(mock_server.Resolve(
      L"non_existing.html")));

  request.AddRef();
  request.Initialize(&mock, 1,  // request_id
      unreachable.spec(), "get",
      "",      // referrer
      "",      // extra request
      NULL,    // upload data
      ResourceType::MAIN_FRAME,  // resource type
      true,
      0);   // frame busting

  // Expect headers
  EXPECT_CALL(mock, OnResponseStarted(1, testing::_,
                                      testing::StartsWith("HTTP/1.1 404"),
                                      testing::_, testing::_, testing::_,
                                      testing::_, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  EXPECT_CALL(mock, OnResponseEnd(1, testing::Property(
              &net::URLRequestStatus::error,
              net::ERR_TUNNEL_CONNECTION_FAILED)))
    .Times(testing::AtMost(1));

  request.Start();
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  request.Release();
}

TEST(UrlmonUrlRequestTest, ZeroLengthResponse) {
  MockUrlDelegate mock;
  chrome_frame_test::TimedMsgLoop loop;

  testing::StrictMock<MockWebServer> mock_server(1337, L"127.0.0.1",
      chrome_frame_test::GetTestDataFolder());
  mock_server.ExpectAndServeAnyRequests(CFInvocation(CFInvocation::NONE));

  base::win::ScopedCOMInitializer init_com;
  CComObjectStackEx<UrlmonUrlRequest> request;

  request.AddRef();
  request.Initialize(&mock, 1,  // request_id
      WideToUTF8(mock_server.Resolve(L"empty.html")), "get",
      "",      // referrer
      "",      // extra request
      NULL,    // upload data
      ResourceType::MAIN_FRAME,  // resource type
      true,
      0);   // frame busting

  // Expect headers
  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                                      testing::_, testing::_, testing::_,
                                      testing::_))
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
      &net::URLRequestStatus::is_success, true))).Times(1);
  request.Read(512);
  request.Release();
}

ACTION_P4(ManagerRead, loop, mgr, request_id, bytes_to_read) {
  loop->PostTask(FROM_HERE,
                 base::Bind(&UrlmonUrlRequestManager::ReadUrlRequest,
                            base::Unretained(mgr), request_id, bytes_to_read));
}
ACTION_P3(ManagerEndRequest, loop, mgr, request_id) {
  loop->PostTask(FROM_HERE, base::Bind(&UrlmonUrlRequestManager::EndUrlRequest,
                                       base::Unretained(mgr), request_id,
                                       net::URLRequestStatus()));
}

// Simplest test - retrieve file from local web server.
TEST(UrlmonUrlRequestManagerTest, Simple1) {
  MockUrlDelegate mock;
  chrome_frame_test::TimedMsgLoop loop;

  testing::StrictMock<MockWebServer> mock_server(1337, L"127.0.0.1",
      chrome_frame_test::GetTestDataFolder());
  mock_server.ExpectAndServeAnyRequests(CFInvocation(CFInvocation::NONE));

  scoped_ptr<UrlmonUrlRequestManager> mgr(new UrlmonUrlRequestManager());
  mgr->set_delegate(&mock);
  AutomationURLRequest r1;
  r1.url =  WideToUTF8(mock_server.Resolve(L"chrome_frame_window_open.html"));
  r1.method = "get";
  r1.resource_type = 0;
  r1.load_flags = 0;

  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                             testing::_, testing::_, testing::_, testing::_))
      .Times(1)
      .WillOnce(ManagerRead(&loop, mgr.get(), 1, 512));

  EXPECT_CALL(mock, OnReadComplete(1, testing::Property(&std::string::size,
                                                        testing::Gt(0u))))
    .Times(testing::AtLeast(1))
    .WillRepeatedly(ManagerRead(&loop, mgr.get(), 1, 2));

  EXPECT_CALL(mock, OnResponseEnd(1, testing::_))
    .Times(1)
    .WillOnce(QUIT_LOOP_SOON(loop, 2));

  mgr->StartUrlRequest(1, r1);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mgr.reset();
}

TEST(UrlmonUrlRequestManagerTest, Abort1) {
  MockUrlDelegate mock;
  chrome_frame_test::TimedMsgLoop loop;

  testing::StrictMock<MockWebServer> mock_server(1337, L"127.0.0.1",
      chrome_frame_test::GetTestDataFolder());
  mock_server.ExpectAndServeAnyRequests(CFInvocation(CFInvocation::NONE));

  scoped_ptr<UrlmonUrlRequestManager> mgr(new UrlmonUrlRequestManager());
  mgr->set_delegate(&mock);
  AutomationURLRequest r1;
  r1.url = WideToUTF8(mock_server.Resolve(L"chrome_frame_window_open.html"));
  r1.method = "get";
  r1.resource_type = 0;
  r1.load_flags = 0;

  EXPECT_CALL(mock, OnResponseStarted(1, testing::_, testing::_, testing::_,
                               testing::_, testing::_, testing::_, testing::_))
    .Times(1)
    .WillOnce(testing::DoAll(
        ManagerEndRequest(&loop, mgr.get(), 1),
        QUIT_LOOP_SOON(loop, 3)));

  EXPECT_CALL(mock, OnReadComplete(1, testing::_))
    .Times(0);

  EXPECT_CALL(mock, OnResponseEnd(1, testing::_))
    .Times(0);

  mgr->StartUrlRequest(1, r1);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mgr.reset();
}
