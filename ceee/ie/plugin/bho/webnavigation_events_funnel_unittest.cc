// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for WebNavigationEventsFunnel.

#include <atlcomcli.h>

#include "base/time.h"
#include "base/values.h"
#include "ceee/ie/plugin/bho/webnavigation_events_funnel.h"
#include "chrome/browser/extensions/extension_webnavigation_api_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"


namespace keys = extension_webnavigation_api_constants;

namespace {

using testing::Return;
using testing::StrEq;

MATCHER_P(ValuesEqual, value, "") {
  return arg.Equals(value);
}

class TestWebNavigationEventsFunnel : public WebNavigationEventsFunnel {
 public:
  TestWebNavigationEventsFunnel() {}
  MOCK_METHOD2(SendEvent, HRESULT(const char*, const Value&));
};

TEST(WebNavigationEventsFunnelTest, OnBeforeNavigate) {
  TestWebNavigationEventsFunnel webnavigation_events_funnel;

  int tab_handle = 256;
  std::string url("http://www.google.com/");
  int frame_id = 512;
  int request_id = 1024;
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, tab_handle);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetInteger(keys::kRequestIdKey, request_id);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webnavigation_events_funnel,
              SendEvent(StrEq(keys::kOnBeforeNavigate), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webnavigation_events_funnel.OnBeforeNavigate(
      static_cast<CeeeWindowHandle>(tab_handle), CComBSTR(url.c_str()),
      frame_id, request_id, time_stamp));
}

TEST(WebNavigationEventsFunnelTest, OnBeforeRetarget) {
  TestWebNavigationEventsFunnel webnavigation_events_funnel;

  int source_tab_handle = 256;
  std::string source_url("http://docs.google.com/");
  std::string target_url("http://calendar.google.com/");
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kSourceTabIdKey, source_tab_handle);
  args.SetString(keys::kSourceUrlKey, source_url);
  args.SetString(keys::kTargetUrlKey, target_url);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webnavigation_events_funnel,
              SendEvent(StrEq(keys::kOnBeforeRetarget), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webnavigation_events_funnel.OnBeforeRetarget(
      static_cast<CeeeWindowHandle>(source_tab_handle),
      CComBSTR(source_url.c_str()), CComBSTR(target_url.c_str()), time_stamp));
}

TEST(WebNavigationEventsFunnelTest, OnCommitted) {
  TestWebNavigationEventsFunnel webnavigation_events_funnel;

  int tab_handle = 256;
  std::string url("http://mail.google.com/");
  int frame_id = 512;
  std::string transition_type("link");
  std::string transition_qualifiers("client_redirect");
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, tab_handle);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetString(keys::kTransitionTypeKey, transition_type);
  args.SetString(keys::kTransitionQualifiersKey, transition_qualifiers);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webnavigation_events_funnel,
              SendEvent(StrEq(keys::kOnCommitted), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webnavigation_events_funnel.OnCommitted(
      static_cast<CeeeWindowHandle>(tab_handle), CComBSTR(url.c_str()),
      frame_id, transition_type.c_str(), transition_qualifiers.c_str(),
      time_stamp));
}

TEST(WebNavigationEventsFunnelTest, OnCompleted) {
  TestWebNavigationEventsFunnel webnavigation_events_funnel;

  int tab_handle = 256;
  std::string url("http://groups.google.com/");
  int frame_id = 512;
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, tab_handle);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webnavigation_events_funnel,
              SendEvent(StrEq(keys::kOnCompleted), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webnavigation_events_funnel.OnCompleted(
      static_cast<CeeeWindowHandle>(tab_handle), CComBSTR(url.c_str()),
      frame_id, time_stamp));
}

TEST(WebNavigationEventsFunnelTest, OnDOMContentLoaded) {
  TestWebNavigationEventsFunnel webnavigation_events_funnel;

  int tab_handle = 256;
  std::string url("http://mail.google.com/");
  int frame_id = 512;
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, tab_handle);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webnavigation_events_funnel,
              SendEvent(StrEq(keys::kOnDOMContentLoaded), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webnavigation_events_funnel.OnDOMContentLoaded(
      static_cast<CeeeWindowHandle>(tab_handle), CComBSTR(url.c_str()),
      frame_id, time_stamp));
}

TEST(WebNavigationEventsFunnelTest, OnErrorOccurred) {
  TestWebNavigationEventsFunnel webnavigation_events_funnel;

  int tab_handle = 256;
  std::string url("http://mail.google.com/");
  int frame_id = 512;
  std::string error("not a valid URL");
  base::Time time_stamp = base::Time::FromDoubleT(2048.0);

  DictionaryValue args;
  args.SetInteger(keys::kTabIdKey, tab_handle);
  args.SetString(keys::kUrlKey, url);
  args.SetInteger(keys::kFrameIdKey, frame_id);
  args.SetString(keys::kErrorKey, error);
  args.SetDouble(keys::kTimeStampKey,
                 base::Time::kMillisecondsPerSecond * time_stamp.ToDoubleT());

  EXPECT_CALL(webnavigation_events_funnel,
              SendEvent(StrEq(keys::kOnErrorOccurred), ValuesEqual(&args)))
      .WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(webnavigation_events_funnel.OnErrorOccurred(
      static_cast<CeeeWindowHandle>(tab_handle), CComBSTR(url.c_str()),
      frame_id, CComBSTR(error.c_str()), time_stamp));
}

}  // namespace
