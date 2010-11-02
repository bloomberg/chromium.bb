// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for TabEventsFunnel.

#include <atlcomcli.h>

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "ceee/ie/plugin/bho/tab_events_funnel.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"


namespace ext_event_names = extension_event_names;
namespace keys = extension_tabs_module_constants;

namespace {

using testing::Return;
using testing::StrEq;

MATCHER_P(ValuesEqual, value, "") {
  return arg.Equals(value);
}

class TestTabEventsFunnel : public TabEventsFunnel {
 public:
  MOCK_METHOD2(SendEvent, HRESULT(const char*, const Value&));
};

TEST(TabEventsFunnelTest, OnTabCreated) {
  TestTabEventsFunnel tab_events_funnel;
  int tab_id = 42;
  HWND tab_handle = reinterpret_cast<HWND>(tab_id);
  std::string url("http://www.google.com");
  std::string status(keys::kStatusValueComplete);

  DictionaryValue dict;
  dict.SetInteger(keys::kIdKey, tab_id);
  dict.SetString(keys::kUrlKey, url);
  dict.SetString(keys::kStatusKey, status);

  EXPECT_CALL(tab_events_funnel, SendEvent(
      StrEq(ext_event_names::kOnTabCreated), ValuesEqual(&dict))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(tab_events_funnel.OnCreated(tab_handle,
                                                       CComBSTR(url.c_str()),
                                                       true));
}

TEST(TabEventsFunnelTest, OnTabMoved) {
  TestTabEventsFunnel tab_events_funnel;

  int tab_id = 42;
  HWND tab_handle = reinterpret_cast<HWND>(tab_id);
  int window_id = 24;
  int from_index = 12;
  int to_index = 21;

  ListValue tab_moved_args;
  tab_moved_args.Append(Value::CreateIntegerValue(tab_id));

  DictionaryValue* dict = new DictionaryValue;
  dict->SetInteger(keys::kWindowIdKey, window_id);
  dict->SetInteger(keys::kFromIndexKey, from_index);
  dict->SetInteger(keys::kFromIndexKey, to_index);
  tab_moved_args.Append(dict);

  EXPECT_CALL(tab_events_funnel, SendEvent(StrEq(ext_event_names::kOnTabMoved),
      ValuesEqual(&tab_moved_args))).WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(tab_events_funnel.OnMoved(tab_handle, window_id,
                                                     from_index, to_index));
}

TEST(TabEventsFunnelTest, OnTabRemoved) {
  TestTabEventsFunnel tab_events_funnel;

  int tab_id = 42;
  HWND tab_handle = reinterpret_cast<HWND>(tab_id);
  scoped_ptr<Value> args(Value::CreateIntegerValue(tab_id));

  EXPECT_CALL(tab_events_funnel, SendEvent(
      StrEq(ext_event_names::kOnTabRemoved), ValuesEqual(args.get()))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(tab_events_funnel.OnRemoved(tab_handle));
}

TEST(TabEventsFunnelTest, OnTabSelectionChanged) {
  TestTabEventsFunnel tab_events_funnel;

  int tab_id = 42;
  HWND tab_handle = reinterpret_cast<HWND>(tab_id);
  int window_id = 24;

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));
  DictionaryValue* dict = new DictionaryValue;
  dict->SetInteger(keys::kWindowIdKey, window_id);
  args.Append(dict);

  EXPECT_CALL(tab_events_funnel, SendEvent(
      StrEq(ext_event_names::kOnTabSelectionChanged), ValuesEqual(&args))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(tab_events_funnel.OnSelectionChanged(tab_handle,
                                                                window_id));
}

TEST(TabEventsFunnelTest, OnTabUpdated) {
  TestTabEventsFunnel tab_events_funnel;

  int tab_id = 24;
  HWND tab_handle = reinterpret_cast<HWND>(tab_id);
  READYSTATE ready_state = READYSTATE_INTERACTIVE;

  ListValue args;
  args.Append(Value::CreateIntegerValue(tab_id));
  DictionaryValue* dict = new DictionaryValue;
  dict->SetString(keys::kStatusKey, keys::kStatusValueLoading);
  args.Append(dict);

  // Without a URL.
  EXPECT_CALL(tab_events_funnel, SendEvent(
      StrEq(ext_event_names::kOnTabUpdated), ValuesEqual(&args))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(tab_events_funnel.OnUpdated(tab_handle, NULL,
                                                       ready_state));
  // With a URL.
  CComBSTR url(L"http://imbored.com");
  dict->SetString(keys::kUrlKey, url.m_str);
  EXPECT_CALL(tab_events_funnel, SendEvent(
      StrEq(ext_event_names::kOnTabUpdated), ValuesEqual(&args))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(tab_events_funnel.OnUpdated(tab_handle, url,
                                                       ready_state));
}

}  // namespace
