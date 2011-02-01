// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for WebRequestEventsFunnel.

#include <atlcomcli.h>

#include "base/time.h"
#include "base/values.h"
#include "ceee/ie/plugin/bho/webrequest_events_funnel.h"
#include "chrome/browser/extensions/extension_webrequest_api_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace keys = extension_webrequest_api_constants;

namespace {

using testing::Return;
using testing::StrEq;

MATCHER_P(ValuesEqual, value, "") {
  return arg.Equals(value);
}

class TestWebRequestEventsFunnel : public WebRequestEventsFunnel {
 public:
  MOCK_METHOD2(SendEvent, HRESULT(const char*, const Value&));
};

TEST(WebRequestEventsFunnelTest, OnBeforeRedirect) {
  TestWebRequestEventsFunnel webrequest_events_funnel;

  int request_id = 256;
  std::wstring url(L"http://www.google.com/");
  DWORD status_code = 200;
  std::wstring redirect_url(L"http://mail.google.com/");
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kStatusCodeKey, status_code);
  args.SetString(keys::kRedirectUrlKey, redirect_url);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webrequest_events_funnel,
              SendEvent(StrEq(keys::kOnBeforeRedirect), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webrequest_events_funnel.OnBeforeRedirect(
      request_id, url.c_str(), status_code, redirect_url.c_str(), time_stamp));
}

TEST(WebRequestEventsFunnelTest, OnBeforeRequest) {
  TestWebRequestEventsFunnel webrequest_events_funnel;

  int request_id = 256;
  std::wstring url(L"http://calendar.google.com/");
  std::string method("GET");
  int tab_handle = 512;
  std::string type("main_frame");
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetString(keys::kMethodKey, method);
  args.SetInteger(keys::kTabIdKey, tab_handle);
  args.SetString(keys::kTypeKey, type);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webrequest_events_funnel,
              SendEvent(StrEq(keys::kOnBeforeRequest), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webrequest_events_funnel.OnBeforeRequest(
      request_id, url.c_str(), method.c_str(),
      static_cast<CeeeWindowHandle>(tab_handle), type.c_str(), time_stamp));
}

TEST(WebRequestEventsFunnelTest, OnCompleted) {
  TestWebRequestEventsFunnel webrequest_events_funnel;

  int request_id = 256;
  std::wstring url(L"http://image.google.com/");
  DWORD status_code = 404;
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kStatusCodeKey, status_code);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webrequest_events_funnel,
              SendEvent(StrEq(keys::kOnCompleted), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webrequest_events_funnel.OnCompleted(
      request_id, url.c_str(), status_code, time_stamp));
}

TEST(WebRequestEventsFunnelTest, OnErrorOccurred) {
  TestWebRequestEventsFunnel webrequest_events_funnel;

  int request_id = 256;
  std::wstring url(L"http://docs.google.com/");
  std::wstring error(L"cannot resolve the host");
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetString(keys::kErrorKey, error);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webrequest_events_funnel,
              SendEvent(StrEq(keys::kOnErrorOccurred), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webrequest_events_funnel.OnErrorOccurred(
      request_id, url.c_str(), error.c_str(), time_stamp));
}

TEST(WebRequestEventsFunnelTest, OnHeadersReceived) {
  TestWebRequestEventsFunnel webrequest_events_funnel;

  int request_id = 256;
  std::wstring url(L"http://news.google.com/");
  DWORD status_code = 200;
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kStatusCodeKey, status_code);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webrequest_events_funnel,
              SendEvent(StrEq(keys::kOnHeadersReceived), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webrequest_events_funnel.OnHeadersReceived(
      request_id, url.c_str(), status_code, time_stamp));
}

TEST(WebRequestEventsFunnelTest, OnRequestSent) {
  TestWebRequestEventsFunnel webrequest_events_funnel;

  int request_id = 256;
  std::wstring url(L"http://finance.google.com/");
  std::string ip("127.0.0.1");
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetString(keys::kUrlKey, url);
  args.SetString(keys::kIpKey, ip);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webrequest_events_funnel,
              SendEvent(StrEq(keys::kOnRequestSent), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webrequest_events_funnel.OnRequestSent(
      request_id, url.c_str(), ip.c_str(), time_stamp));
}

}  // namespace
