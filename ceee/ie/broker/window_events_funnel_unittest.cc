// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for WindowEventsFunnel.

// MockWin32 can't be included after ChromeFrameHost because of an include
// incompatibility with atlwin.h.
#include "ceee/testing/utils/mock_win32.h"  // NOLINT

#include "base/json/json_writer.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "ceee/ie/broker/chrome_postman.h"
#include "ceee/ie/broker/window_events_funnel.h"
#include "ceee/ie/common/ie_util.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/test_utils.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"


namespace ext_event_names = extension_event_names;
namespace keys = extension_tabs_module_constants;

namespace {

using testing::_;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;

MATCHER_P(ValuesEqual, value, "") {
  return arg.Equals(value);
}

class TestWindowEventsFunnel : public WindowEventsFunnel {
 public:
  MOCK_METHOD2(SendEvent, HRESULT(const char*, const Value&));
  HRESULT CallSendEvent(const char* name, const Value& value) {
    return WindowEventsFunnel::SendEvent(name, value);
  }
};

TEST(WindowEventsFunnelTest, SendEvent) {
  TestWindowEventsFunnel events_funnel;

  static const char* kEventName = "MADness";
  DictionaryValue event_args;
  event_args.SetInteger("Answer to the Ultimate Question of Life,"
                        "the Universe, and Everything", 42);
  event_args.SetString("AYBABTU", "All your base are belong to us");
  event_args.SetDouble("www.unrealtournament.com", 3.0);

  ListValue args_list;
  args_list.Append(event_args.DeepCopy());

  std::string event_args_str;
  base::JSONWriter::Write(&args_list, false, &event_args_str);

  class MockChromePostman : public ChromePostman {
   public:
    MOCK_METHOD2(FireEvent, void(const char*, const char*));
  };
  // Simply instantiating the postman will register it as the
  // one and only singleton to use all the time.
  CComObjectStackEx<testing::StrictMock<MockChromePostman>> postman;
  EXPECT_CALL(postman, FireEvent(
      StrEq(kEventName),
      StrEq(event_args_str.c_str()))).Times(1);
  EXPECT_HRESULT_SUCCEEDED(events_funnel.CallSendEvent(kEventName, event_args));
}

// Mock IeIsInPrivateBrowsing.
MOCK_STATIC_CLASS_BEGIN(MockIeUtil)
  MOCK_STATIC_INIT_BEGIN(MockIeUtil)
    MOCK_STATIC_INIT2(ie_util::GetIEIsInPrivateBrowsing,
                      GetIEIsInPrivateBrowsing);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC0(bool, , GetIEIsInPrivateBrowsing);
MOCK_STATIC_CLASS_END(MockIeUtil)

TEST(WindowEventsFunnelTest, OnWindowCreated) {
  testing::LogDisabler no_dchecks;
  TestWindowEventsFunnel window_events_funnel;

  int window_id = 42;
  HWND window = reinterpret_cast<HWND>(window_id);

  testing::MockUser32 user32;
  EXPECT_CALL(user32, GetWindowRect(window, NotNull()))
      .WillOnce(Return(FALSE));
  EXPECT_CALL(window_events_funnel, SendEvent(_, _)).Times(0);
  // We return the HRESULT conversion of GetLastError.
  ::SetLastError(ERROR_INVALID_ACCESS);
  EXPECT_HRESULT_FAILED(window_events_funnel.OnCreated(window));
  ::SetLastError(ERROR_SUCCESS);

  RECT window_rect = {1, 2, 3, 4};
  EXPECT_CALL(user32, GetWindowRect(window, NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<1>(window_rect), Return(TRUE)));
  EXPECT_CALL(user32, GetForegroundWindow()).WillOnce(Return((HWND)NULL));
  testing::MockWindowUtils mock_windows_utils;
  EXPECT_CALL(mock_windows_utils, GetTopLevelParent((HWND)NULL))
      .WillOnce(Return(window));

  StrictMock<MockIeUtil> mock_ie_util;
  EXPECT_CALL(mock_ie_util, GetIEIsInPrivateBrowsing()).WillRepeatedly(
      Return(false));

  scoped_ptr<DictionaryValue> dict(new DictionaryValue());
  dict->SetInteger(keys::kIdKey, window_id);
  dict->SetBoolean(keys::kFocusedKey, true);
  dict->SetInteger(keys::kLeftKey, window_rect.left);
  dict->SetInteger(keys::kTopKey, window_rect.top);
  dict->SetInteger(keys::kWidthKey, window_rect.right - window_rect.left);
  dict->SetInteger(keys::kHeightKey, window_rect.bottom - window_rect.top);
  dict->SetBoolean(keys::kIncognitoKey, false);
  // TODO(mad@chromium.org): for now, always setting to "normal" since
  // we are not handling popups or app windows in IE yet.
  dict->SetString(keys::kWindowTypeKey, keys::kWindowTypeValueNormal);

  EXPECT_CALL(window_events_funnel, SendEvent(StrEq(
      ext_event_names::kOnWindowCreated), ValuesEqual(dict.get()))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(window_events_funnel.OnCreated(window));
}

TEST(WindowEventsFunnelTest, OnFocusChanged) {
  TestWindowEventsFunnel window_events_funnel;
  int window_id = 42;

  scoped_ptr<Value> args(Value::CreateIntegerValue(window_id));
  EXPECT_CALL(window_events_funnel, SendEvent(StrEq(
      ext_event_names::kOnWindowFocusedChanged), ValuesEqual(args.get()))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(window_events_funnel.OnFocusChanged(window_id));
}

TEST(WindowEventsFunnelTest, OnWindowRemoved) {
  TestWindowEventsFunnel window_events_funnel;
  int window_id = 42;
  HWND window = reinterpret_cast<HWND>(window_id);

  scoped_ptr<Value> args(Value::CreateIntegerValue(window_id));
  EXPECT_CALL(window_events_funnel, SendEvent(
      StrEq(ext_event_names::kOnWindowRemoved), ValuesEqual(args.get()))).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(window_events_funnel.OnRemoved(window));
}

}  // namespace
