// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Window API implementation unit tests.

// MockWin32 can't be included after ChromeFrameHost because of an include
// incompatibility with atlwin.h.
#include "ceee/testing/utils/mock_win32.h"  // NOLINT

#include <iepmapi.h>
#include <set>

#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/ie/broker/chrome_postman.h"
#include "ceee/ie/broker/window_api_module.h"
#include "ceee/ie/common/ie_util.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/test_utils.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::AddRef;
using testing::AnyNumber;
using testing::AtLeast;
using testing::CopyStringToArgument;
using testing::DoAll;
using testing::Gt;
using testing::InstanceCountMixin;
using testing::Invoke;
using testing::MockApiDispatcher;
using testing::MockApiInvocation;
using testing::NotNull;
using testing::Return;
using testing::Sequence;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;

using window_api::WindowApiResult;
using window_api::IterativeWindowApiResult;

namespace keys = extension_tabs_module_constants;
namespace ext_event_names = extension_event_names;

const wchar_t kClassName[] = L"IEFrame";
const int kRequestId = 12;
const int kWindowId = 23;
const HWND kWindowHwnd = (HWND)34;
const int kRandomWindowId = 45;
const HWND kRandomWindowHwnd = (HWND)56;

TEST(WindowApi, RegisterInvocations) {
  StrictMock<MockApiDispatcher> disp;
  EXPECT_CALL(disp, RegisterInvocation(NotNull(), NotNull())).Times(AtLeast(7));
  window_api::RegisterInvocations(&disp);
}

TEST(WindowApi, IsIeFrameClassWithNull) {
  testing::LogDisabler no_dchecks;
  WindowApiResult invocation_result(WindowApiResult::kNoRequestId);
  EXPECT_FALSE(invocation_result.IsIeFrameClass(NULL));
}

TEST(WindowApi, IsIeFrameClassWithNonWindow) {
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, IsWindow(NotNull())).WillRepeatedly(Return(FALSE));
  WindowApiResult invocation_result(WindowApiResult::kNoRequestId);
  EXPECT_FALSE(invocation_result.IsIeFrameClass(reinterpret_cast<HWND>(1)));
}

TEST(WindowApi, IsIeFrameClassWrongClassName) {
  const wchar_t kClassName[] = L"IEFrames";  // note 's', making it invalid.
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, IsWindow(NotNull())).WillOnce(Return(TRUE));
  EXPECT_CALL(user32, GetClassName(NotNull(), NotNull(), Gt(0))).WillOnce(
      DoAll(CopyStringToArgument<1>(kClassName),
            Return(arraysize(kClassName))));
  WindowApiResult invocation_result(WindowApiResult::kNoRequestId);
  EXPECT_FALSE(invocation_result.IsIeFrameClass(reinterpret_cast<HWND>(1)));
}

TEST(WindowApi, IsIeFrameClassStraightline) {
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, IsWindow(NotNull())).WillOnce(Return(TRUE));
  EXPECT_CALL(user32, GetClassName(NotNull(), NotNull(), Gt(0))).WillOnce(
      DoAll(CopyStringToArgument<1>(kClassName),
            Return(arraysize(kClassName))));
  WindowApiResult invocation_result(WindowApiResult::kNoRequestId);
  EXPECT_TRUE(invocation_result.IsIeFrameClass(reinterpret_cast<HWND>(1)));
}

TEST(WindowApi, TopIeWindowNeverFound) {
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, FindDescendentWindow(_, _, _, NotNull())).
      WillOnce(Return(false));
  EXPECT_EQ(NULL, WindowApiResult::TopIeWindow());
}

TEST(WindowApi, TopIeWindowStraightline) {
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, FindDescendentWindow(_, _, _, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<3>(kRandomWindowHwnd), Return(true)));
  EXPECT_EQ(kRandomWindowHwnd, WindowApiResult::TopIeWindow());
}

class MockWindowApiResult
    : public WindowApiResult,
      public InstanceCountMixin<MockWindowApiResult> {
 public:
  explicit MockWindowApiResult(int request_id)
      : WindowApiResult(request_id) {
  }
  MOCK_METHOD1(CreateTabList, Value*(BSTR));
  MOCK_METHOD3(SetResultFromWindowInfo, void(HWND, const CeeeWindowInfo&,
      bool));
  MOCK_METHOD2(UpdateWindowRect, bool(HWND, const DictionaryValue*));
  MOCK_METHOD2(CreateWindowValue, bool(HWND, bool));
  MOCK_METHOD0(PostResult, void());
  MOCK_METHOD1(PostError, void(const std::string&));

  virtual ApiDispatcher* GetDispatcher() {
    return &mock_api_dispatcher_;
  }

  void CallSetResultFromWindowInfo(HWND window, const CeeeWindowInfo& info,
                                   bool pop) {
    WindowApiResult::SetResultFromWindowInfo(window, info, pop);
  }
  bool CallCreateWindowValue(HWND window, bool pop) {
    return WindowApiResult::CreateWindowValue(window, pop);
  }
  Value* CallCreateTabList(BSTR tabs) {
    return WindowApiResult::CreateTabList(tabs);
  }
  bool CallUpdateWindowRect(HWND window, const DictionaryValue* dict_value) {
    return WindowApiResult::UpdateWindowRect(window, dict_value);
  }
  StrictMock<MockApiDispatcher> mock_api_dispatcher_;
};

class MockIterativeWindowApiResult
    : public IterativeWindowApiResult,
      public InstanceCountMixin<MockIterativeWindowApiResult> {
 public:
  explicit MockIterativeWindowApiResult(int request_id)
      : IterativeWindowApiResult(request_id) {}

  MOCK_METHOD0(CallRealPostResult, void());
  MOCK_METHOD1(CallRealPostError, void(const std::string&));
  MOCK_METHOD1(CreateTabList, Value*(BSTR));
  MOCK_METHOD3(SetResultFromWindowInfo, void(HWND, const CeeeWindowInfo&,
      bool));
  MOCK_METHOD2(UpdateWindowRect, bool(HWND, const DictionaryValue*));
  MOCK_METHOD2(CreateWindowValue, bool(HWND, bool));

  virtual void PostError(const std::string& error) {
    ++error_counter_;
    IterativeWindowApiResult::PostError(error);
  }

  virtual void PostResult() {
    ++success_counter_;
    IterativeWindowApiResult::PostResult();
  }

  virtual ApiDispatcher* GetDispatcher() {
    return &mock_api_dispatcher_;
  }

  static void ResetCounters() {
    success_counter_ = 0;
    error_counter_ = 0;
  }

  static int success_counter() {
    return success_counter_;
  }

  static int error_counter() {
    return error_counter_;
  }

  StrictMock<MockApiDispatcher> mock_api_dispatcher_;

 private:
  static int success_counter_;
  static int error_counter_;
};

int MockIterativeWindowApiResult::success_counter_ = 0;
int MockIterativeWindowApiResult::error_counter_ = 0;

// Mock static functions defined in WindowApiResult.
MOCK_STATIC_CLASS_BEGIN(MockWindowApiResultStatics)
  MOCK_STATIC_INIT_BEGIN(MockWindowApiResultStatics)
    MOCK_STATIC_INIT2(WindowApiResult::IsTabWindowClass,
                      IsTabWindowClass);
    MOCK_STATIC_INIT2(WindowApiResult::IsIeFrameClass,
                      IsIeFrameClass);
    MOCK_STATIC_INIT2(WindowApiResult::TopIeWindow,
                      TopIeWindow);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC1(bool, , IsTabWindowClass, HWND);
  MOCK_STATIC1(bool, , IsIeFrameClass, HWND);
  MOCK_STATIC0(HWND, , TopIeWindow);
MOCK_STATIC_CLASS_END(MockWindowApiResultStatics)

class WindowApiTests: public testing::Test {
 public:
  virtual void SetUp() {
    EXPECT_HRESULT_SUCCEEDED(testing::MockWindowExecutor::CreateInitialized(
        &mock_window_executor_, mock_window_executor_keeper_.Receive()));
    EXPECT_HRESULT_SUCCEEDED(testing::MockTabExecutor::CreateInitialized(
        &mock_tab_executor_, mock_tab_executor_keeper_.Receive()));
  }

  virtual void TearDown() {
    // Everything should have been relinquished.
    mock_window_executor_keeper_.Release();
    mock_tab_executor_keeper_.Release();
    ASSERT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
  }
 protected:
  void MockGetWindowExecutor(MockApiDispatcher* executor_owner) {
    EXPECT_CALL(*executor_owner, GetExecutor(_, _, NotNull())).
        WillRepeatedly(DoAll(
            SetArgumentPointee<2>(mock_window_executor_keeper_.get()),
            AddRef(mock_window_executor_keeper_.get())));
  }

  void MockGetTabExecutor(MockApiDispatcher* executor_owner) {
    EXPECT_CALL(*executor_owner, GetExecutor(_, _, NotNull())).
        WillRepeatedly(DoAll(
            SetArgumentPointee<2>(mock_tab_executor_keeper_.get()),
            AddRef(mock_tab_executor_keeper_.get())));
  }

  void WindowAlwaysHasThread() {
    EXPECT_CALL(user32_, GetWindowThreadProcessId(_, _)).WillRepeatedly(
        Return(1));  // We only need to return a non-0 value.
  }

  void MockIsIeFrameClass() {
    // This is to mock the static call to IsIeFrameClass.
    EXPECT_CALL(user32_, IsWindow(NotNull())).WillRepeatedly(Return(TRUE));
    EXPECT_CALL(user32_, GetClassName(NotNull(), NotNull(), Gt(0))).
        WillRepeatedly(DoAll(CopyStringToArgument<1>(kClassName),
                             Return(arraysize(kClassName))));
  }
  StrictMock<testing::MockUser32> user32_;
  // The executor classes are already strict from their base class impl.
  testing::MockWindowExecutor* mock_window_executor_;
  testing::MockTabExecutor* mock_tab_executor_;
  // To control the life span of the executors.
  ScopedComPtr<ICeeeWindowExecutor> mock_window_executor_keeper_;
  ScopedComPtr<ICeeeTabExecutor> mock_tab_executor_keeper_;

 private:
  class MockChromePostman : public ChromePostman {
   public:
    MOCK_METHOD2(PostMessage, void(BSTR, BSTR));
  };
  // We should never get to the postman, we mock all the calls getting there.
  // So we simply instantiate it strict and it will register itself as the
  // one and only singleton to use all the time.
  CComObjectStackEx<StrictMock<MockChromePostman>> postman_;
};

TEST_F(WindowApiTests, CreateTabList) {
  // We only check that we can properly handle an empty list since we can't
  // easily mock the tab_api::GetAllTabsInWindow declared on the stack.
  // We validate it more completely in the tabs api unittest anyway.
  WindowApiResult invocation_result(WindowApiResult::kNoRequestId);
  scoped_ptr<Value> returned_value(invocation_result.CreateTabList(L"[]"));
  EXPECT_NE(static_cast<Value*>(NULL), returned_value.get());
  EXPECT_TRUE(returned_value->IsType(Value::TYPE_LIST));
  EXPECT_EQ(0, static_cast<ListValue*>(returned_value.get())->GetSize());

  // Also test the failure path.
  testing::LogDisabler no_dchecks;
  returned_value.reset(invocation_result.CreateTabList(L""));
  EXPECT_EQ(NULL, returned_value.get());
}

// Mock IeIsInPrivateBrowsing.
MOCK_STATIC_CLASS_BEGIN(MockIeUtil)
  MOCK_STATIC_INIT_BEGIN(MockIeUtil)
    MOCK_STATIC_INIT2(ie_util::GetIEIsInPrivateBrowsing,
                      GetIEIsInPrivateBrowsing);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC0(bool, , GetIEIsInPrivateBrowsing);
MOCK_STATIC_CLASS_END(MockIeUtil)

TEST_F(WindowApiTests, SetResultFromWindowInfo) {
  testing::LogDisabler no_dchecks;

  // Standard test without tabs list populate nor an internal list.
  StrictMock<MockWindowApiResult> invocation_result(
      WindowApiResult::kNoRequestId);
  CeeeWindowInfo window_info;
  window_info.rect.left = 1;
  window_info.rect.right = 2;
  window_info.rect.top = 3;
  window_info.rect.bottom = 4;
  window_info.tab_list = NULL;

  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowIdFromHandle(kWindowHwnd)).WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowHandleFromId(kWindowId)).WillRepeatedly(Return(kWindowHwnd));

  StrictMock<MockIeUtil> mock_ie_util;
  EXPECT_CALL(mock_ie_util, GetIEIsInPrivateBrowsing()).WillRepeatedly(
    Return(false));

  invocation_result.CallSetResultFromWindowInfo(kWindowHwnd, window_info,
      false);
  const Value * result = invocation_result.value();
  EXPECT_NE(static_cast<Value*>(NULL), result);
  EXPECT_TRUE(result->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue dict;
  dict.SetInteger(keys::kIdKey, kWindowId);
  dict.SetBoolean(keys::kFocusedKey, window_info.focused != FALSE);

  dict.SetInteger(keys::kLeftKey, window_info.rect.left);
  dict.SetInteger(keys::kTopKey, window_info.rect.top);
  dict.SetInteger(keys::kWidthKey, window_info.rect.right -
                                   window_info.rect.left);
  dict.SetInteger(keys::kHeightKey, window_info.rect.bottom -
                                    window_info.rect.top);

  dict.SetBoolean(keys::kIncognitoKey, false);

  // TODO(mad@chromium.org): for now, always setting to "normal" since
  // we are not handling popups or app windows in IE yet.
  dict.SetString(keys::kWindowTypeKey, keys::kWindowTypeValueNormal);
  EXPECT_TRUE(dict.Equals(result));

  invocation_result.set_value(NULL);

  // Now make sure we call CreateTablist, yet still succeed if it returns NULL.
  int tab_list = 42;
  EXPECT_CALL(invocation_result, CreateTabList((BSTR)tab_list)).
      WillOnce(Return(static_cast<Value*>(NULL)));
  window_info.tab_list = (BSTR)tab_list;

  invocation_result.CallSetResultFromWindowInfo(kWindowHwnd, window_info, true);
  EXPECT_NE(static_cast<Value*>(NULL), invocation_result.value());

  invocation_result.set_value(NULL);

  // And now a successful run with CreateTablist.
  EXPECT_CALL(invocation_result, CreateTabList((BSTR)tab_list)).
      WillOnce(Return(Value::CreateIntegerValue(tab_list)));

  invocation_result.CallSetResultFromWindowInfo(kWindowHwnd, window_info, true);
  result = invocation_result.value();
  EXPECT_NE(static_cast<Value*>(NULL), result);
  EXPECT_TRUE(result->IsType(Value::TYPE_DICTIONARY));
  dict.Set(keys::kTabsKey, Value::CreateIntegerValue(tab_list));
  EXPECT_TRUE(dict.Equals(result));
}

TEST_F(WindowApiTests, CreateWindowValue) {
  testing::LogDisabler no_dchecks;
  EXPECT_CALL(user32_, IsWindow(_)).WillRepeatedly(Return(FALSE));
  EXPECT_CALL(user32_, GetWindowThreadProcessId(_, _)).
      WillRepeatedly(Return(0));

  StrictMock<MockWindowApiResult> invocation_result(
      WindowApiResult::kNoRequestId);
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowIdFromHandle(kWindowHwnd)).WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowHandleFromId(kWindowId)).WillRepeatedly(Return(kWindowHwnd));

  // Fail because the window is not associated to a thread.
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateWindowValue(kWindowHwnd, false));
  EXPECT_EQ(NULL, invocation_result.value());

  // Now fail because the window is not an IE Frame.
  WindowAlwaysHasThread();
  invocation_result.set_value(NULL);
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateWindowValue(kWindowHwnd, false));
  EXPECT_EQ(NULL, invocation_result.value());

  // Now fail because we can't get an executor.
  MockIsIeFrameClass();
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
              GetExecutor(kWindowHwnd, _, NotNull())).
      WillOnce(SetArgumentPointee<2>((HANDLE)NULL));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateWindowValue(kWindowHwnd, false));
  EXPECT_EQ(NULL, invocation_result.value());

  // Now get the executor to fail.
  MockGetWindowExecutor(&invocation_result.mock_api_dispatcher_);
  EXPECT_CALL(*mock_window_executor_, GetWindow(FALSE, NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateWindowValue(kWindowHwnd, false));
  EXPECT_EQ(NULL, invocation_result.value());

  // Now go all the way.
  EXPECT_CALL(*mock_window_executor_, GetWindow(FALSE, NotNull())).
      WillOnce(Return(S_OK));
  EXPECT_CALL(invocation_result, SetResultFromWindowInfo(_, _, _)).Times(1);
  EXPECT_TRUE(invocation_result.CallCreateWindowValue(kWindowHwnd, false));
}

TEST_F(WindowApiTests, UpdateWindowRect) {
  testing::LogDisabler no_dchecks;
  EXPECT_CALL(user32_, IsWindow(_)).WillRepeatedly(Return(FALSE));

  StrictMock<MockWindowApiResult> invocation_result(
      WindowApiResult::kNoRequestId);
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowIdFromHandle(kWindowHwnd)).WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowHandleFromId(kWindowId)).WillRepeatedly(Return(kWindowHwnd));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowIdFromHandle(kRandomWindowHwnd)).
          WillRepeatedly(Return(kRandomWindowId));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowHandleFromId(kRandomWindowId)).
          WillRepeatedly(Return(kRandomWindowHwnd));

  // Window has no thread.
  EXPECT_CALL(user32_, GetWindowThreadProcessId(_, _)).WillRepeatedly(
              Return(0));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd, NULL));
  EXPECT_EQ(NULL, invocation_result.value());

  // Window is not an IEFrame.
  WindowAlwaysHasThread();
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd, NULL));
  EXPECT_EQ(NULL, invocation_result.value());

  // Now try an empty dictionary (which should not cause an error).
  MockIsIeFrameClass();
  EXPECT_CALL(invocation_result, CreateWindowValue(_, false)).
      WillOnce(Return(true));
  EXPECT_TRUE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd, NULL));

  // Now try a dictionary with invalid dictionary values.
  DictionaryValue bad_values;
  bad_values.Set(keys::kLeftKey, Value::CreateNullValue());
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd,
                                                      &bad_values));
  EXPECT_EQ(NULL, invocation_result.value());

  bad_values.Set(keys::kLeftKey, Value::CreateIntegerValue(43));
  bad_values.Set(keys::kTopKey, Value::CreateNullValue());
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd,
                                                      &bad_values));
  EXPECT_EQ(NULL, invocation_result.value());

  bad_values.Set(keys::kTopKey, Value::CreateIntegerValue(44));
  bad_values.Set(keys::kWidthKey, Value::CreateNullValue());
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd,
                                                      &bad_values));
  EXPECT_EQ(NULL, invocation_result.value());

  bad_values.Set(keys::kWidthKey, Value::CreateIntegerValue(45));
  bad_values.Set(keys::kHeightKey, Value::CreateNullValue());
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd,
                                                      &bad_values));
  EXPECT_EQ(NULL, invocation_result.value());

  // Now make sure the values get properly propagated.
  // But start by failing the GetExecutor.
  scoped_ptr<DictionaryValue> good_values(static_cast<DictionaryValue*>(
      bad_values.DeepCopy()));
  good_values->Set(keys::kHeightKey, Value::CreateIntegerValue(46));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
              GetExecutor(kRandomWindowHwnd, _, NotNull())).
      WillOnce(SetArgumentPointee<2>((HANDLE)NULL));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd,
                                                      good_values.get()));
  EXPECT_EQ(NULL, invocation_result.value());

  // Now get the executor to fail.
  MockGetWindowExecutor(&invocation_result.mock_api_dispatcher_);
  EXPECT_CALL(*mock_window_executor_, UpdateWindow(43, 44, 45, 46, _)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd,
                                                      good_values.get()));
  EXPECT_EQ(NULL, invocation_result.value());

  // Now go all the way,
  EXPECT_CALL(invocation_result, SetResultFromWindowInfo(_, _, _)).Times(1);
  EXPECT_CALL(*mock_window_executor_, UpdateWindow(43, 44, 45, 46, _)).
      WillOnce(Return(S_OK));
  EXPECT_TRUE(invocation_result.CallUpdateWindowRect(kRandomWindowHwnd,
                                                     good_values.get()));
}

template <class BaseClass>
class MockWindowInvocation
    : public MockApiInvocation<WindowApiResult, MockWindowApiResult,
                               BaseClass> {
};

template <class BaseClass>
class MockIterativeWindowInvocation
    : public MockApiInvocation<IterativeWindowApiResult,
                               MockIterativeWindowApiResult,
                               BaseClass> {
};

TEST_F(WindowApiTests, GetWindowErrorHandling) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockWindowInvocation<window_api::GetWindow>> invocation;
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(user32_, IsWindow(_)).WillRepeatedly(Return(FALSE));

  // Bad args failure.
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId);
}

TEST_F(WindowApiTests, GetWindowStraightline) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockWindowInvocation<window_api::GetWindow>> invocation;
  MockIsIeFrameClass();
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, CreateWindowValue(
      kRandomWindowHwnd, _)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetWindowHandleFromId(kRandomWindowId)).
          WillOnce(Return(kRandomWindowHwnd));
  ListValue args_list;
  args_list.Append(Value::CreateIntegerValue(kRandomWindowId));
  invocation.Execute(args_list, kRequestId);
}

TEST_F(WindowApiTests, GetCurrentWindowErrorHandling) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockWindowInvocation<window_api::GetCurrentWindow>> invocation;
  StrictMock<testing::MockWindowUtils> window_utils;
  MockWindowApiResultStatics result_statics;
  DictionaryValue associated_tab;

  EXPECT_CALL(user32_, IsWindow(_)).WillRepeatedly(Return(FALSE));

  // Bad args failure.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId, &associated_tab);

  associated_tab.SetInteger(keys::kIdKey, 3);

  // Bad window failures.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(invocation.mock_api_dispatcher_, GetTabHandleFromToolBandId(3)).
      WillOnce(Return(kRandomWindowHwnd));
  EXPECT_CALL(window_utils, GetTopLevelParent(kRandomWindowHwnd)).
      WillOnce(Return(kRandomWindowHwnd));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabIdFromHandle(kRandomWindowHwnd)).
          WillOnce(Return(kRandomWindowId));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId, &associated_tab);

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(invocation.mock_api_dispatcher_, GetTabHandleFromToolBandId(3)).
      WillOnce(Return(kRandomWindowHwnd));
  EXPECT_CALL(window_utils, GetTopLevelParent(kRandomWindowHwnd)).
      WillOnce(Return(kWindowHwnd));
  EXPECT_CALL(result_statics, IsTabWindowClass(kRandomWindowHwnd)).
      WillOnce(Return(FALSE));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabIdFromHandle(kRandomWindowHwnd)).
          WillOnce(Return(kRandomWindowId));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId, &associated_tab);

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(invocation.mock_api_dispatcher_, GetTabHandleFromToolBandId(3)).
      WillOnce(Return(kRandomWindowHwnd));
  EXPECT_CALL(window_utils, GetTopLevelParent(kRandomWindowHwnd)).
      WillOnce(Return(kWindowHwnd));
  EXPECT_CALL(result_statics, IsTabWindowClass(kRandomWindowHwnd)).
      WillOnce(Return(TRUE));
  EXPECT_CALL(result_statics, IsIeFrameClass(kWindowHwnd)).
      WillOnce(Return(FALSE));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabIdFromHandle(kRandomWindowHwnd)).
          WillOnce(Return(kRandomWindowId));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId, &associated_tab);
}

TEST_F(WindowApiTests, GetCurrentWindowStraightline) {
  testing::LogDisabler no_dchecks;

  StrictMock<MockWindowInvocation<window_api::GetCurrentWindow>> invocation;
  MockWindowApiResultStatics result_statics;
  invocation.AllocateNewResult(kRequestId);

  EXPECT_CALL(invocation.mock_api_dispatcher_, GetTabHandleFromToolBandId(3)).
      WillOnce(Return(kRandomWindowHwnd));
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, GetTopLevelParent(kRandomWindowHwnd)).
      WillOnce(Return(kWindowHwnd));
  EXPECT_CALL(result_statics, IsTabWindowClass(kRandomWindowHwnd)).
      WillOnce(Return(TRUE));
  EXPECT_CALL(result_statics, IsIeFrameClass(kWindowHwnd)).
      WillOnce(Return(TRUE));
  EXPECT_CALL(*invocation.invocation_result_,
      CreateWindowValue(kWindowHwnd, false)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  DictionaryValue associated_tab;
  associated_tab.SetInteger(keys::kIdKey, 3);
  invocation.Execute(ListValue(), kRequestId, &associated_tab);
}

TEST_F(WindowApiTests, GetCurrentWindowNoContext) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockWindowInvocation<window_api::GetCurrentWindow>> invocation;
  MockWindowApiResultStatics result_statics;

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(result_statics, TopIeWindow()).WillOnce(Return(
      kRandomWindowHwnd));
  EXPECT_CALL(*invocation.invocation_result_, CreateWindowValue(
      kRandomWindowHwnd, _)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  invocation.Execute(ListValue(), kRequestId, NULL);
}

TEST_F(WindowApiTests, GetLastFocusedWindowStraightline) {
  testing::LogDisabler no_dchecks;  // don't care about NULL pointers.

  EXPECT_CALL(user32_, IsWindow(_)).WillRepeatedly(Return(TRUE));

  StrictMock<MockWindowInvocation<window_api::GetLastFocusedWindow>> invocation;
  invocation.AllocateNewResult(kRequestId);
  MockWindowApiResultStatics result_statics;
  EXPECT_CALL(result_statics, TopIeWindow()).WillOnce(Return(
      kRandomWindowHwnd));
  EXPECT_CALL(*invocation.invocation_result_, CreateWindowValue(
      kRandomWindowHwnd, _)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  invocation.Execute(ListValue(), kRequestId);
}

// Mock the CoCreateInstance call that is used to create a new IE window.
MOCK_STATIC_CLASS_BEGIN(MockIeWindowCreation)
  MOCK_STATIC_INIT_BEGIN(MockIeWindowCreation)
    MOCK_STATIC_INIT(CoCreateInstance);
    MOCK_STATIC_INIT(IEIsProtectedModeURL);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC1(HRESULT, CALLBACK, IEIsProtectedModeURL, LPCWSTR);
  MOCK_STATIC5(HRESULT, CALLBACK, CoCreateInstance, REFCLSID, LPUNKNOWN,
               DWORD, REFIID, LPVOID*);
MOCK_STATIC_CLASS_END(MockIeWindowCreation)

TEST_F(WindowApiTests, CreateWindowErrorHandling) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockWindowInvocation<window_api::CreateWindowFunc>> invocation;
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  ListValue wrong_type;
  wrong_type.Append(Value::CreateBooleanValue(false));
  invocation.Execute(wrong_type, kRequestId);

  invocation.AllocateNewResult(kRequestId);
  ListValue args_list;
  DictionaryValue* dict_value = new DictionaryValue;
  args_list.Append(dict_value);
  // Wrong data type...
  dict_value->SetInteger(keys::kUrlKey, rand());
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(args_list, kRequestId);

  invocation.AllocateNewResult(kRequestId);
  // InvalidUrl
  dict_value->SetString(keys::kUrlKey, "ht@tp://www.google.com/");
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(args_list, kRequestId);

  // Valid dictionary...
  dict_value->SetString(keys::kUrlKey, "http://ossum.the.magnificent.com/");
  dict_value->SetInteger(keys::kLeftKey, 21);  // Just to force the rect access.

  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, FindTopLevelWindows(_, _)).Times(AnyNumber());

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  MockIeWindowCreation mock_ie_create;
  // TODO(mad@chromium.org): Test behavior with protected on too.
  EXPECT_CALL(mock_ie_create, IEIsProtectedModeURL(_)).
      WillRepeatedly(Return(S_FALSE));
  EXPECT_CALL(mock_ie_create, CoCreateInstance(_, _, _, _, _)).
      WillOnce(Return(REGDB_E_CLASSNOTREG));
  invocation.Execute(args_list, kRequestId);

  invocation.AllocateNewResult(kRequestId);
  CComObject<StrictMock<testing::MockIWebBrowser2>>* browser;
  CComObject<StrictMock<testing::MockIWebBrowser2>>::CreateInstance(&browser);
  DCHECK(browser != NULL);
  ScopedComPtr<IWebBrowser2> browser_keeper(browser);
  EXPECT_CALL(mock_ie_create, CoCreateInstance(_, _, _, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<4>(browser_keeper.get()),
                     AddRef(browser_keeper.get()), Return(S_OK)));

  EXPECT_CALL(*browser, get_HWND(NotNull())).WillRepeatedly(DoAll(
      SetArgumentPointee<0>(0), Return(S_OK)));
  EXPECT_CALL(*invocation.invocation_result_, UpdateWindowRect(_, _)).
      WillOnce(Return(false));
  invocation.Execute(args_list, kRequestId);

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, UpdateWindowRect(_, _)).
      WillOnce(Return(true));
  EXPECT_CALL(*browser, Navigate(_, _, _, _, _)).WillOnce(Return(E_FAIL));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(args_list, kRequestId);

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, UpdateWindowRect(_, _)).
      WillOnce(Return(true));
  EXPECT_CALL(*browser, Navigate(_, _, _, _, _)).WillRepeatedly(Return(S_OK));
  EXPECT_CALL(*browser, put_Visible(VARIANT_TRUE)).WillOnce(Return(E_FAIL));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(args_list, kRequestId);
}

TEST_F(WindowApiTests, CreateWindowStraightline) {
  testing::LogDisabler no_dchecks;

  ListValue args1;
  DictionaryValue* dict1 = new DictionaryValue;
  args1.Append(dict1);
  dict1->SetString(keys::kUrlKey, "http://ossum.the.magnificent.com/");
  dict1->SetInteger(keys::kLeftKey, 21);  // Just to force the rect access.

  ListValue args2;
  DictionaryValue* dict2 = new DictionaryValue;
  args2.Append(dict2);
  dict2->SetString(keys::kUrlKey, "http://ossum.the.omnipotent.com/");
  dict2->SetString(keys::kWindowTypeKey, keys::kWindowTypeValuePopup);

  ListValue args3;
  args3.Append(Value::CreateNullValue());

  ListValue empty_args;

  const wchar_t* about_blank = L"about:blank";
  struct {
    const ListValue* args;
    const wchar_t* expected_url;
    const bool popup;
  } test_data[] = {
    { &args1, L"http://ossum.the.magnificent.com/", false },
    { &args2, L"http://ossum.the.omnipotent.com/", true },
    { &args3, about_blank, false },
    { &empty_args, about_blank, false },
  };

  typedef StrictMock<MockWindowInvocation<window_api::CreateWindowFunc>>
      MockCreateWindowFunc;

  CComObject<StrictMock<testing::MockIWebBrowser2>>* browser;
  CComObject<StrictMock<testing::MockIWebBrowser2>>::CreateInstance(&browser);
  DCHECK(browser != NULL);
  ScopedComPtr<IWebBrowser2> browser_keeper(browser);
  MockIeWindowCreation mock_ie_create;
  // TODO(mad@chromium.org): Test behavior with protected on too.
  EXPECT_CALL(mock_ie_create, IEIsProtectedModeURL(_)).
      WillRepeatedly(Return(S_FALSE));
  EXPECT_CALL(mock_ie_create, CoCreateInstance(_, _, _, _, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<4>(browser_keeper.get()),
                     AddRef(browser_keeper.get()), Return(S_OK)));
  EXPECT_CALL(*browser, get_HWND(NotNull())).WillRepeatedly(DoAll(
      SetArgumentPointee<0>(0), Return(S_OK)));
  EXPECT_CALL(*browser, Navigate(_, _, _, _, _)).WillRepeatedly(Return(S_OK));
  EXPECT_CALL(*browser, put_Visible(VARIANT_TRUE)).WillRepeatedly(Return(S_OK));

  for (int i = 0; i < arraysize(test_data); ++i) {
    MockCreateWindowFunc invocation;
    invocation.AllocateNewResult(kRequestId);
    EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
    Value* first_arg = NULL;
    if (!test_data[i].args->Get(0, &first_arg) ||
        first_arg->GetType() == Value::TYPE_NULL) {
      EXPECT_CALL(*invocation.invocation_result_, CreateWindowValue(_, _)).
          WillOnce(Return(true));
      invocation.Execute(*test_data[i].args, kRequestId);
    } else {
      EXPECT_CALL(*invocation.invocation_result_, UpdateWindowRect(_, _)).
          WillOnce(Return(true));
      if (test_data[i].popup) {
        EXPECT_CALL(*browser, put_AddressBar(VARIANT_FALSE)).
            WillOnce(Return(S_OK));
        EXPECT_CALL(*browser, put_StatusBar(VARIANT_FALSE)).
            WillOnce(Return(S_OK));
        EXPECT_CALL(*browser, put_ToolBar(FALSE)).WillOnce(Return(S_OK));
      }
      invocation.Execute(*test_data[i].args, kRequestId);
    }
  }
}

TEST_F(WindowApiTests, UpdateWindowErrorHandling) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockWindowInvocation<window_api::UpdateWindow>>
      invocation;
  invocation.AllocateNewResult(kRequestId);

  ListValue root;
  root.Append(Value::CreateIntegerValue(kRandomWindowId));
  // Too few values in list.
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(root, kRequestId);

  invocation.AllocateNewResult(kRequestId);

  Value* wrong_type = Value::CreateStringValue(L"The Answer");
  root.Append(wrong_type);  // root takes ownership.
  // Right number of values, wrong type of second value.
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(root, kRequestId);

  invocation.AllocateNewResult(kRequestId);

  root.Remove(*wrong_type);
  root.Append(new DictionaryValue());  // root takes ownership.
  // Right values, but not IE Frame.
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, IsWindowClass(_, _)).WillOnce(Return(FALSE));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetWindowHandleFromId(kRandomWindowId)).
          WillOnce(Return(kRandomWindowHwnd));
  invocation.Execute(root, kRequestId);
}

TEST_F(WindowApiTests, UpdateWindowStraightline) {
  testing::LogDisabler no_dchecks;

  ListValue root;
  root.Append(Value::CreateIntegerValue(kRandomWindowId));
  DictionaryValue* args = new DictionaryValue();  // root takes ownership.
  root.Append(args);

  MockIsIeFrameClass();

  StrictMock<MockWindowInvocation<window_api::UpdateWindow>>
      invocation;
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, UpdateWindowRect(
      kRandomWindowHwnd, args)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetWindowHandleFromId(kRandomWindowId)).
          WillOnce(Return(kRandomWindowHwnd));
  invocation.Execute(root, kRequestId);
}

class MockRemoveWindow
    : public StrictMock<MockWindowInvocation<window_api::RemoveWindow>> {
};

TEST_F(WindowApiTests, RemoveWindowErrorHandling) {
  testing::LogDisabler no_dchecks;

  // Wrong argument type.
  MockRemoveWindow invocation;
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId);

  // Not a valid window.
  EXPECT_CALL(user32_, IsWindow(_)).WillRepeatedly(Return(FALSE));
  ListValue good_args;
  good_args.Append(Value::CreateIntegerValue(kRandomWindowId));
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetWindowHandleFromId(kRandomWindowId)).
          WillOnce(Return(kRandomWindowHwnd));
  invocation.Execute(good_args, kRequestId);

  // No executor.
  MockIsIeFrameClass();
  EXPECT_CALL(invocation.mock_api_dispatcher_,
              GetExecutor(kRandomWindowHwnd, _, NotNull())).
      WillOnce(SetArgumentPointee<2>((HANDLE)NULL));
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetWindowHandleFromId(kRandomWindowId)).
          WillOnce(Return(kRandomWindowHwnd));
  invocation.Execute(good_args, kRequestId);

  // Executor fails.
  MockGetWindowExecutor(&invocation.mock_api_dispatcher_);
  EXPECT_CALL(*mock_window_executor_, RemoveWindow()).
      WillOnce(Return(E_FAIL));
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetWindowHandleFromId(kRandomWindowId)).
          WillOnce(Return(kRandomWindowHwnd));
  invocation.Execute(good_args, kRequestId);
}

TEST_F(WindowApiTests, RemoveWindowStraightline) {
  testing::LogDisabler no_dchecks;

  MockIsIeFrameClass();
  MockRemoveWindow invocation;
  ListValue good_args;
  good_args.Append(Value::CreateIntegerValue(kRandomWindowId));
  MockGetWindowExecutor(&invocation.mock_api_dispatcher_);
  EXPECT_CALL(*mock_window_executor_, RemoveWindow()).
      WillOnce(Return(S_OK));
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, RegisterEphemeralEventHandler(
      StrEq(ext_event_names::kOnWindowRemoved),
      window_api::RemoveWindow::ContinueExecution,
      invocation.invocation_result_.get())).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetWindowHandleFromId(kRandomWindowId)).
          WillOnce(Return(kRandomWindowHwnd));
  ApiDispatcher::InvocationResult* result = invocation.invocation_result_.get();
  invocation.Execute(good_args, kRequestId);

  std::ostringstream args;
  args << "[" << kRandomWindowId << "]";
  invocation.ContinueExecution(args.str(), result, invocation.GetDispatcher());
}

TEST_F(WindowApiTests, GetAllWindowsErrorHandling) {
  testing::LogDisabler no_dchecks;
  ListValue bad_args;
  DictionaryValue* bad_dict = new DictionaryValue();
  bad_dict->SetInteger(keys::kPopulateKey, 42);
  bad_args.Append(bad_dict);
  StrictMock<MockIterativeWindowInvocation<window_api::GetAllWindows> >
      invocation;
  invocation.AllocateNewResult(kRequestId);
  // Using a strict mock ensures that FindTopLevelWindows won't get called.
  MockIterativeWindowApiResult::ResetCounters();
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostError(_)).Times(1);
  invocation.Execute(bad_args, kRequestId);
  EXPECT_EQ(MockIterativeWindowApiResult::error_counter(), 1);
}

TEST_F(WindowApiTests, GetAllWindowsStraightline) {
  testing::LogDisabler no_dchecks;

  ListValue empty_args;
  ListValue null_args;
  null_args.Append(Value::CreateNullValue());
  ListValue true_args;
  DictionaryValue* dict_value = new DictionaryValue;
  // The list will take ownership
  ASSERT_TRUE(true_args.Set(0, dict_value));
  dict_value->SetBoolean(keys::kPopulateKey, true);
  struct {
    const ListValue* value;
    bool populate;
  } values[] = {
      { &empty_args, false },
      { &null_args, false },
      { &true_args, true }
  };

  std::set<HWND> ie_hwnds;
  static const HWND kWindow1 = reinterpret_cast<HWND>(1);
  ie_hwnds.insert(kWindow1);
  static const HWND kWindow2 = reinterpret_cast<HWND>(2);
  ie_hwnds.insert(kWindow2);

  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, FindTopLevelWindows(_, NotNull())).WillRepeatedly(
      SetArgumentPointee<1>(ie_hwnds));

  EXPECT_CALL(user32_, GetWindowThreadProcessId(_, _)).
      WillRepeatedly(Return(1));

  for (int i = 0; i < arraysize(values); ++i) {
    StrictMock<MockIterativeWindowInvocation<window_api::GetAllWindows> >
        invocation;
    invocation.AllocateNewResult(kRequestId);
    EXPECT_CALL(*invocation.invocation_result_, CreateWindowValue(
                kWindow1, values[i].populate)).WillOnce(Return(true));
    EXPECT_CALL(*invocation.invocation_result_, CreateWindowValue(
                kWindow2, values[i].populate)).WillOnce(Return(true));
    EXPECT_CALL(*invocation.invocation_result_, CallRealPostResult()).Times(1);
    MockIterativeWindowApiResult::ResetCounters();
    invocation.Execute(*values[i].value, kRequestId);
    EXPECT_EQ(MockIterativeWindowApiResult::error_counter(), 0);
    EXPECT_EQ(MockIterativeWindowApiResult::success_counter(), ie_hwnds.size());
  }
}

}  // namespace
