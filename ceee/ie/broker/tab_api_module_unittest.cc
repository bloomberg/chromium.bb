// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tab API implementation unit tests.

// MockWin32 can't be included after ChromeFrameHost because of an include
// incompatibility with atlwin.h.
#include "ceee/testing/utils/mock_win32.h"  // NOLINT

#include "base/json/json_writer.h"
#include "base/json/json_reader.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/chrome_postman.h"
#include "ceee/ie/broker/tab_api_module.h"
#include "ceee/ie/common/ie_util.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/test_utils.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ext = extension_tabs_module_constants;
namespace ext_event_names = extension_event_names;

namespace {

using tab_api::CreateTab;
using tab_api::GetAllTabsInWindow;
using tab_api::GetAllTabsInWindowResult;
using tab_api::GetCurrentTab;
using tab_api::GetSelectedTab;
using tab_api::GetTab;
using tab_api::MoveTab;
using tab_api::RegisterInvocations;
using tab_api::RemoveTab;
using tab_api::TabApiResult;
using tab_api::TabsInsertCode;
using tab_api::UpdateTab;

using testing::_;
using testing::AddRef;
using testing::AtLeast;
using testing::CopyInterfaceToArgument;
using testing::CopyStringToArgument;
using testing::Gt;
using testing::InstanceCountMixin;
using testing::MockApiDispatcher;
using testing::MockApiInvocation;
using testing::NotNull;
using testing::Return;
using testing::Sequence;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;
using testing::VariantPointerEq;

const int kGoodTabWindowId = 99;
const int kGoodFrameWindowId = 88;
const int kBadWindowId = 666;

const CeeeWindowHandle kGoodTabWindowHandle = kGoodTabWindowId + 1;
const CeeeWindowHandle kGoodFrameWindowHandle = kGoodFrameWindowId + 1;
const CeeeWindowHandle kBadWindowHandle = kBadWindowId + 1;

const HWND kGoodTabWindow = reinterpret_cast<HWND>(kGoodTabWindowHandle);
const HWND kGoodFrameWindow = reinterpret_cast<HWND>(kGoodFrameWindowHandle);
const HWND kBadWindow = reinterpret_cast<HWND>(kBadWindowHandle);

const int kTabIndex = 26;
const wchar_t kClassName[] = L"TabWindowClass";
const int kRequestId = 43;

TEST(TabApi, RegisterInvocations) {
  StrictMock<MockApiDispatcher> disp;
  EXPECT_CALL(disp, RegisterInvocation(NotNull(), NotNull())).Times(AtLeast(7));
  RegisterInvocations(&disp);
}

class MockTabApiResult : public TabApiResult {
 public:
  explicit MockTabApiResult(int request_id) : TabApiResult(request_id) {}
  MOCK_METHOD2(GetSpecifiedOrCurrentFrameWindow, HWND(const Value&, bool*));
  MOCK_METHOD2(CreateTabValue, bool(int, long));
  MOCK_METHOD0(PostResult, void());
  MOCK_METHOD1(PostError, void(const std::string&));

  virtual ApiDispatcher* GetDispatcher() {
    return &mock_api_dispatcher_;
  }

  // Need to overload to bypass the mock.
  HWND CallGetSpecifiedOrCurrentFrameWindow(const Value& args,
                                            bool* specified) {
    return TabApiResult::GetSpecifiedOrCurrentFrameWindow(args, specified);
  }
  bool CallCreateTabValue(int tab_id, long index) {
    return TabApiResult::CreateTabValue(tab_id, index);
  }

  StrictMock<MockApiDispatcher> mock_api_dispatcher_;
};

// TODO(mad@chromium.org) TODO(hansl@google.com): Unify tests between
// {window,tab}_module_api. Create a base class for executor mocker
// tests, since we now have two very similar classes. Consider adding
// the cookie and infobar API tests too.
class TabApiTests: public testing::Test {
 public:
  virtual void SetUp() {
    EXPECT_HRESULT_SUCCEEDED(testing::MockTabExecutor::CreateInitialized(
        &mock_tab_executor_, mock_tab_executor_keeper_.Receive()));
    EXPECT_HRESULT_SUCCEEDED(testing::MockWindowExecutor::CreateInitialized(
        &mock_window_executor_, mock_window_executor_keeper_.Receive()));
    if (!testing::MockExecutorsManager::test_instance_) {
      // To be freed at exit by Singleton class.
      testing::MockExecutorsManager::test_instance_ =
          new StrictMock<testing::MockExecutorsManager>;
    }
    executors_manager_ =
        reinterpret_cast<StrictMock<testing::MockExecutorsManager>*>(
            testing::MockExecutorsManager::test_instance_);
  }

  virtual void TearDown() {
    // Everything should have been relinquished.
    mock_window_executor_keeper_.Release();
    mock_tab_executor_keeper_.Release();
    ASSERT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
  }

 protected:
  void AlwaysMockGetTabExecutor(MockApiDispatcher* api_dispatcher,
                                HWND window) {
    // We can't use CopyInterfaceToArgument here because GetExecutor takes a
    // void** argument instead of an interface.
    EXPECT_CALL(*api_dispatcher, GetExecutor(window, _, NotNull())).
        WillRepeatedly(DoAll(
            SetArgumentPointee<2>(mock_tab_executor_keeper_.get()),
            AddRef(mock_tab_executor_keeper_.get())));
  }

  void MockGetTabExecutorOnce(MockApiDispatcher* api_dispatcher, HWND window) {
    // We can't use CopyInterfaceToArgument here because GetExecutor takes a
    // void** argument instead of an interface.
    EXPECT_CALL(*api_dispatcher, GetExecutor(window, _, NotNull())).
        WillOnce(DoAll(SetArgumentPointee<2>(mock_tab_executor_keeper_.get()),
                       AddRef(mock_tab_executor_keeper_.get())));
  }

  void MockGetWindowExecutorOnce(MockApiDispatcher* api_dispatcher,
                                 HWND window) {
    EXPECT_CALL(*api_dispatcher, GetExecutor(window, _, NotNull())).
        WillOnce(DoAll(
            SetArgumentPointee<2>(mock_window_executor_keeper_.get()),
            AddRef(mock_window_executor_keeper_.get())));
  }

  void AlwaysMockGetWindowExecutor(MockApiDispatcher* api_dispatcher,
                                   HWND window) {
    EXPECT_CALL(*api_dispatcher, GetExecutor(window, _, NotNull())).
        WillRepeatedly(DoAll(
            SetArgumentPointee<2>(mock_window_executor_keeper_.get()),
            AddRef(mock_window_executor_keeper_.get())));
  }

  StrictMock<testing::MockUser32> user32_;
  // The executor classes are already strict from their base class impl.
  testing::MockTabExecutor* mock_tab_executor_;
  testing::MockWindowExecutor* mock_window_executor_;
  // Lifespan controlled by Singleton template.
  StrictMock<testing::MockExecutorsManager>* executors_manager_;

 private:
  class MockChromePostman : public ChromePostman {
   public:
    MOCK_METHOD2(PostMessage, void(BSTR, BSTR));
  };
  // We should never get to the postman, we mock all the calls getting there.
  // So we simply instantiate it strict and it will register itself as the
  // one and only singleton to use all the time.
  CComObjectStackEx< StrictMock< MockChromePostman > > postman_;
  // To control the life span of the tab executor.
  base::win::ScopedComPtr<ICeeeTabExecutor> mock_tab_executor_keeper_;
  base::win::ScopedComPtr<ICeeeWindowExecutor> mock_window_executor_keeper_;
};

TEST_F(TabApiTests, CreateTabValueErrorHandling) {
  testing::LogDisabler no_dchecks;

  // Window with no thread.
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, WindowHasNoThread(kGoodTabWindow)).
      WillOnce(Return(true));

  StrictMock<MockTabApiResult> invocation_result(TabApiResult::kNoRequestId);
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).
      WillRepeatedly(Return(kGoodTabWindow));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowIdFromHandle(kGoodFrameWindow)).
      WillRepeatedly(Return(kGoodFrameWindowId));

  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateTabValue(kGoodTabWindowId, -1));

  // Not an IE Frame.
  invocation_result.set_value(NULL);
  EXPECT_CALL(window_utils, WindowHasNoThread(kGoodTabWindow)).
      WillRepeatedly(Return(false));
  EXPECT_CALL(window_utils, IsWindowClass(_, _)).WillOnce(Return(false));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateTabValue(kGoodTabWindowId, -1));

  // No Executor for the tab window.
  invocation_result.set_value(NULL);
  EXPECT_CALL(window_utils, GetTopLevelParent(_)).
      WillRepeatedly(Return(kGoodFrameWindow));
  EXPECT_CALL(window_utils, IsWindowClass(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(user32_, IsWindowVisible(_)).WillRepeatedly(Return(FALSE));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
              GetExecutor(kGoodTabWindow, _, _)).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateTabValue(kGoodTabWindowId, -1));

  // Failing executor for the tab info.
  invocation_result.set_value(NULL);
  MockGetTabExecutorOnce(&invocation_result.mock_api_dispatcher_,
                         kGoodTabWindow);
  EXPECT_CALL(*mock_tab_executor_, GetTabInfo(NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateTabValue(kGoodTabWindowId, -1));

  // No Executor for the frame window.
  // We only mock once at a time so that the executor for the tab doesn't get
  // confused by the one for the frame window, or vice versa.
  invocation_result.set_value(NULL);
  MockGetTabExecutorOnce(&invocation_result.mock_api_dispatcher_,
                         kGoodTabWindow);
  EXPECT_CALL(*mock_tab_executor_, GetTabInfo(NotNull())).
      WillRepeatedly(Return(S_OK));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
              GetExecutor(kGoodFrameWindow, _, NotNull())).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateTabValue(kGoodTabWindowId, -1));

  // Failing executor for the tab index.
  invocation_result.set_value(NULL);
  AlwaysMockGetWindowExecutor(&invocation_result.mock_api_dispatcher_,
      kGoodFrameWindow);
  AlwaysMockGetTabExecutor(&invocation_result.mock_api_dispatcher_,
      kGoodTabWindow);
  EXPECT_CALL(*mock_window_executor_, GetTabIndex(kGoodTabWindowHandle,
      NotNull())).WillOnce(Return(E_FAIL));
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  EXPECT_FALSE(invocation_result.CallCreateTabValue(kGoodTabWindowId, -1));
}

TEST_F(TabApiTests, CreateTabValue) {
  // Setup all we need from other for success.
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, WindowHasNoThread(kGoodTabWindow)).
      WillRepeatedly(Return(false));
  EXPECT_CALL(window_utils, IsWindowClass(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(window_utils, GetTopLevelParent(_)).
      WillRepeatedly(Return(kGoodFrameWindow));

  // Try a few different combination or parameters.
  struct {
    bool window_selected;
    const char* status;
    const char* fav_icon_url;
    const char* title;
    const char* url;
    int index;
  } tests[] = {
    { false, ext::kStatusValueLoading, NULL, "Mine", "http://icietla", 1 },
    { false, ext::kStatusValueComplete, "Here", "Yours", "https://secure", 32 },
    { true, ext::kStatusValueLoading, "There", "There's", "Just a string", -1 },
    { true, ext::kStatusValueComplete, NULL, "Unknown", "Boo!!!", -1 },
  };

  for (int i = 0; i < arraysize(tests); ++i) {
    EXPECT_CALL(user32_, IsWindowVisible(_)).WillOnce(
        Return(tests[i].window_selected ? TRUE : FALSE));
    CeeeTabInfo tab_info;
    // The allocation within the tab_info will be freed by the caller of
    // GetTabInfo which we mock below...
    tab_info.url = ::SysAllocString(CA2W(tests[i].url).m_psz);
    tab_info.title = ::SysAllocString(CA2W(tests[i].title).m_psz);
    tab_info.protected_mode = FALSE;

    if (strcmp(tests[i].status, ext::kStatusValueLoading) == 0)
      tab_info.status = kCeeeTabStatusLoading;
    else
      tab_info.status = kCeeeTabStatusComplete;

    if (tests[i].fav_icon_url) {
      tab_info.fav_icon_url =
          ::SysAllocString(CA2W(tests[i].fav_icon_url).m_psz);
    } else {
      tab_info.fav_icon_url = NULL;
    }

    StrictMock<MockTabApiResult> invocation_result(TabApiResult::kNoRequestId);
    AlwaysMockGetTabExecutor(&invocation_result.mock_api_dispatcher_,
                             kGoodTabWindow);
    AlwaysMockGetWindowExecutor(&invocation_result.mock_api_dispatcher_,
                                kGoodFrameWindow);
    EXPECT_CALL(*mock_tab_executor_, GetTabInfo(NotNull())).
        WillOnce(DoAll(SetArgumentPointee<0>(tab_info), Return(S_OK)));
    EXPECT_CALL(invocation_result.mock_api_dispatcher_,
        GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
    EXPECT_CALL(invocation_result.mock_api_dispatcher_,
        GetWindowIdFromHandle(kGoodFrameWindow)).
        WillOnce(Return(kGoodFrameWindowId));

    if (tests[i].index == -1) {
      EXPECT_CALL(*mock_window_executor_, GetTabIndex(kGoodTabWindowHandle,
          NotNull())).WillOnce(DoAll(SetArgumentPointee<1>(kTabIndex),
              Return(S_OK)));
    }
    if (i % 2 == 0)
      invocation_result.set_value(new ListValue);
    EXPECT_TRUE(invocation_result.CallCreateTabValue(
        kGoodTabWindowId, tests[i].index));
    EXPECT_NE(static_cast<Value*>(NULL), invocation_result.value());
    const Value* result = NULL;
    if (i % 2 == 0) {
      EXPECT_TRUE(invocation_result.value()->IsType(Value::TYPE_LIST));
      const ListValue* list = static_cast<const ListValue*>(
          invocation_result.value());
      Value * non_const_result = NULL;
      EXPECT_TRUE(list->Get(0, &non_const_result));
      result = non_const_result;
    } else {
      result = invocation_result.value();
    }
    EXPECT_TRUE(result->IsType(Value::TYPE_DICTIONARY));
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(result);

    int tab_id;
    EXPECT_TRUE(dict->GetInteger(ext::kIdKey, &tab_id));
    EXPECT_EQ(kGoodTabWindowId, tab_id);

    int window_id;
    EXPECT_TRUE(dict->GetInteger(ext::kWindowIdKey, &window_id));
    EXPECT_EQ(kGoodFrameWindowId, window_id);

    bool selected = false;
    EXPECT_TRUE(dict->GetBoolean(ext::kSelectedKey, &selected));
    EXPECT_EQ(tests[i].window_selected, selected);

    std::string url;
    EXPECT_TRUE(dict->GetString(ext::kUrlKey, &url));
    EXPECT_STREQ(tests[i].url, url.c_str());

    std::string title;
    EXPECT_TRUE(dict->GetString(ext::kTitleKey, &title));
    EXPECT_STREQ(tests[i].title, title.c_str());

    std::string status;
    EXPECT_TRUE(dict->GetString(ext::kStatusKey, &status));
    EXPECT_STREQ(tests[i].status, status.c_str());

    std::string fav_icon_url;
    if (tests[i].fav_icon_url == NULL) {
      EXPECT_FALSE(dict->GetString(ext::kFavIconUrlKey, &fav_icon_url));
    } else {
      EXPECT_TRUE(dict->GetString(ext::kFavIconUrlKey, &fav_icon_url));
      EXPECT_STREQ(tests[i].fav_icon_url, fav_icon_url.c_str());
    }

    int index = -1;
    EXPECT_TRUE(dict->GetInteger(ext::kIndexKey, &index));
    if (tests[i].index == -1)
      EXPECT_EQ(kTabIndex, index);
    else
      EXPECT_EQ(tests[i].index, index);
  }
}

TEST(TabApi, IsTabWindowClassWithNull) {
  testing::LogDisabler no_dchecks;
  TabApiResult invocation_result(TabApiResult::kNoRequestId);
  EXPECT_FALSE(invocation_result.IsTabWindowClass(NULL));
}

TEST(TabApi, IsTabWindowClassWithNonWindow) {
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, IsWindow(NotNull())).WillRepeatedly(Return(FALSE));
  TabApiResult invocation_result(TabApiResult::kNoRequestId);
  EXPECT_FALSE(invocation_result.IsTabWindowClass(reinterpret_cast<HWND>(1)));
}

TEST(TabApi, IsTabWindowClassWrongClassName) {
  const wchar_t kBadClassName[] = L"BadWindowClass";
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, IsWindow(NotNull())).WillOnce(Return(TRUE));
  EXPECT_CALL(user32, GetClassName(NotNull(), NotNull(), Gt(0))).WillOnce(
      DoAll(CopyStringToArgument<1>(kBadClassName),
            Return(arraysize(kBadClassName))));
  TabApiResult invocation_result(TabApiResult::kNoRequestId);
  EXPECT_FALSE(invocation_result.IsTabWindowClass(reinterpret_cast<HWND>(1)));
}

TEST(TabApi, IsTabWindowClassStraightline) {
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, IsWindow(NotNull())).WillOnce(Return(TRUE));
  EXPECT_CALL(user32, GetClassName(NotNull(), NotNull(), Gt(0))).WillOnce(
      DoAll(CopyStringToArgument<1>(kClassName),
            Return(arraysize(kClassName))));
  TabApiResult invocation_result(TabApiResult::kNoRequestId);
  EXPECT_TRUE(invocation_result.IsTabWindowClass(reinterpret_cast<HWND>(1)));
}

TEST(TabApi, GetSpecifiedOrCurrentFrameWindow) {
  testing::LogDisabler no_dchecks;

  StrictMock<testing::MockWindowUtils> window_utils;
  scoped_ptr<Value> bad_args(Value::CreateDoubleValue(4.7));
  DictionaryValue bad_dict_args;  // no window ID key.
  DictionaryValue good_dict_args;
  int window1_id = 77;
  HWND window1 = reinterpret_cast<HWND>(window1_id);
  good_dict_args.SetInteger(ext::kWindowIdKey, window1_id);

  StrictMock<MockTabApiResult> invocation_result(TabApiResult::kNoRequestId);

  // First, fail because no method finds us a window.
  EXPECT_CALL(window_utils, FindDescendentWindow(_, _, _, _))
      .WillOnce(Return(false));
  HWND null_window = NULL;
  EXPECT_EQ(null_window, invocation_result.CallGetSpecifiedOrCurrentFrameWindow(
      ListValue(), NULL));

  // Then, fail because we find a window that's not a window.
  HWND window2 = reinterpret_cast<HWND>(88);
  EXPECT_CALL(window_utils, FindDescendentWindow(_, _, _, _)).WillRepeatedly(
      DoAll(SetArgumentPointee<3>(window2), Return(true)));
  EXPECT_CALL(window_utils, IsWindowClass(window2, _)).WillOnce(Return(false));
  EXPECT_EQ(null_window, invocation_result.CallGetSpecifiedOrCurrentFrameWindow(
      ListValue(), NULL));

  // From now on, all windows are valid.
  EXPECT_CALL(window_utils, IsWindowClass(_, _)).WillRepeatedly(Return(true));

  // So succeed for once - we're using null args so we get the
  // window from the FindDescendentWindow.
  bool specified = true;
  EXPECT_EQ(window2, invocation_result.CallGetSpecifiedOrCurrentFrameWindow(
      ListValue(), &specified));
  EXPECT_FALSE(specified);

  ListValue good_list_with_null;
  ASSERT_TRUE(good_list_with_null.Set(0, Value::CreateNullValue()));
  specified = true;
  EXPECT_EQ(window2, invocation_result.CallGetSpecifiedOrCurrentFrameWindow(
      good_list_with_null, &specified));
  EXPECT_FALSE(specified);

  ListValue good_list_with_null_dict;
  ASSERT_TRUE(good_list_with_null_dict.Set(0, new DictionaryValue()));
  specified = true;
  EXPECT_EQ(window2, invocation_result.CallGetSpecifiedOrCurrentFrameWindow(
    good_list_with_null_dict, &specified));
  EXPECT_FALSE(specified);

  // From now on, we can expect those to always return consistent values.
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowHandleFromId(kGoodTabWindowId)).
      WillRepeatedly(Return(kGoodTabWindow));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetWindowHandleFromId(window1_id)).WillRepeatedly(Return(window1));

  // Get window from good args.
  scoped_ptr<Value> good_int_args(Value::CreateIntegerValue(kGoodTabWindowId));
  ListValue good_list_with_int;
  ASSERT_TRUE(good_list_with_int.Set(0, Value::CreateIntegerValue(window1_id)));
  ListValue good_list_with_dict;
  ASSERT_TRUE(good_list_with_dict.Set(0, good_dict_args.DeepCopy()));

  EXPECT_EQ(kGoodTabWindow, invocation_result.
      CallGetSpecifiedOrCurrentFrameWindow(*good_int_args.get(), &specified));
  EXPECT_TRUE(specified);
  specified = false;
  EXPECT_EQ(window1, invocation_result.CallGetSpecifiedOrCurrentFrameWindow(
      good_dict_args, &specified));
  EXPECT_TRUE(specified);
  specified = false;
  EXPECT_EQ(window1, invocation_result.CallGetSpecifiedOrCurrentFrameWindow(
      good_list_with_int, &specified));
  EXPECT_TRUE(specified);
  specified = false;
  EXPECT_EQ(window1, invocation_result.CallGetSpecifiedOrCurrentFrameWindow(
      good_list_with_dict, &specified));
  EXPECT_TRUE(specified);
}

TEST_F(TabApiTests, GetTab) {
  testing::LogDisabler no_dchecks;

  ListValue good_args;
  ASSERT_TRUE(good_args.Set(0, Value::CreateIntegerValue(kGoodTabWindowId)));
  ListValue bad_window;
  ASSERT_TRUE(bad_window.Set(0, Value::CreateIntegerValue(kBadWindowId)));

  // Mocking IsTabWindowClass.
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, IsWindowClass(kGoodTabWindow, _)).
      WillRepeatedly(Return(true));
  EXPECT_CALL(window_utils, IsWindowClass(kBadWindow, _)).
      WillRepeatedly(Return(false));

  StrictMock<MockApiInvocation<TabApiResult, MockTabApiResult, GetTab> >
      invocation;
  invocation.AllocateNewResult(kRequestId);

  // Start with success.
  EXPECT_CALL(*invocation.invocation_result_,
              CreateTabValue(kGoodTabWindowId, _)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(good_args, kRequestId);

  // No more successful calls.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(false));
  invocation.Execute(good_args, kRequestId);

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kBadWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kBadWindowId)).WillOnce(Return(kBadWindow));
  invocation.Execute(bad_window, kRequestId);

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId);
}

TEST_F(TabApiTests, GetCurrentTab) {
  testing::LogDisabler no_dchecks;

  // Mocking IsTabWindowClass.
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, IsWindowClass(kGoodTabWindow, _)).
      WillRepeatedly(Return(true));
  EXPECT_CALL(window_utils, IsWindowClass(kBadWindow, _)).
      WillRepeatedly(Return(false));

  StrictMock<MockApiInvocation<TabApiResult, MockTabApiResult, GetCurrentTab> >
      invocation;

  // Start with success--no context.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  invocation.Execute(ListValue(), kRequestId, NULL);

  // Now successfully get a current tab.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(invocation.mock_api_dispatcher_, GetTabHandleFromToolBandId(3)).
      WillOnce(Return(kGoodTabWindow));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabIdFromHandle(kGoodTabWindow)).WillOnce(Return(kGoodTabWindowId));
  EXPECT_CALL(*invocation.invocation_result_,
      CreateTabValue(kGoodTabWindowId, -1)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  DictionaryValue associated_tab;
  associated_tab.SetInteger(ext::kIdKey, 3);
  invocation.Execute(ListValue(), kRequestId, &associated_tab);

  // No more successful calls.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(invocation.mock_api_dispatcher_, GetTabHandleFromToolBandId(3)).
      WillOnce(Return(kBadWindow));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabIdFromHandle(kBadWindow)).WillOnce(Return(kBadWindowId));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId, &associated_tab);

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  associated_tab.Remove(ext::kIdKey, NULL);
  invocation.Execute(ListValue(), kRequestId, &associated_tab);
}

TEST_F(TabApiTests, GetSelectedTab) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockApiInvocation<TabApiResult, MockTabApiResult, GetSelectedTab> >
      invocation;
  // No frame window.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
      GetSpecifiedOrCurrentFrameWindow(_, _)).WillOnce(
          Return(static_cast<HWND>(NULL)));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId);

  // Success
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              GetSpecifiedOrCurrentFrameWindow(_, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(true),
                           Return(kGoodFrameWindow)));
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, FindDescendentWindow(
      kGoodFrameWindow, _, _, NotNull())).
          WillOnce(DoAll(SetArgumentPointee<3>(kGoodTabWindow), Return(true)));
  EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(kGoodTabWindow)).
      WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_,
              CreateTabValue(kGoodTabWindowId, _)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabIdFromHandle(kGoodTabWindow)).WillOnce(Return(kGoodTabWindowId));
  invocation.Execute(ListValue(), kRequestId);

  // TODO(mad@chromium.org): Try the async case.
}

class MockGetAllTabsInWindowResult : public GetAllTabsInWindowResult {
 public:
  explicit MockGetAllTabsInWindowResult(int request_id)
      : GetAllTabsInWindowResult(request_id) {}
  MOCK_METHOD2(CreateTabValue, bool(int, long));
  MOCK_METHOD2(GetSpecifiedOrCurrentFrameWindow, HWND(const Value&, bool*));
  MOCK_METHOD1(Execute, bool(BSTR));
  MOCK_METHOD0(PostResult, void());
  MOCK_METHOD1(PostError, void(const std::string&));

  virtual ApiDispatcher* GetDispatcher() {
    return &mock_api_dispatcher_;
  }

  bool CallExecute(BSTR tabs) {
    return GetAllTabsInWindowResult::Execute(tabs);
  }

  StrictMock<MockApiDispatcher> mock_api_dispatcher_;
};

TEST_F(TabApiTests, GetAllTabsInWindowResult) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockGetAllTabsInWindowResult> invocation_result(
      TabApiResult::kNoRequestId);

  // Start with a few failure cases.
  // Not a proper JSON string.
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  invocation_result.CallExecute(L"Bla bla bla");
  invocation_result.set_value(NULL);

  // Proper JSON string, not being a list.
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  invocation_result.CallExecute(L"{}");
  invocation_result.set_value(NULL);

  // Wrong number of elements in the list.
  EXPECT_CALL(invocation_result, PostError(_)).Times(1);
  invocation_result.CallExecute(L"[23]");
  invocation_result.set_value(NULL);

  // Empty is valid yet doesn't get CreateTabValue called.
  EXPECT_TRUE(invocation_result.CallExecute(L"[]"));
  invocation_result.set_value(NULL);

  // Successes.
  int index = 24;
  EXPECT_CALL(invocation_result, CreateTabValue(kGoodTabWindowId, index)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation_result.mock_api_dispatcher_,
      GetTabIdFromHandle(kGoodTabWindow)).WillOnce(Return(kGoodTabWindowId));
  EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(kGoodTabWindow)).
      WillOnce(Return(true));
  {
    std::wostringstream args;
    args << L"[" << kGoodTabWindowHandle << L"," << index << L"]";
    EXPECT_TRUE(invocation_result.CallExecute((BSTR)(args.str().c_str())));
    EXPECT_NE(static_cast<Value*>(NULL), invocation_result.value());
    invocation_result.set_value(NULL);
  }

  {
    std::wostringstream args;
    Sequence seq;
    struct id_map_struct {
      int id;
      HWND handle;
      int value;
    } const test_id_map[] = {
      { 1, reinterpret_cast<HWND>(2), 3 },
      { 4, reinterpret_cast<HWND>(5), 6 },
      { 7, reinterpret_cast<HWND>(8), 9 },
      { 0 }
    };

    // Build the JSON list from the above test map. We also want to expect the
    // correct underlying calls.
    args << L"[";
    for (int i = 0; test_id_map[i].id != 0; ++i) {
      const id_map_struct& item = test_id_map[i];
      EXPECT_CALL(invocation_result.mock_api_dispatcher_,
          GetTabIdFromHandle(item.handle)).WillOnce(Return(item.id));
      EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(item.handle)).
          WillOnce(Return(true));
      EXPECT_CALL(invocation_result, CreateTabValue(item.id, item.value)).
          InSequence(seq).WillOnce(Return(true));
      if (i)
        args << L",";
      args << reinterpret_cast<int>(item.handle) << L"," << item.value;
    }
    args << L"]";
    EXPECT_TRUE(invocation_result.CallExecute((BSTR)(args.str().c_str())));
    EXPECT_NE(static_cast<Value*>(NULL), invocation_result.value());
  }
}

TEST_F(TabApiTests, GetAllTabsInWindow) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockApiInvocation<GetAllTabsInWindowResult,
      MockGetAllTabsInWindowResult, GetAllTabsInWindow>> invocation;

  // No frame window.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
      GetSpecifiedOrCurrentFrameWindow(_, _)).WillOnce(
          Return(static_cast<HWND>(NULL)));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId);

  // No Executor.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              GetSpecifiedOrCurrentFrameWindow(_, _)).
      WillRepeatedly(Return(kGoodFrameWindow));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
              GetExecutor(kGoodFrameWindow, _, NotNull())).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId);

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              GetSpecifiedOrCurrentFrameWindow(_, _)).
      WillRepeatedly(Return(kGoodFrameWindow));

  // Failing Executor.
  // The executor classes are already strict from their base class impl.
  testing::MockWindowExecutor* mock_window_executor;
  base::win::ScopedComPtr<ICeeeWindowExecutor> mock_window_executor_keeper_;
  EXPECT_HRESULT_SUCCEEDED(testing::MockWindowExecutor::CreateInitialized(
      &mock_window_executor, mock_window_executor_keeper_.Receive()));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
              GetExecutor(kGoodFrameWindow, _, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(
                               mock_window_executor_keeper_.get()),
                           AddRef(mock_window_executor_keeper_.get())));
  EXPECT_CALL(*mock_window_executor, GetTabs(NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(ListValue(), kRequestId);

  // Successes.
  invocation.AllocateNewResult(kRequestId);
  BSTR tab_ids = ::SysAllocString(L"");
  EXPECT_CALL(*invocation.invocation_result_,
              GetSpecifiedOrCurrentFrameWindow(_, _)).
      WillOnce(Return(kGoodFrameWindow));
  EXPECT_CALL(*mock_window_executor, GetTabs(NotNull())).
      WillOnce(DoAll(SetArgumentPointee<0>(tab_ids), Return(S_OK)));
  EXPECT_CALL(*invocation.invocation_result_, Execute(tab_ids)).
      WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  invocation.Execute(ListValue(), kRequestId);
  // Execute freed tab_ids...
  tab_ids = NULL;
}

// TODO(mad@chromium.org): Test the event handling.
class MockUpdateTab : public StrictMock<MockApiInvocation<TabApiResult,
                                                          MockTabApiResult,
                                                          UpdateTab> > {
};

TEST_F(TabApiTests, UpdateTab) {
  ListValue bad_window;
  bad_window.Append(Value::CreateIntegerValue(kBadWindowId));

  ListValue wrong_second_arg;
  wrong_second_arg.Append(Value::CreateIntegerValue(kGoodTabWindowId));
  wrong_second_arg.Append(Value::CreateStringValue(L"Angus"));

  ListValue wrong_url_type;
  DictionaryValue* wrong_url_type_dictionary = new DictionaryValue();
  wrong_url_type_dictionary->SetInteger(ext::kUrlKey, 1);
  wrong_url_type.Append(Value::CreateIntegerValue(kGoodTabWindowId));
  wrong_url_type.Append(wrong_url_type_dictionary);

  ListValue wrong_selected_type;
  DictionaryValue* wrong_selected_type_dictionary = new DictionaryValue();
  wrong_selected_type_dictionary->SetString(ext::kSelectedKey, L"yes");
  wrong_selected_type.Append(Value::CreateIntegerValue(kGoodTabWindowId));
  wrong_selected_type.Append(wrong_selected_type_dictionary);

  ListValue good_url;
  DictionaryValue* good_url_dictionary = new DictionaryValue();
  good_url_dictionary->SetString(ext::kUrlKey, "http://google.com");
  good_url.Append(Value::CreateIntegerValue(kGoodTabWindowId));
  good_url.Append(good_url_dictionary);

  ListValue selected_true;
  DictionaryValue* selected_true_dictionary = new DictionaryValue();
  selected_true_dictionary->SetBoolean(ext::kSelectedKey, true);
  selected_true.Append(Value::CreateIntegerValue(kGoodTabWindowId));
  selected_true.Append(selected_true_dictionary);

  testing::LogDisabler no_dchecks;

  MockUpdateTab invocation;
  ListValue empty_list;
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(empty_list, kRequestId);

  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kBadWindowId)).WillRepeatedly(Return(kBadWindow));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).
      WillRepeatedly(Return(kGoodTabWindow));

  // Not an IeServerClass.
  invocation.AllocateNewResult(kRequestId);
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, IsWindowClass(kBadWindow, _)).
      WillOnce(Return(false));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(bad_window, kRequestId);

  // Window has no thread.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(window_utils, IsWindowClass(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(window_utils, WindowHasNoThread(kBadWindow)).
      WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(bad_window, kRequestId);

  EXPECT_CALL(window_utils, WindowHasNoThread(_)).WillRepeatedly(Return(false));

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(wrong_second_arg, kRequestId);
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(wrong_url_type, kRequestId);
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(wrong_selected_type, kRequestId);

  // Can't get an executor for Navigate.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(invocation.mock_api_dispatcher_,
              GetExecutor(kGoodTabWindow, _, NotNull())).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(good_url, kRequestId);

  // Failing Executor.
  invocation.AllocateNewResult(kRequestId);
  AlwaysMockGetTabExecutor(&invocation.mock_api_dispatcher_, kGoodTabWindow);
  EXPECT_CALL(*mock_tab_executor_, Navigate(_, _, _)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(good_url, kRequestId);

  // Can't get an executor for SelectTab.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(window_utils, GetTopLevelParent(kGoodTabWindow)).
      WillRepeatedly(Return(kGoodFrameWindow));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
              GetExecutor(kGoodFrameWindow, _, NotNull())).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(selected_true, kRequestId);

  // Failing Executor.
  invocation.AllocateNewResult(kRequestId);
  AlwaysMockGetWindowExecutor(&invocation.mock_api_dispatcher_,
      kGoodFrameWindow);
  EXPECT_CALL(*mock_window_executor_, SelectTab(kGoodTabWindowHandle)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(selected_true, kRequestId);

  // Successful Navigate.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*mock_tab_executor_, Navigate(_, _, _)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(*invocation.invocation_result_,
              CreateTabValue(kGoodTabWindowId, -1)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  invocation.Execute(good_url, kRequestId);

  // Successful SelectTab.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*mock_window_executor_, SelectTab(kGoodTabWindowHandle)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(*invocation.invocation_result_,
              CreateTabValue(kGoodTabWindowId, -1)).WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  invocation.Execute(selected_true, kRequestId);
}

class MockRemoveTab : public StrictMock<MockApiInvocation<TabApiResult,
                                                          MockTabApiResult,
                                                          RemoveTab> > {
};

TEST_F(TabApiTests, RemoveTabExecute) {
  testing::LogDisabler no_dchecks;

  MockRemoveTab invocation;
  ListValue list_args;

  // Bad arguments.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  invocation.Execute(list_args, kRequestId);

  // Not a IeServerClass.
  invocation.AllocateNewResult(kRequestId);

  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, IsWindowClass(_, _)).WillOnce(Return(false));

  list_args.Append(Value::CreateIntegerValue(kGoodTabWindowId));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(list_args, kRequestId);

  // Fail to get the executor.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(window_utils, IsWindowClass(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(window_utils, GetTopLevelParent(kGoodTabWindow)).
      WillRepeatedly(Return(kGoodFrameWindow));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
              GetExecutor(kGoodFrameWindow, _, NotNull())).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(list_args, kRequestId);

  // Failing executor.
  invocation.AllocateNewResult(kRequestId);
  AlwaysMockGetWindowExecutor(&invocation.mock_api_dispatcher_,
      kGoodFrameWindow);
  EXPECT_CALL(*mock_window_executor_, RemoveTab(kGoodTabWindowHandle)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(list_args, kRequestId);

  // Success.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*mock_window_executor_, RemoveTab(kGoodTabWindowHandle)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(invocation.mock_api_dispatcher_, RegisterEphemeralEventHandler(
    StrEq(ext_event_names::kOnTabRemoved),
    RemoveTab::ContinueExecution,
    invocation.invocation_result_.get()));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  // This will cause the invocation result to be released at the end of the
  // test since the success case purposely does not delete it.
  scoped_ptr<ApiDispatcher::InvocationResult> result(
      invocation.invocation_result_.get());
  invocation.Execute(list_args, kRequestId);
}

TEST_F(TabApiTests, RemoveTabContinueExecution) {
  testing::LogDisabler no_dchecks;

  // Failure cases.
  MockRemoveTab invocation;

  ListValue list_value;
  list_value.Append(Value::CreateIntegerValue(kGoodTabWindowId));
  std::string input_args;
  base::JSONWriter::Write(&list_value, false, &input_args);

  // Bad arguments.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  HRESULT hr = invocation.CallContinueExecution("");
  EXPECT_HRESULT_FAILED(hr);

  // Mismatched tab IDs.
  invocation.AllocateNewResult(kRequestId);
  invocation.invocation_result_->SetValue(
      ext::kTabIdKey, Value::CreateIntegerValue(1234));
  hr = invocation.CallContinueExecution(input_args);
  EXPECT_HRESULT_SUCCEEDED(hr);

  // Success.
  invocation.AllocateNewResult(kRequestId);
  invocation.invocation_result_->SetValue(
      ext::kTabIdKey, Value::CreateIntegerValue(kGoodTabWindowId));
  EXPECT_CALL(*invocation.invocation_result_, PostResult());
  hr = invocation.CallContinueExecution(input_args);
  EXPECT_HRESULT_SUCCEEDED(hr);
}

// Mock the CoCreateInstance call that is used to create a new IE window.
MOCK_STATIC_CLASS_BEGIN(MockIeUtil)
  MOCK_STATIC_INIT_BEGIN(MockIeUtil)
    MOCK_STATIC_INIT2(ie_util::GetWebBrowserForTopLevelIeHwnd,
                      GetWebBrowserForTopLevelIeHwnd);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC3(HRESULT, , GetWebBrowserForTopLevelIeHwnd,
               HWND, IWebBrowser2*, IWebBrowser2**);
MOCK_STATIC_CLASS_END(MockIeUtil)

// Mock static calls to TabApiResult.
MOCK_STATIC_CLASS_BEGIN(MockStaticTabApiResult)
  MOCK_STATIC_INIT_BEGIN(MockStaticTabApiResult)
    MOCK_STATIC_INIT2(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow,
                      IsTabFromSameOrUnspecifiedFrameWindow);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC4(HRESULT, , IsTabFromSameOrUnspecifiedFrameWindow,
               const DictionaryValue&, const Value*, HWND*, ApiDispatcher*);
MOCK_STATIC_CLASS_END(MockStaticTabApiResult)

// TODO(mad@chromium.org): Test the asynchronicity and the event handling.
class MockCreateTab : public StrictMock<MockApiInvocation<TabApiResult,
                                                          MockTabApiResult,
                                                          CreateTab> > {
};

TEST_F(TabApiTests, CreateTabExecute) {
  testing::LogDisabler no_dchecks;

  // Failure cases.
  MockCreateTab invocation;

  ListValue list_value;
  DictionaryValue* list_dict = new DictionaryValue;
  // list_dict will be freed by ListValue, yet we need to keep it around
  // to set values into it.
  list_value.Append(list_dict);

  // No Frame window.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
      GetSpecifiedOrCurrentFrameWindow(_, _)).WillOnce(Return(
          static_cast<HWND>(NULL)));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  invocation.Execute(list_value, kRequestId);

  // Can't get Executor/IWebBrowser2.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              GetSpecifiedOrCurrentFrameWindow(_, _)).
      WillRepeatedly(Return(kGoodFrameWindow));
  StrictMock<MockIeUtil> mock_ie_util;
  StrictMock<testing::MockWindowUtils> mock_window_utils;
  bool pre_vista = base::win::GetVersion() < base::win::VERSION_VISTA;
  if (pre_vista) {
    EXPECT_CALL(mock_ie_util, GetWebBrowserForTopLevelIeHwnd(
        kGoodFrameWindow, _, NotNull())).WillOnce(Return(E_FAIL));
  } else {
    EXPECT_CALL(*executors_manager_, FindTabChildImpl(kGoodFrameWindow)).
        WillRepeatedly(Return(kGoodTabWindow));
    EXPECT_CALL(invocation.mock_api_dispatcher_,
        GetExecutor(kGoodTabWindow, _, _)).
            WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  }
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  invocation.Execute(list_value, kRequestId);

  // Navigate Fails.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              GetSpecifiedOrCurrentFrameWindow(_, _)).
      WillOnce(Return(kGoodFrameWindow));
  CComObject<StrictMock<testing::MockIWebBrowser2>>* browser;
  CComObject<StrictMock<testing::MockIWebBrowser2>>::CreateInstance(
      &browser);
  DCHECK(browser != NULL);
  base::win::ScopedComPtr<IWebBrowser2> browser_keeper(browser);
  if (pre_vista) {
    EXPECT_CALL(mock_ie_util, GetWebBrowserForTopLevelIeHwnd(
        kGoodFrameWindow, _, NotNull())).WillRepeatedly(DoAll(
            CopyInterfaceToArgument<2>(browser_keeper.get()), Return(S_OK)));
    EXPECT_CALL(*browser, Navigate(_, _, _, _, _)).WillOnce(Return(E_FAIL));
  } else {
    AlwaysMockGetTabExecutor(&invocation.mock_api_dispatcher_, kGoodTabWindow);
    EXPECT_CALL(*mock_tab_executor_, Navigate(_, _, _)).
        WillOnce(Return(E_FAIL));
  }
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  invocation.Execute(list_value, kRequestId);

  // Success!
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              GetSpecifiedOrCurrentFrameWindow(_, _)).
      WillOnce(Return(kGoodFrameWindow));
  if (pre_vista) {
    EXPECT_CALL(*browser, Navigate(_, _, _, _, _)).WillOnce(Return(S_OK));
  } else {
    EXPECT_CALL(*mock_tab_executor_, Navigate(_, _, _)).
        WillOnce(Return(S_OK));
  }
  EXPECT_CALL(invocation.mock_api_dispatcher_, RegisterEphemeralEventHandler(
      StrEq(ext_event_names::kOnTabCreated),
      CreateTab::ContinueExecution,
      invocation.invocation_result_.get()));

  // This will cause the invocation result to be released at the end of the
  // test since the success case purposely does not delete it.
  scoped_ptr<ApiDispatcher::InvocationResult> result(
      invocation.invocation_result_.get());
  invocation.Execute(list_value, kRequestId);
}

TEST_F(TabApiTests, CreateTabContinueExecution) {
  testing::LogDisabler no_dchecks;

  // Failure cases.
  MockCreateTab invocation;
  const char kUrl[] = "http://url/";
  const int kIndex = 0;

  ListValue list_value;
  DictionaryValue* dict_value = new DictionaryValue;
  list_value.Append(dict_value);
  dict_value->SetInteger(ext::kWindowIdKey, kGoodTabWindowId);
  dict_value->SetString(ext::kUrlKey, kUrl);
  dict_value->SetInteger(ext::kIndexKey, kIndex);

  std::string input_args;
  base::JSONWriter::Write(&list_value, false, &input_args);

  // No input dictionary.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_));
  HRESULT hr = invocation.CallContinueExecution("");
  EXPECT_HRESULT_FAILED(hr);

  // IsTabFromSameOrUnspecifiedFrameWindow returns S_FALSE.
  invocation.AllocateNewResult(kRequestId);
  StrictMock<MockStaticTabApiResult> tab_api_result;
  EXPECT_CALL(tab_api_result,
              IsTabFromSameOrUnspecifiedFrameWindow(_, _, _, NotNull())).
      WillOnce(Return(S_FALSE));

  hr = invocation.CallContinueExecution(input_args);
  EXPECT_HRESULT_SUCCEEDED(hr);

  // Mistmatched URLs.
  invocation.AllocateNewResult(kRequestId);
  invocation.invocation_result_->SetValue(
      ext::kUrlKey, Value::CreateStringValue("http://other/"));

  EXPECT_CALL(tab_api_result,
              IsTabFromSameOrUnspecifiedFrameWindow(_, _, _, NotNull())).
      WillOnce(Return(S_OK));

  hr = invocation.CallContinueExecution(input_args);
  EXPECT_HRESULT_SUCCEEDED(hr);

  // Success with no index.
  invocation.AllocateNewResult(kRequestId);
  invocation.invocation_result_->SetValue(
      ext::kUrlKey, Value::CreateStringValue(kUrl));

  EXPECT_CALL(tab_api_result,
              IsTabFromSameOrUnspecifiedFrameWindow(_, _, _, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<2>(kGoodTabWindow), Return(S_OK)));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabIdFromHandle(kGoodTabWindow)).WillOnce(Return(kGoodTabWindowId));

  EXPECT_CALL(*invocation.invocation_result_,
              CreateTabValue(kGoodTabWindowId, -1)).
      WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult());

  hr = invocation.CallContinueExecution(input_args);
  EXPECT_HRESULT_SUCCEEDED(hr);

  // Success with an index.
  invocation.AllocateNewResult(kRequestId);
  ApiDispatcher::InvocationResult* result = invocation.invocation_result_.get();
  result->SetValue(ext::kUrlKey, Value::CreateStringValue(kUrl));
  result->SetValue(ext::kIndexKey, Value::CreateIntegerValue(kIndex));

  EXPECT_CALL(tab_api_result,
              IsTabFromSameOrUnspecifiedFrameWindow(_, _, _, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<2>(kGoodTabWindow), Return(S_OK)));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabIdFromHandle(kGoodTabWindow)).WillOnce(Return(kGoodTabWindowId));

  StrictMock<testing::MockWindowUtils> window_utils;
  AlwaysMockGetWindowExecutor(&invocation.mock_api_dispatcher_, kGoodTabWindow);
  EXPECT_CALL(window_utils, GetTopLevelParent(kGoodTabWindow)).
      WillOnce(Return(kGoodTabWindow));

  EXPECT_CALL(*mock_window_executor_,
      MoveTab(reinterpret_cast<CeeeWindowHandle>(kGoodTabWindow), kIndex)).
      WillOnce(Return(S_OK));

  EXPECT_CALL(*invocation.invocation_result_,
              CreateTabValue(kGoodTabWindowId, kIndex)).
      WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult());

  hr = invocation.CallContinueExecution(input_args);
  EXPECT_HRESULT_SUCCEEDED(hr);
}

class MockMoveTab : public StrictMock<MockApiInvocation<TabApiResult,
                                                        MockTabApiResult,
                                                        MoveTab> > {
};

TEST_F(TabApiTests, MoveTab) {
  testing::LogDisabler no_dchecks;

  // Empty list is not valid.
  StrictMock<testing::MockWindowUtils> window_utils;
  MockMoveTab invocation;
  ListValue good_args;
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(good_args, kRequestId);

  // First entry should be an Integer.
  invocation.AllocateNewResult(kRequestId);
  DictionaryValue* good_args_dict = new DictionaryValue();
  good_args.Append(good_args_dict);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(good_args, kRequestId);

  // Good arg format, but not an IE Frame.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_TRUE(good_args.Insert(0, Value::CreateIntegerValue(kGoodTabWindowId)));
  EXPECT_CALL(window_utils, IsWindowClass(kGoodTabWindow, _)).
      WillOnce(Return(false));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(good_args, kRequestId);

  // Wrong second list value.
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(window_utils, IsWindowClass(kGoodTabWindow, _)).
      WillRepeatedly(Return(true));
  EXPECT_TRUE(good_args.Insert(1, Value::CreateNullValue()));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(good_args, kRequestId);

  // Unsupported window id key.
  invocation.AllocateNewResult(kRequestId);
  good_args.Remove(1, NULL);
  good_args_dict->SetInteger(ext::kWindowIdKey, kBadWindowId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(good_args, kRequestId);

  // Wrong format for index key
  invocation.AllocateNewResult(kRequestId);
  good_args_dict->Remove(ext::kWindowIdKey, NULL);
  good_args_dict->SetBoolean(ext::kIndexKey, false);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(good_args, kRequestId);

  // Can't get an executor
  invocation.AllocateNewResult(kRequestId);
  good_args_dict->SetInteger(ext::kIndexKey, kTabIndex);
  EXPECT_CALL(window_utils, GetTopLevelParent(kGoodTabWindow)).
      WillRepeatedly(Return(kGoodFrameWindow));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
              GetExecutor(kGoodFrameWindow, _, _)).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  invocation.Execute(good_args, kRequestId);

  // Get the Executor to fail
  invocation.AllocateNewResult(kRequestId);
  AlwaysMockGetWindowExecutor(&invocation.mock_api_dispatcher_,
                              kGoodFrameWindow);
  EXPECT_CALL(*mock_window_executor_,
      MoveTab(reinterpret_cast<CeeeWindowHandle>(kGoodTabWindow), kTabIndex)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(good_args, kRequestId);

  // Success
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*mock_window_executor_,
      MoveTab(reinterpret_cast<CeeeWindowHandle>(kGoodTabWindow), kTabIndex)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(*invocation.invocation_result_,
              CreateTabValue(kGoodTabWindowId, kTabIndex)).
              WillOnce(Return(true));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.Execute(good_args, kRequestId);
}

class MockTabsInsertCode
    : public StrictMock<MockApiInvocation<TabApiResult, MockTabApiResult,
                                          TabsInsertCode> > {
 public:
  // Overloaded to change the type of the return value for easier testing.
  StrictMock<MockTabApiResult>* CallExecuteImpl(const ListValue& args,
                                                int request_id,
                                                CeeeTabCodeType type,
                                                int* tab_id,
                                                HRESULT* hr) {
    return static_cast<StrictMock<MockTabApiResult>*>(ExecuteImpl(
        args, request_id, type, tab_id, hr));
  }
  // Declare this so that this is not an abstract class.
  virtual void Execute(const ListValue& args, int request_id) {}
};

TEST_F(TabApiTests, TabsInsertCode) {
  testing::LogDisabler no_dchecks;
  StrictMock<testing::MockWindowUtils> window_utils;
  int tab_id;
  HRESULT hr;

  // Empty list is not valid.
  MockTabsInsertCode invocation;
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_EQ(NULL, invocation.CallExecuteImpl(
      ListValue(), kRequestId, kCeeeTabCodeTypeCss, &tab_id, &hr));

  // First parameter should be an Integer.
  ListValue good_args;
  DictionaryValue* good_args_dict = new DictionaryValue();
  good_args.Append(good_args_dict);
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_EQ(NULL, invocation.CallExecuteImpl(
      good_args, kRequestId, kCeeeTabCodeTypeCss, &tab_id, &hr));

  // Good arg format, but no values in dictionary.
  EXPECT_TRUE(good_args.Insert(0, Value::CreateIntegerValue(kGoodTabWindowId)));
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_EQ(NULL, invocation.CallExecuteImpl(
      good_args, kRequestId, kCeeeTabCodeTypeCss, &tab_id, &hr));

  // Good arg format, but both code and file values in dictionary.
  good_args_dict->SetString(ext::kCodeKey, "alert(5);");
  good_args_dict->SetString(ext::kFileKey, "test.js");
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_EQ(NULL, invocation.CallExecuteImpl(
      good_args, kRequestId, kCeeeTabCodeTypeCss, &tab_id, &hr));

  // Good arg format, but not an IE Frame.
  good_args_dict->Remove(ext::kFileKey, NULL);
  EXPECT_CALL(window_utils, IsWindowClass(kGoodTabWindow, _)).
      WillOnce(Return(false));
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  EXPECT_EQ(NULL, invocation.CallExecuteImpl(
      good_args, kRequestId, kCeeeTabCodeTypeCss, &tab_id, &hr));

  // Can't get an executor.
  EXPECT_CALL(window_utils, IsWindowClass(kGoodTabWindow, _)).
      WillRepeatedly(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
              GetExecutor(kGoodTabWindow, _, _)).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, PostError(_)).Times(1);
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  EXPECT_EQ(NULL, invocation.CallExecuteImpl(
      good_args, kRequestId, kCeeeTabCodeTypeCss, &tab_id, &hr));

  // Executor failure, all_frames defaulted to false.
  AlwaysMockGetTabExecutor(&invocation.mock_api_dispatcher_, kGoodTabWindow);
  EXPECT_CALL(*mock_tab_executor_,
              InsertCode(_, _, false, kCeeeTabCodeTypeCss)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.AllocateNewResult(kRequestId);
  scoped_ptr<StrictMock<MockTabApiResult>> result(invocation.CallExecuteImpl(
      good_args, kRequestId, kCeeeTabCodeTypeCss, &tab_id, &hr));
  EXPECT_EQ(kGoodTabWindowId, tab_id);
  EXPECT_EQ(E_FAIL, hr);

  // Success, all_frames defaulted to false.
  AlwaysMockGetTabExecutor(&invocation.mock_api_dispatcher_, kGoodTabWindow);
  EXPECT_CALL(*mock_tab_executor_,
              InsertCode(_, _, false, kCeeeTabCodeTypeCss)).
      WillRepeatedly(Return(S_OK));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.AllocateNewResult(kRequestId);
  result.reset(invocation.CallExecuteImpl(
      good_args, kRequestId, kCeeeTabCodeTypeCss, &tab_id, &hr));
  EXPECT_EQ(tab_id, kGoodTabWindowId);
  EXPECT_HRESULT_SUCCEEDED(hr);

  // Success, set all_frames to true.
  good_args_dict->SetBoolean(ext::kAllFramesKey, true);
  EXPECT_CALL(*mock_tab_executor_, InsertCode(_, _, true,
      kCeeeTabCodeTypeJs)).WillOnce(Return(S_OK));
  EXPECT_CALL(invocation.mock_api_dispatcher_, IsTabIdValid(kGoodTabWindowId)).
      WillOnce(Return(true));
  EXPECT_CALL(invocation.mock_api_dispatcher_,
      GetTabHandleFromId(kGoodTabWindowId)).WillOnce(Return(kGoodTabWindow));
  invocation.AllocateNewResult(kRequestId);
  result.reset(invocation.CallExecuteImpl(
      good_args, kRequestId, kCeeeTabCodeTypeJs, &tab_id, &hr));
  EXPECT_EQ(tab_id, kGoodTabWindowId);
  EXPECT_HRESULT_SUCCEEDED(hr);
}

TEST_F(TabApiTests, IsTabFromSameOrUnspecifiedFrameWindow) {
  // We need a mock for this now.
  StrictMock<MockApiDispatcher> mock_api_dispatcher;

  // We expect these calls repeatedly.
  // TODO(mad@chromium): Add test cases for when they return other values.
  EXPECT_CALL(mock_api_dispatcher, GetTabHandleFromId(kGoodTabWindowId)).
      WillRepeatedly(Return(kGoodTabWindow));
  EXPECT_CALL(mock_api_dispatcher, GetWindowIdFromHandle(kGoodFrameWindow)).
      WillRepeatedly(Return(kGoodFrameWindowId));
  EXPECT_CALL(mock_api_dispatcher, GetWindowHandleFromId(kGoodFrameWindowId)).
      WillRepeatedly(Return(kGoodFrameWindow));
  EXPECT_CALL(mock_api_dispatcher, IsTabIdValid(kGoodTabWindowId)).
      WillRepeatedly(Return(true));

  // We always need a kIdKey value in the input_dict.
  DictionaryValue input_dict;
  input_dict.SetInteger(ext::kIdKey, kGoodTabWindowId);
  // Start with no saved dict, so any input value is good.
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, NULL, NULL, &mock_api_dispatcher), S_OK);
  // Also test that we are properly returned the input value.
  HWND tab_window = NULL;
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, NULL, &tab_window, &mock_api_dispatcher), S_OK);
  EXPECT_EQ(kGoodTabWindow, tab_window);

  // Now check with the same value found as a grand parent.
  FundamentalValue saved_window(kGoodFrameWindowId);
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, GetTopLevelParent(kGoodTabWindow)).
      WillRepeatedly(Return(kGoodFrameWindow));
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, &saved_window, NULL, &mock_api_dispatcher), S_OK);
  tab_window = NULL;
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, &saved_window, &tab_window, &mock_api_dispatcher), S_OK);
  EXPECT_EQ(kGoodTabWindow, tab_window);

  // Now check with the same value provided in the input_dict.
  input_dict.SetInteger(ext::kWindowIdKey, kGoodFrameWindowId);
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, &saved_window, NULL, &mock_api_dispatcher), S_OK);
  tab_window = NULL;
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, &saved_window, &tab_window, &mock_api_dispatcher), S_OK);
  EXPECT_EQ(kGoodTabWindow, tab_window);

  // And now check the cases where they differ.
  FundamentalValue other_saved_window(kGoodFrameWindowId + 1);
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, &other_saved_window, NULL, &mock_api_dispatcher), S_FALSE);
  tab_window = NULL;
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, &other_saved_window, &tab_window, &mock_api_dispatcher),
          S_FALSE);
  EXPECT_EQ(kGoodTabWindow, tab_window);

  input_dict.Remove(ext::kWindowIdKey, NULL);
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, &other_saved_window, NULL, &mock_api_dispatcher), S_FALSE);
  tab_window = NULL;
  EXPECT_EQ(TabApiResult::IsTabFromSameOrUnspecifiedFrameWindow(
      input_dict, &other_saved_window, &tab_window, &mock_api_dispatcher),
          S_FALSE);
  EXPECT_EQ(kGoodTabWindow, tab_window);
}

}  // namespace
