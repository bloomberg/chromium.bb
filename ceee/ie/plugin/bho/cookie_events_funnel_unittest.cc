// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for CookieEventsFunnel.

#include "ceee/ie/plugin/bho/cookie_events_funnel.h"
#include "chrome/browser/extensions/extension_cookies_api_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::Return;
using testing::StrEq;

MATCHER_P(ValuesEqual, value, "") {
  return arg.Equals(value);
}

class TestCookieEventsFunnel : public CookieEventsFunnel {
 public:
  MOCK_METHOD2(SendEvent, HRESULT(const char*, const Value&));
};

TEST(CookieEventsFunnelTest, OnChanged) {
  TestCookieEventsFunnel cookie_events_funnel;

  bool removed = true;
  cookie_api::CookieInfo cookie_info;
  cookie_info.name = ::SysAllocString(L"FOO");
  cookie_info.value = ::SysAllocString(L"BAR");
  cookie_info.secure = TRUE;
  cookie_info.session = TRUE;
  cookie_info.store_id = ::SysAllocString(L"a store id!!");

  DictionaryValue dict;
  dict.SetBoolean(extension_cookies_api_constants::kRemovedKey, removed);
  DictionaryValue* cookie = new DictionaryValue();
  cookie->SetString(extension_cookies_api_constants::kNameKey, "FOO");
  cookie->SetString(extension_cookies_api_constants::kValueKey, "BAR");
  cookie->SetString(extension_cookies_api_constants::kDomainKey, "");
  cookie->SetBoolean(extension_cookies_api_constants::kHostOnlyKey, false);
  cookie->SetString(extension_cookies_api_constants::kPathKey, "");
  cookie->SetBoolean(extension_cookies_api_constants::kSecureKey, true);
  cookie->SetBoolean(extension_cookies_api_constants::kHttpOnlyKey, false);
  cookie->SetBoolean(extension_cookies_api_constants::kSessionKey, true);
  cookie->SetString(extension_cookies_api_constants::kStoreIdKey,
                    "a store id!!");
  dict.Set(extension_cookies_api_constants::kCookieKey, cookie);

  EXPECT_CALL(cookie_events_funnel, SendEvent(
    StrEq(extension_cookies_api_constants::kOnChanged), ValuesEqual(&dict))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(
      cookie_events_funnel.OnChanged(removed, cookie_info));
}

}  // namespace
