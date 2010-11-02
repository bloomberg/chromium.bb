// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE browser helper object implementation.
#include "ceee/ie/plugin/bho/cookie_accountant.h"

#include <wininet.h>

#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/mock_static.h"
#include "ceee/testing/utils/test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::MockCookieEventsFunnel;
using testing::Return;

bool StringsNullOrEqual(wchar_t* a, wchar_t* b) {
  if (a == NULL && b == NULL)
    return true;
  if (a != NULL && b != NULL && wcscmp(a, b) == 0)
    return true;
  return false;
}

MATCHER_P(CookiesEqual, cookie, "") {
  return
      StringsNullOrEqual(arg.name, cookie->name) &&
      StringsNullOrEqual(arg.value, cookie->value) &&
      StringsNullOrEqual(arg.domain, cookie->domain) &&
      arg.host_only == cookie->host_only &&
      StringsNullOrEqual(arg.path, cookie->path) &&
      arg.secure == cookie->secure &&
      arg.http_only == cookie->http_only &&
      arg.session == cookie->session &&
      arg.expiration_date == cookie->expiration_date;
}

// Mock WinInet functions.
MOCK_STATIC_CLASS_BEGIN(MockWinInet)
  MOCK_STATIC_INIT_BEGIN(MockWinInet)
    MOCK_STATIC_INIT(InternetSetCookieExA);
    MOCK_STATIC_INIT(InternetSetCookieExW);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC5(DWORD, CALLBACK, InternetSetCookieExA, LPCSTR, LPCSTR, LPCSTR,
               DWORD, DWORD_PTR);
  MOCK_STATIC5(DWORD, CALLBACK, InternetSetCookieExW, LPCWSTR, LPCWSTR,
               LPCWSTR, DWORD, DWORD_PTR);
MOCK_STATIC_CLASS_END(MockWinInet)

class MockCookieAccountant : public CookieAccountant {
 public:
  MOCK_METHOD3(RecordCookie,
               void(const std::string&, const std::string&,
                    const base::Time&));

  void CallRecordCookie(const std::string& url,
                              const std::string& cookie_data,
                              const base::Time& current_time) {
    CookieAccountant::RecordCookie(url, cookie_data, current_time);
  }

  virtual CookieEventsFunnel& cookie_events_funnel() {
    return mock_cookie_events_funnel_;
  }

  static void set_singleton_instance(CookieAccountant* instance) {
    singleton_instance_ = instance;
  }

  MockCookieEventsFunnel mock_cookie_events_funnel_;
};

class CookieAccountantTest : public testing::Test {
};

TEST_F(CookieAccountantTest, SetCookiePatchFiresCookieEvent) {
  MockWinInet mock_wininet;
  MockCookieAccountant cookie_accountant;
  MockCookieAccountant::set_singleton_instance(&cookie_accountant);

  EXPECT_CALL(mock_wininet, InternetSetCookieExA(_, _, _, _, _)).
      WillOnce(Return(5));
  EXPECT_CALL(cookie_accountant, RecordCookie("foo.com", "foo=bar", _));
  EXPECT_EQ(5, CookieAccountant::InternetSetCookieExAPatch(
      "foo.com", NULL, "foo=bar", 0, NULL));

  EXPECT_CALL(mock_wininet, InternetSetCookieExW(_, _, _, _, _)).
      WillOnce(Return(6));
  EXPECT_CALL(cookie_accountant, RecordCookie("foo.com", "foo=bar", _));
  EXPECT_EQ(6, CookieAccountant::InternetSetCookieExWPatch(
      L"foo.com", NULL, L"foo=bar", 0, NULL));
}

TEST_F(CookieAccountantTest, RecordCookie) {
  testing::LogDisabler no_dchecks;
  MockCookieAccountant cookie_accountant;
  cookie_api::CookieInfo expected_cookie;
  expected_cookie.name = ::SysAllocString(L"FOO");
  expected_cookie.value = ::SysAllocString(L"bar");
  expected_cookie.host_only = TRUE;
  expected_cookie.http_only = TRUE;
  expected_cookie.expiration_date = 1278201600;
  EXPECT_CALL(cookie_accountant.mock_cookie_events_funnel_,
              OnChanged(false, CookiesEqual(&expected_cookie)));
  cookie_accountant.CallRecordCookie(
      "http://www.google.com",
      "FOO=bar; httponly; expires=Sun, 4 Jul 2010 00:00:00 UTC",
      base::Time::Now());

  cookie_api::CookieInfo expected_cookie2;
  expected_cookie2.name = ::SysAllocString(L"");
  expected_cookie2.value = ::SysAllocString(L"helloworld");
  expected_cookie2.domain = ::SysAllocString(L"omg.com");
  expected_cookie2.path = ::SysAllocString(L"/leaping/lizards");
  expected_cookie2.secure = TRUE;
  expected_cookie2.session = TRUE;
  EXPECT_CALL(cookie_accountant.mock_cookie_events_funnel_,
              OnChanged(false, CookiesEqual(&expected_cookie2)));
  cookie_accountant.CallRecordCookie(
      "http://www.omg.com",
      "helloworld; path=/leaping/lizards; secure; domain=omg.com",
      base::Time::Now());
}

TEST_F(CookieAccountantTest, RecordHttpResponseCookies) {
  testing::LogDisabler no_dchecks;
  MockCookieAccountant cookie_accountant;
  EXPECT_CALL(cookie_accountant, RecordCookie("", " foo=bar", _));
  EXPECT_CALL(cookie_accountant, RecordCookie("", "HELLO=world235", _));
  cookie_accountant.RecordHttpResponseCookies(
      "HTTP/1.1 200 OK\nSet-Cookie: foo=bar\nCookie: not_a=cookie\n"
      "Set-Cookie:HELLO=world235", base::Time::Now());
}

}  // namespace
