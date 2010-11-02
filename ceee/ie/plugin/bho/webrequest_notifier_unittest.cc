// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for WebRequestNotifier class.
#include "ceee/ie/plugin/bho/webrequest_notifier.h"

#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::InSequence;
using testing::MockFunction;
using testing::StrictMock;

class TestWebRequestNotifier : public WebRequestNotifier {
 public:
  TestWebRequestNotifier()
      : mock_method_(L"GET"),
        mock_status_code_(200),
        mock_transfer_encoding_present_(false),
        mock_content_length_(0),
        mock_content_length_present_(false) {
  }
  virtual ~TestWebRequestNotifier() {}

  virtual WebRequestEventsFunnel& webrequest_events_funnel() {
    return mock_webrequest_events_funnel_;
  }

  virtual bool QueryHttpInfoString(HINTERNET /*request*/,
                                   DWORD info_flag,
                                   std::wstring* value) {
    if (value == NULL)
      return false;
    if (info_flag == HTTP_QUERY_REQUEST_METHOD) {
      *value = mock_method_;
      return true;
    } else if (mock_transfer_encoding_present_ &&
               info_flag == HTTP_QUERY_TRANSFER_ENCODING) {
      *value = mock_transfer_encoding_;
      return true;
    } else {
      return false;
    }
  }

  virtual bool QueryHttpInfoNumber(HINTERNET /*request*/,
                                   DWORD info_flag,
                                   DWORD* value) {
    if (value == NULL)
      return false;
    if (info_flag == HTTP_QUERY_STATUS_CODE) {
      *value = mock_status_code_;
      return true;
    } else if (mock_content_length_present_ &&
               info_flag == HTTP_QUERY_CONTENT_LENGTH) {
      *value = mock_content_length_;
      return true;
    } else {
      return false;
    }
  }

  StrictMock<testing::MockWebRequestEventsFunnel>
      mock_webrequest_events_funnel_;

  std::wstring mock_method_;
  DWORD mock_status_code_;
  std::wstring mock_transfer_encoding_;
  bool mock_transfer_encoding_present_;
  DWORD mock_content_length_;
  bool mock_content_length_present_;

  friend class GTEST_TEST_CLASS_NAME_(WebRequestNotifierTestFixture,
                                      TestConstructUrl);
  friend class GTEST_TEST_CLASS_NAME_(WebRequestNotifierTestFixture,
                                      TestDetermineMessageLength);
  friend class GTEST_TEST_CLASS_NAME_(WebRequestNotifierTestFixture,
                                      TestTransitRequestToNextState);
  friend class GTEST_TEST_CLASS_NAME_(WebRequestNotifierTestFixture,
                                      TestWinINetPatchHandlers);
};

class WebRequestNotifierTestFixture : public testing::Test {
 protected:
  virtual void SetUp() {
    webrequest_notifier_.reset(new TestWebRequestNotifier());
    ASSERT_TRUE(webrequest_notifier_->RequestToStart());
  }

  virtual void TearDown() {
    webrequest_notifier_->RequestToStop();
    webrequest_notifier_.reset(NULL);
  }

  scoped_ptr<TestWebRequestNotifier> webrequest_notifier_;
};

TEST_F(WebRequestNotifierTestFixture, TestConstructUrl) {
  std::wstring output;
  EXPECT_TRUE(webrequest_notifier_->ConstructUrl(
      true, L"www.google.com", INTERNET_INVALID_PORT_NUMBER, L"/foobar",
      &output));
  EXPECT_STREQ(L"https://www.google.com/foobar", output.c_str());

  EXPECT_TRUE(webrequest_notifier_->ConstructUrl(
      false, L"mail.google.com", INTERNET_DEFAULT_HTTP_PORT, L"/index.html",
      &output));
  EXPECT_STREQ(L"http://mail.google.com/index.html", output.c_str());

  EXPECT_TRUE(webrequest_notifier_->ConstructUrl(
      false, L"docs.google.com", INTERNET_DEFAULT_HTTPS_PORT, L"/login",
      &output));
  EXPECT_STREQ(L"http://docs.google.com:443/login", output.c_str());

  EXPECT_TRUE(webrequest_notifier_->ConstructUrl(
      true, L"image.google.com", 123, L"/helloworld", &output));
  EXPECT_STREQ(L"https://image.google.com:123/helloworld", output.c_str());
}

TEST_F(WebRequestNotifierTestFixture, TestDetermineMessageLength) {
  DWORD length = 0;
  WebRequestNotifier::RequestInfo::MessageLengthType type =
      WebRequestNotifier::RequestInfo::UNKNOWN_MESSAGE_LENGTH_TYPE;
  HINTERNET request = reinterpret_cast<HINTERNET>(1024);

  // Requests with "HEAD" method don't have a message body in the response.
  webrequest_notifier_->mock_method_ = L"HEAD";
  EXPECT_TRUE(webrequest_notifier_->DetermineMessageLength(
      request, 200, &length, &type));
  EXPECT_EQ(0, length);
  EXPECT_EQ(WebRequestNotifier::RequestInfo::NO_MESSAGE_BODY, type);

  // Requests with status code 1XX, 204 or 304 don't have a message body in the
  // response.
  webrequest_notifier_->mock_method_ = L"GET";
  EXPECT_TRUE(webrequest_notifier_->DetermineMessageLength(
      request, 100, &length, &type));
  EXPECT_EQ(0, length);
  EXPECT_EQ(WebRequestNotifier::RequestInfo::NO_MESSAGE_BODY, type);

  // If a Transfer-Encoding header field is present and has any value other than
  // "identity", the response body length is variable.
  // The Content-Length field is ignored in this case.
  webrequest_notifier_->mock_transfer_encoding_present_ = true;
  webrequest_notifier_->mock_transfer_encoding_ = L"chunked";
  webrequest_notifier_->mock_content_length_present_ = true;
  webrequest_notifier_->mock_content_length_ = 256;
  EXPECT_TRUE(webrequest_notifier_->DetermineMessageLength(
      request, 200, &length, &type));
  EXPECT_EQ(0, length);
  EXPECT_EQ(WebRequestNotifier::RequestInfo::VARIABLE_MESSAGE_LENGTH, type);

  // If a Content-Length header field is present, the response body length is
  // the same as specified in the Content-Length header.
  webrequest_notifier_->mock_transfer_encoding_present_ = false;
  EXPECT_TRUE(webrequest_notifier_->DetermineMessageLength(
      request, 200, &length, &type));
  EXPECT_EQ(256, length);
  EXPECT_EQ(WebRequestNotifier::RequestInfo::CONTENT_LENGTH_HEADER, type);

  // Otherwise, consider the response body length is variable.
  webrequest_notifier_->mock_content_length_present_ = false;
  EXPECT_TRUE(webrequest_notifier_->DetermineMessageLength(
      request, 200, &length, &type));
  EXPECT_EQ(0, length);
  EXPECT_EQ(WebRequestNotifier::RequestInfo::VARIABLE_MESSAGE_LENGTH, type);
}

TEST_F(WebRequestNotifierTestFixture, TestTransitRequestToNextState) {
  scoped_ptr<WebRequestNotifier::RequestInfo> info(
      new WebRequestNotifier::RequestInfo());
  MockFunction<void(int check_point)> check;
  {
    InSequence sequence;
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnBeforeRequest(_, _, _, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnRequestSent(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnHeadersReceived(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnBeforeRedirect(_, _, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnRequestSent(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnHeadersReceived(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnCompleted(_, _, _, _));
    EXPECT_CALL(check, Call(1));

    EXPECT_CALL(check, Call(2));

    EXPECT_CALL(check, Call(3));

    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnBeforeRequest(_, _, _, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnRequestSent(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnErrorOccurred(_, _, _, _));
    EXPECT_CALL(check, Call(4));

    EXPECT_CALL(check, Call(5));
  }

  // The normal state transition sequence.
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::WILL_NOTIFY_BEFORE_REQUEST, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_BEFORE_REQUEST, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_REQUEST_SENT, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_HEADERS_RECEIVED, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_BEFORE_REDIRECT, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_REQUEST_SENT, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_HEADERS_RECEIVED, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_COMPLETED, info.get());
  check.Call(1);

  // No event is fired after webRequest.onCompleted for any request.
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_BEFORE_REQUEST, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::ERROR_OCCURRED, info.get());
  check.Call(2);

  // No webRequest.onErrorOccurred is fired since the first event for any
  // request has to be webRequest.onBeforeRequest.
  info.reset(new WebRequestNotifier::RequestInfo());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::WILL_NOTIFY_BEFORE_REQUEST, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::ERROR_OCCURRED, info.get());
  check.Call(3);

  // Unexpected next-state will result in webRequest.onErrorOccurred to be
  // fired.
  info.reset(new WebRequestNotifier::RequestInfo());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::WILL_NOTIFY_BEFORE_REQUEST, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_BEFORE_REQUEST, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_REQUEST_SENT, info.get());
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_COMPLETED, info.get());
  check.Call(4);

  // No event is fired after webRequest.onErrorOccurred for any request.
  webrequest_notifier_->TransitRequestToNextState(
      WebRequestNotifier::RequestInfo::NOTIFIED_HEADERS_RECEIVED, info.get());
  check.Call(5);
}

TEST_F(WebRequestNotifierTestFixture, TestWinINetPatchHandlers) {
  MockFunction<void(int check_point)> check;
  {
    InSequence sequence;
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnBeforeRequest(_, _, _, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnRequestSent(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnHeadersReceived(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnBeforeRedirect(_, _, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnRequestSent(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnHeadersReceived(_, _, _, _));
    EXPECT_CALL(webrequest_notifier_->mock_webrequest_events_funnel_,
                OnCompleted(_, _, _, _));
    EXPECT_CALL(check, Call(1));
  }

  HINTERNET internet = reinterpret_cast<HINTERNET>(512);
  HINTERNET server = reinterpret_cast<HINTERNET>(1024);
  HINTERNET request = reinterpret_cast<HINTERNET>(2048);

  webrequest_notifier_->mock_method_ = L"GET";
  webrequest_notifier_->mock_status_code_ = 200;
  webrequest_notifier_->mock_content_length_ = 256;
  webrequest_notifier_->mock_content_length_present_ = true;

  webrequest_notifier_->HandleBeforeInternetConnect(internet);
  webrequest_notifier_->HandleAfterInternetConnect(
      server, L"www.google.com", INTERNET_DEFAULT_HTTP_PORT,
      INTERNET_SERVICE_HTTP);

  webrequest_notifier_->HandleAfterHttpOpenRequest(server, request, "GET",
                                                   L"/", 0);

  webrequest_notifier_->HandleBeforeHttpSendRequest(request);

  webrequest_notifier_->HandleBeforeInternetStatusCallback(
      NULL, request, NULL, INTERNET_STATUS_SENDING_REQUEST, NULL, 0);
  webrequest_notifier_->HandleBeforeInternetStatusCallback(
      NULL, request, NULL, INTERNET_STATUS_REQUEST_SENT, NULL, 0);
  webrequest_notifier_->HandleBeforeInternetStatusCallback(
      NULL, request, NULL, INTERNET_STATUS_REDIRECT,
      "http://www.google.com/index.html", 32);
  webrequest_notifier_->HandleBeforeInternetStatusCallback(
      NULL, request, NULL, INTERNET_STATUS_SENDING_REQUEST, NULL, 0);
  webrequest_notifier_->HandleBeforeInternetStatusCallback(
      NULL, request, NULL, INTERNET_STATUS_REQUEST_SENT, NULL, 0);
  webrequest_notifier_->HandleBeforeInternetStatusCallback(
      NULL, request, NULL, INTERNET_STATUS_REQUEST_COMPLETE, NULL, 0);

  DWORD number_of_bytes_read = 64;
  webrequest_notifier_->HandleAfterInternetReadFile(request, TRUE,
      &number_of_bytes_read);
  number_of_bytes_read = 128;
  webrequest_notifier_->HandleAfterInternetReadFile(request, TRUE,
      &number_of_bytes_read);
  number_of_bytes_read = 64;
  webrequest_notifier_->HandleAfterInternetReadFile(request, TRUE,
      &number_of_bytes_read);

  // Since we have read the whole response body, webRequest.onCompleted has been
  // sent at this point.
  check.Call(1);

  webrequest_notifier_->HandleBeforeInternetStatusCallback(
      NULL, request, NULL, INTERNET_STATUS_HANDLE_CLOSING, NULL, 0);
  webrequest_notifier_->HandleBeforeInternetStatusCallback(
      NULL, server, NULL, INTERNET_STATUS_HANDLE_CLOSING, NULL, 0);
}

}  // namespace
