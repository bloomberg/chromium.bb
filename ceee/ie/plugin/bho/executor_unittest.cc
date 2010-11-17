// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Executor implementation unit tests.

// MockWin32 must not be included after atlwin, which is included by some
// headers in here, so we need to put it at the top:
#include "ceee/testing/utils/mock_win32.h"  // NOLINT

#include <wininet.h>

#include <vector>

#include "ceee/ie/broker/cookie_api_module.h"
#include "ceee/ie/broker/common_api_module.h"
#include "ceee/ie/broker/tab_api_module.h"
#include "ceee/ie/broker/window_api_module.h"
#include "ceee/ie/common/ie_util.h"
#include "ceee/ie/common/mock_ie_tab_interfaces.h"
#include "ceee/ie/plugin/bho/executor.h"
#include "ceee/ie/plugin/bho/infobar_manager.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/ie/testing/mock_frame_event_handler_host.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/mshtml_mocks.h"
#include "ceee/testing/utils/mock_static.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "broker_lib.h"  // NOLINT

namespace {

using testing::_;
using testing::AddRef;
using testing::CopyInterfaceToArgument;
using testing::CopyBSTRToArgument;
using testing::InstanceCountMixin;
using testing::Invoke;
using testing::IsNull;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::WithoutArgs;

const int kGoodWindowId = 42;
const HWND kGoodWindow = reinterpret_cast<HWND>(kGoodWindowId);
const int kOtherGoodWindowId = 84;
const HWND kOtherGoodWindow = reinterpret_cast<HWND>(kOtherGoodWindowId);
const int kTabIndex = 26;
const int kOtherTabIndex = 62;

const wchar_t* kUrl1 = L"http://www.google.com";
const wchar_t* kUrl2 = L"http://myintranet";
const wchar_t* kTitle1 = L"MyLord";
const wchar_t* kTitle2 = L"Your MADness";

class TestingMockExecutorCreatorTeardown
    : public CeeeExecutorCreator,
      public InitializingCoClass<
          StrictMock<TestingMockExecutorCreatorTeardown>> {
 public:
  // TODO(mad@chromium.org): Add reference counting testing/validation.
  TestingMockExecutorCreatorTeardown() {
    hook_ = reinterpret_cast<HHOOK>(1);
    current_thread_id_ = 42L;
  }
  HRESULT Initialize(StrictMock<TestingMockExecutorCreatorTeardown>** self) {
    // Yes, this seems fishy, but it is called from InitializingCoClass
    // which does it on the class we pass it as a template, so we are OK.
    *self = static_cast<StrictMock<TestingMockExecutorCreatorTeardown>*>(this);
    return S_OK;
  }

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, Teardown, HRESULT(long));
};

class MockInfobarManager : public infobar_api::InfobarManager {
 public:
  MockInfobarManager() : InfobarManager(kGoodWindow) {}
  MOCK_METHOD4(Show, HRESULT(infobar_api::InfobarType, int,
                             const std::wstring&, bool));
  MOCK_METHOD1(Hide, HRESULT(infobar_api::InfobarType));
  MOCK_METHOD0(HideAll, void());
};

TEST(ExecutorCreator, ProperTearDownOnDestruction) {
  StrictMock<TestingMockExecutorCreatorTeardown>* executor_creator = NULL;
  CComPtr<ICeeeExecutorCreator> executor_creator_keeper;
  ASSERT_HRESULT_SUCCEEDED(StrictMock<TestingMockExecutorCreatorTeardown>::
      CreateInitialized(&executor_creator, &executor_creator_keeper));
  EXPECT_CALL(*executor_creator, Teardown(42L)).WillOnce(Return(S_OK));
  // The Release of the last reference should call FinalRelease.
}

// Mock object for some functions used for hooking.
MOCK_STATIC_CLASS_BEGIN(MockHooking)
  MOCK_STATIC_INIT_BEGIN(MockHooking)
    MOCK_STATIC_INIT(SetWindowsHookEx);
    MOCK_STATIC_INIT(UnhookWindowsHookEx);
    MOCK_STATIC_INIT(PostThreadMessage);
    MOCK_STATIC_INIT(PeekMessage);
    MOCK_STATIC_INIT(CallNextHookEx);
    MOCK_STATIC_INIT(CoCreateInstance);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC4(HHOOK, CALLBACK, SetWindowsHookEx, int, HOOKPROC, HINSTANCE,
               DWORD);
  MOCK_STATIC1(BOOL, CALLBACK, UnhookWindowsHookEx, HHOOK);
  MOCK_STATIC4(BOOL, CALLBACK, PostThreadMessage, DWORD, UINT, WPARAM, LPARAM);
  MOCK_STATIC5(BOOL, CALLBACK, PeekMessage, LPMSG, HWND, UINT, UINT, UINT);
  MOCK_STATIC4(LRESULT, CALLBACK, CallNextHookEx, HHOOK, int, WPARAM, LPARAM);
  MOCK_STATIC5(HRESULT, CALLBACK, CoCreateInstance, REFCLSID, LPUNKNOWN,
               DWORD, REFIID, LPVOID*);
MOCK_STATIC_CLASS_END(MockHooking)

class TestingExecutorCreator : public CeeeExecutorCreator {
 public:
  // TODO(mad@chromium.org): Add reference counting testing/validation.
  STDMETHOD_(ULONG, AddRef)() { return 1; }
  STDMETHOD_(ULONG, Release)() { return 1; }
  STDMETHOD (QueryInterface)(REFIID, LPVOID*) { return S_OK; }

  // Accessorize... :-)
  long current_thread_id() const { return current_thread_id_; }
  void set_current_thread_id(long thread_id) { current_thread_id_ = thread_id; }
  HHOOK hook() const { return hook_; }
  void set_hook(HHOOK hook) { hook_ = hook; }

  // Publicize...
  using CeeeExecutorCreator::kCreateWindowExecutorMessage;
  using CeeeExecutorCreator::GetMsgProc;
};

template<int error_code>
void SetFakeLastError() {
  ::SetLastError(error_code);
}

TEST(ExecutorCreator, CreateExecutor) {
  testing::LogDisabler no_dchecks;

  // Start with hooking failure.
  StrictMock<MockHooking> mock_hooking;
  EXPECT_CALL(mock_hooking, SetWindowsHookEx(WH_GETMESSAGE, _, _, 42L)).
      WillOnce(Return(static_cast<HHOOK>(NULL)));
  TestingExecutorCreator executor_creator;
  EXPECT_HRESULT_FAILED(executor_creator.CreateWindowExecutor(42L, 42L));
  EXPECT_EQ(0L, executor_creator.current_thread_id());
  EXPECT_EQ(NULL, executor_creator.hook());

  // Then succeed hooking but fail message posting.
  EXPECT_CALL(mock_hooking, SetWindowsHookEx(WH_GETMESSAGE, _, _, 42L)).
      WillRepeatedly(Return(reinterpret_cast<HHOOK>(1)));
  EXPECT_CALL(mock_hooking, PostThreadMessage(42L, _, 0, 0)).
      WillOnce(DoAll(WithoutArgs(Invoke(
          SetFakeLastError<ERROR_INVALID_ACCESS>)), Return(FALSE)));
  EXPECT_HRESULT_FAILED(executor_creator.CreateWindowExecutor(42L, 0L));
  ::SetLastError(ERROR_SUCCESS);

  // Success!!!
  EXPECT_CALL(mock_hooking, PostThreadMessage(42L, _, 0, 0)).
      WillRepeatedly(Return(TRUE));
  EXPECT_HRESULT_SUCCEEDED(executor_creator.CreateWindowExecutor(42L, 0L));
}

TEST(ExecutorCreator, Teardown) {
  testing::LogDisabler no_dchecks;

  // Start with nothing to do.
  TestingExecutorCreator executor_creator;
  EXPECT_HRESULT_SUCCEEDED(executor_creator.Teardown(0));

  // OK check that we properly unhook now...
  StrictMock<MockHooking> mock_hooking;
  HHOOK fake_hook = reinterpret_cast<HHOOK>(1);
  EXPECT_CALL(mock_hooking, UnhookWindowsHookEx(fake_hook)).
      WillOnce(Return(TRUE));
  executor_creator.set_current_thread_id(42L);
  executor_creator.set_hook(fake_hook);
  EXPECT_HRESULT_SUCCEEDED(executor_creator.Teardown(42L));
  EXPECT_EQ(0L, executor_creator.current_thread_id());
  EXPECT_EQ(NULL, executor_creator.hook());
}

class ExecutorCreatorTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(testing::MockWindowExecutor::CreateInitialized(
        &executor_, &executor_keeper_));
    ASSERT_HRESULT_SUCCEEDED(testing::MockBroker::CreateInitialized(
        &broker_, &broker_keeper_));
  }

  virtual void TearDown() {
    executor_ = NULL;
    executor_keeper_.Release();

    broker_ = NULL;
    broker_keeper_.Release();

    // Everything should have been relinquished.
    ASSERT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
  }

 protected:
  testing::MockWindowExecutor* executor_;
  CComPtr<ICeeeWindowExecutor> executor_keeper_;

  testing::MockBroker* broker_;
  CComPtr<ICeeeBrokerRegistrar> broker_keeper_;
};

TEST_F(ExecutorCreatorTest, GetMsgProc) {
  testing::LogDisabler no_dchecks;

  // Start with nothing to do.
  StrictMock<MockHooking> mock_hooking;
  EXPECT_CALL(mock_hooking, CallNextHookEx(_, _, _, _)).WillOnce(Return(0));

  TestingExecutorCreator executor_creator;
  EXPECT_EQ(0, executor_creator.GetMsgProc(HC_SKIP, 0, 0));

  // NULL message.
  EXPECT_CALL(mock_hooking, CallNextHookEx(_, _, _, _)).WillOnce(Return(0));
  EXPECT_EQ(0, executor_creator.GetMsgProc(HC_ACTION, 0, 0));

  // Not our message.
  MSG message;
  message.message = WM_TIMER;
  EXPECT_CALL(mock_hooking, CallNextHookEx(_, _, _, _)).WillOnce(Return(0));
  EXPECT_EQ(0, executor_creator.GetMsgProc(HC_ACTION, 0,
      reinterpret_cast<LPARAM>(&message)));

  // OK check our own code paths now.
  message.message = executor_creator.kCreateWindowExecutorMessage;
  EXPECT_CALL(mock_hooking, CallNextHookEx(_, _, _, _)).Times(0);

  // Not a PM_REMOVE message, delegates to PeekMessage.
  EXPECT_CALL(mock_hooking, PeekMessage(_, _, _, _, _)).WillOnce(Return(TRUE));
  EXPECT_EQ(0, executor_creator.GetMsgProc(HC_ACTION, 0,
      reinterpret_cast<LPARAM>(&message)));

  // With a PM_REMOVE, we get the job done.
  // But lets see if we can silently handle a CoCreateInstance Failure
  EXPECT_CALL(mock_hooking, CoCreateInstance(_, _, _, _, _)).
      WillOnce(Return(REGDB_E_CLASSNOTREG));
  EXPECT_EQ(0, executor_creator.GetMsgProc(HC_ACTION, PM_REMOVE,
      reinterpret_cast<LPARAM>(&message)));

  // Now fail getting the broker registrar.
  message.lParam = reinterpret_cast<LPARAM>(kGoodWindow);
  EXPECT_CALL(*executor_, Initialize(_)).WillOnce(Return(S_OK));
  EXPECT_CALL(mock_hooking, CoCreateInstance(_, _, _, _, _)).
      WillOnce(DoAll(SetArgumentPointee<4>(executor_keeper_.p),
                     AddRef(executor_keeper_.p), Return(S_OK))).
      WillOnce(Return(REGDB_E_CLASSNOTREG));
  EXPECT_EQ(0, executor_creator.GetMsgProc(HC_ACTION, PM_REMOVE,
      reinterpret_cast<LPARAM>(&message)));

  // Now fail the registration itself.
  message.lParam = reinterpret_cast<LPARAM>(kGoodWindow);
  EXPECT_CALL(mock_hooking, CoCreateInstance(_, _, _, _, _)).
      WillOnce(DoAll(SetArgumentPointee<4>(executor_keeper_.p),
                     AddRef(executor_keeper_.p), Return(S_OK))).
      WillOnce(DoAll(SetArgumentPointee<4>(broker_keeper_.p),
                     AddRef(broker_keeper_.p), Return(S_OK)));
  EXPECT_CALL(*executor_, Initialize(_)).WillOnce(Return(S_OK));
  DWORD current_thread_id = ::GetCurrentThreadId();
  EXPECT_CALL(*broker_, RegisterWindowExecutor(current_thread_id,
      executor_keeper_.p)).WillOnce(Return(E_FAIL));
  EXPECT_EQ(0, executor_creator.GetMsgProc(HC_ACTION, PM_REMOVE,
      reinterpret_cast<LPARAM>(&message)));

  // Success!!!
  message.lParam = reinterpret_cast<LPARAM>(kGoodWindow);
  EXPECT_CALL(mock_hooking, CoCreateInstance(_, _, _, _, _)).
      WillOnce(DoAll(SetArgumentPointee<4>(executor_keeper_.p),
                     AddRef(executor_keeper_.p), Return(S_OK))).
      WillOnce(DoAll(SetArgumentPointee<4>(broker_keeper_.p),
                     AddRef(broker_keeper_.p), Return(S_OK)));
  EXPECT_CALL(*executor_, Initialize(_)).WillOnce(Return(S_OK));
  EXPECT_CALL(*broker_, RegisterWindowExecutor(current_thread_id,
      executor_keeper_.p)).WillOnce(Return(S_OK));
  EXPECT_EQ(0, executor_creator.GetMsgProc(HC_ACTION, PM_REMOVE,
      reinterpret_cast<LPARAM>(&message)));
}

MOCK_STATIC_CLASS_BEGIN(MockWinInet)
  MOCK_STATIC_INIT_BEGIN(MockWinInet)
    MOCK_STATIC_INIT(InternetGetCookieExW);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC6(BOOL, CALLBACK, InternetGetCookieExW, LPCWSTR, LPCWSTR, LPWSTR,
               LPDWORD, DWORD, LPVOID);
MOCK_STATIC_CLASS_END(MockWinInet)


// Mock object for some functions used for hooking.
MOCK_STATIC_CLASS_BEGIN(MockIeUtil)
  MOCK_STATIC_INIT_BEGIN(MockIeUtil)
    MOCK_STATIC_INIT2(ie_util::GetIeVersion, GetIeVersion);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC0(ie_util::IeVersion, , GetIeVersion);
MOCK_STATIC_CLASS_END(MockIeUtil)

class TestingExecutor
    : public CeeeExecutor,
      public InitializingCoClass<TestingExecutor> {
 public:
  HRESULT Initialize(TestingExecutor** self) {
    *self = this;
    return S_OK;
  }
  static void set_tab_windows(std::vector<HWND> tab_windows) {
    tab_windows_ = tab_windows;
  }
  void set_id(HWND hwnd) {
    hwnd_ = hwnd;
  }
  void InitializeInfobarManager(MockInfobarManager** manager) {
    ASSERT_TRUE(manager != NULL);
    *manager = new MockInfobarManager();
    infobar_manager_.reset(*manager);
  }
  void TerminateInfobarManager() {
    infobar_manager_.reset(NULL);
  }
  static BOOL MockEnumChildWindows(HWND, WNDENUMPROC, LPARAM p) {
    std::vector<HWND>* tab_windows = reinterpret_cast<std::vector<HWND>*>(p);
    *tab_windows = tab_windows_;
    return TRUE;
  }
  static void set_cookie_data(const std::wstring& cookie_data) {
    cookie_data_ = cookie_data;
  }
  static BOOL MockInternetGetCookieExW(LPCWSTR, LPCWSTR, LPWSTR data,
                                       LPDWORD size, DWORD, LPVOID) {
    EXPECT_TRUE(data != NULL);
    EXPECT_TRUE(*size > cookie_data_.size());
    wcscpy_s(data, *size, cookie_data_.data());
    *size = cookie_data_.size() + 1;
    return TRUE;
  }

  MOCK_METHOD1(GetWebBrowser, HRESULT(IWebBrowser2** browser));
  HRESULT CallGetWebBrowser(IWebBrowser2** browser) {
    return CeeeExecutor::GetWebBrowser(browser);
  }

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetTabs, HRESULT(BSTR*));
  HRESULT CallGetTabs(BSTR* tab_list) {
    return CeeeExecutor::GetTabs(tab_list);
  }

  MOCK_METHOD3(GetCookieValue, HRESULT(BSTR, BSTR, BSTR*));
  HRESULT CallGetCookieValue(BSTR url, BSTR name, BSTR* value) {
    return CeeeExecutor::GetCookieValue(url, name, value);
  }

  // Publicize...
  using CeeeExecutor::GetTabsEnumProc;
  using CeeeExecutor::MoveTab;
  using CeeeExecutor::set_cookie_store_is_registered;
 private:
  static std::vector<HWND> tab_windows_;
  static std::wstring cookie_data_;
};
std::vector<HWND> TestingExecutor::tab_windows_;
std::wstring TestingExecutor::cookie_data_;

// Override to handle DISPID_READYSTATE.
class ExecutorTests: public testing::Test {
 public:
  void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(TestingExecutor::CreateInitialized(
        &executor_, &executor_keeper_));

    browser_ = NULL;
    manager_ = NULL;
  }

  void MockBrowser() {
    CComObject<StrictMock<testing::MockIWebBrowser2>>::CreateInstance(
        &browser_);
    DCHECK(browser_ != NULL);
    browser_keeper_ = browser_;
    EXPECT_CALL(*executor_, GetWebBrowser(NotNull())).
        WillRepeatedly(DoAll(CopyInterfaceToArgument<0>(browser_keeper_),
                             Return(S_OK)));
  }

  void MockSite() {
    ASSERT_HRESULT_SUCCEEDED(
        testing::MockFrameEventHandlerHost::CreateInitializedIID(
            &mock_site_, IID_IUnknown, &mock_site_keeper_));
    executor_->SetSite(mock_site_keeper_);
  }

  void MockTabManager() {
    CComObject<StrictMock<testing::MockITabWindowManagerIe7>>::CreateInstance(
        &manager_);
    DCHECK(manager_ != NULL);
    manager_keeper_ = manager_;
    EXPECT_CALL(ie_tab_interfaces_, TabWindowManagerFromFrame(_, _, _)).
        WillRepeatedly(DoAll(AddRef(manager_keeper_.p), SetArgumentPointee<2>(
            reinterpret_cast<void*>(manager_keeper_.p)), Return(S_OK)));

    EXPECT_CALL(*manager_, IndexFromHWND(_, _)).WillRepeatedly(DoAll(
        SetArgumentPointee<1>(kTabIndex), Return(S_OK)));
  }

  void NotRunningInThisWindowThread(HWND window) {
    EXPECT_CALL(mock_window_utils_, IsWindowThread(window)).
        WillOnce(Return(false));
  }

  void RepeatedlyRunningInThisWindowThread(HWND window) {
    EXPECT_CALL(mock_window_utils_, IsWindowThread(window)).
        WillRepeatedly(Return(true));
  }

  void RepeatedlyRunningInThisParentWindowThread(HWND child_window,
                                                 HWND parent_window) {
    EXPECT_CALL(mock_window_utils_, GetTopLevelParent(child_window)).
        WillRepeatedly(Return(parent_window));
    EXPECT_CALL(mock_window_utils_, IsWindowThread(parent_window)).
        WillRepeatedly(Return(true));
  }
 protected:
  TestingExecutor* executor_;
  StrictMock<testing::MockWindowUtils> mock_window_utils_;


  // TODO(mad@chromium.org): We should standardize on the Mock COM
  // objects creation.
  //            Using InitializingCoClass would probably be better.
  CComObject<StrictMock<testing::MockIWebBrowser2>>* browser_;
  CComPtr<IWebBrowser2> browser_keeper_;
  CComObject<StrictMock<testing::MockITabWindowManagerIe7>>* manager_;

  testing::MockFrameEventHandlerHost* mock_site_;

  StrictMock<testing::MockUser32> user32_;
  StrictMock<testing::MockIeTabInterfaces> ie_tab_interfaces_;

 private:
  CComPtr<IUnknown> executor_keeper_;
  CComPtr<IUnknown> mock_site_keeper_;
  CComPtr<ITabWindowManagerIe7> manager_keeper_;
};

TEST_F(ExecutorTests, GetWebBrowser) {
  MockSite();
  CComPtr<IWebBrowser2> browser;
  {
    EXPECT_CALL(*mock_site_, GetTopLevelBrowser(NotNull())).
        WillOnce(Return(E_FAIL));
    EXPECT_HRESULT_FAILED(executor_->CallGetWebBrowser(&browser));
  }
  EXPECT_CALL(*mock_site_, GetTopLevelBrowser(NotNull())).
      WillOnce(DoAll(SetArgumentPointee<0>(browser_), Return(S_OK)));
  EXPECT_HRESULT_SUCCEEDED(executor_->CallGetWebBrowser(&browser));
  EXPECT_EQ(browser, browser.p);
}

TEST_F(ExecutorTests, GetWindow) {
  testing::LogDisabler no_dchecks;
  executor_->set_id(kGoodWindow);

  // Not running in appropriate thread.
  NotRunningInThisWindowThread(kGoodWindow);
  EXPECT_HRESULT_FAILED(executor_->GetWindow(FALSE, NULL));

  // Fail getting the window RECT.
  RepeatedlyRunningInThisWindowThread(kGoodWindow);
  EXPECT_CALL(user32_, GetForegroundWindow()).
      WillRepeatedly(Return(kGoodWindow));
  EXPECT_CALL(mock_window_utils_, GetTopLevelParent(kGoodWindow)).
      WillRepeatedly(Return(kGoodWindow));
  EXPECT_CALL(user32_, GetWindowRect(kGoodWindow, NotNull())).
      WillOnce(DoAll(WithoutArgs(Invoke(
          SetFakeLastError<ERROR_INVALID_ACCESS>)), Return(FALSE)));
  common_api::WindowInfo window_info;
  EXPECT_HRESULT_FAILED(executor_->GetWindow(FALSE, &window_info));
  EXPECT_EQ(NULL, window_info.tab_list);
  ::SetLastError(ERROR_SUCCESS);

  // Success without tab population.
  RECT window_rect = {1, 2, 3, 5};
  EXPECT_CALL(user32_, GetWindowRect(kGoodWindow, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(window_rect), Return(TRUE)));
  EXPECT_HRESULT_SUCCEEDED(executor_->GetWindow(FALSE, &window_info));
  EXPECT_EQ(TRUE, window_info.focused);
  EXPECT_EQ(window_rect.top, window_info.rect.top);
  EXPECT_EQ(window_rect.left, window_info.rect.left);
  EXPECT_EQ(window_rect.bottom, window_info.rect.bottom);
  EXPECT_EQ(window_rect.right, window_info.rect.right);
  EXPECT_EQ(NULL, window_info.tab_list);

  // Try the not focused case and a bigger rect.
  window_rect.left = 8;
  window_rect.top = 13;
  window_rect.right = 21;
  window_rect.bottom = 34;
  EXPECT_CALL(user32_, GetWindowRect(kOtherGoodWindow, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(window_rect), Return(TRUE)));
  // Different parent, means we are not focused.
  EXPECT_CALL(mock_window_utils_, GetTopLevelParent(kOtherGoodWindow)).
      WillRepeatedly(Return(kGoodWindow));
  RepeatedlyRunningInThisWindowThread(kOtherGoodWindow);
  executor_->set_id(kOtherGoodWindow);
  EXPECT_HRESULT_SUCCEEDED(executor_->GetWindow(FALSE, &window_info));
  EXPECT_EQ(FALSE, window_info.focused);
  EXPECT_EQ(window_rect.top, window_info.rect.top);
  EXPECT_EQ(window_rect.left, window_info.rect.left);
  EXPECT_EQ(window_rect.bottom, window_info.rect.bottom);
  EXPECT_EQ(window_rect.right, window_info.rect.right);
  EXPECT_EQ(NULL, window_info.tab_list);

  // Fail with tab population. We'll test tab population with GetTabs later.
  // GetTabs will fail but at least we confirm that it gets called :-)...
  EXPECT_CALL(*executor_, GetTabs(NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->GetWindow(TRUE, &window_info));
  EXPECT_EQ(NULL, window_info.tab_list);
}

TEST_F(ExecutorTests, GetTabsEnumProc) {
  std::vector<HWND> tab_windows;
  HWND bad_window1 = reinterpret_cast<HWND>(666);
  HWND bad_window2 = reinterpret_cast<HWND>(999);
  EXPECT_CALL(mock_window_utils_, IsWindowClass(kGoodWindow, _)).
      WillOnce(Return(true));
  EXPECT_CALL(mock_window_utils_, IsWindowClass(kOtherGoodWindow, _)).
      WillOnce(Return(true));
  EXPECT_CALL(mock_window_utils_, IsWindowClass(bad_window1, _)).
      WillOnce(Return(false));
  EXPECT_CALL(mock_window_utils_, IsWindowClass(bad_window2, _)).
      WillOnce(Return(false));
  LPARAM param = reinterpret_cast<LPARAM>(&tab_windows);
  EXPECT_TRUE(TestingExecutor::GetTabsEnumProc(kGoodWindow, param));
  EXPECT_TRUE(TestingExecutor::GetTabsEnumProc(bad_window1, param));
  EXPECT_TRUE(TestingExecutor::GetTabsEnumProc(kOtherGoodWindow, param));
  EXPECT_TRUE(TestingExecutor::GetTabsEnumProc(bad_window2, param));
  EXPECT_EQ(2, tab_windows.size());
  EXPECT_EQ(kGoodWindow, tab_windows[0]);
  EXPECT_EQ(kOtherGoodWindow, tab_windows[1]);
}

TEST_F(ExecutorTests, GetTabs) {
  testing::LogDisabler no_dchecks;
  executor_->set_id(kGoodWindow);

  // Not running in appropriate thread.
  NotRunningInThisWindowThread(kGoodWindow);
  EXPECT_HRESULT_FAILED(executor_->CallGetTabs(NULL));

  // We already tested the case where we don't have a tab manager.
  RepeatedlyRunningInThisWindowThread(kGoodWindow);
  MockTabManager();
  EXPECT_CALL(user32_, EnumChildWindows(kGoodWindow, _, _)).
      WillRepeatedly(Invoke(TestingExecutor::MockEnumChildWindows));

  // No tabs case.
  CComBSTR result;
  EXPECT_HRESULT_SUCCEEDED(executor_->CallGetTabs(&result));
  EXPECT_STREQ(L"[]", result.m_str);
  result.Empty();

  static const int kBadWindowId = 21;
  static const HWND kBadWindow = reinterpret_cast<HWND>(kBadWindowId);

  // Fail to get a tab index.
  EXPECT_CALL(*manager_, IndexFromHWND(kGoodWindow, NotNull())).
      WillOnce(Return(E_FAIL));
  std::vector<HWND> tab_windows;
  tab_windows.push_back(kGoodWindow);
  EXPECT_CALL(user32_, IsWindow(kGoodWindow)).WillOnce(Return(TRUE));
  executor_->set_tab_windows(tab_windows);
  EXPECT_HRESULT_FAILED(executor_->CallGetTabs(&result));

  // Successfully return 1 tab.
  EXPECT_CALL(*manager_, IndexFromHWND(kGoodWindow, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(kTabIndex), Return(S_OK)));
  EXPECT_HRESULT_SUCCEEDED(executor_->CallGetTabs(&result));
  wchar_t expected_result[16];
  wnsprintf(expected_result, 16, L"[%d,%d]", kGoodWindowId, kTabIndex);
  EXPECT_STREQ(expected_result, result.m_str);
  result.Empty();

  // Successfully return 2 tabs.
  EXPECT_CALL(*manager_, IndexFromHWND(kGoodWindow, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(kTabIndex), Return(S_OK)));
  EXPECT_CALL(*manager_, IndexFromHWND(kOtherGoodWindow, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(kOtherTabIndex), Return(S_OK)));
  tab_windows.push_back(kOtherGoodWindow);
  executor_->set_tab_windows(tab_windows);
  EXPECT_HRESULT_SUCCEEDED(executor_->CallGetTabs(&result));
  wnsprintf(expected_result, 16, L"[%d,%d,%d,%d]", kGoodWindowId, kTabIndex,
      kOtherGoodWindowId, kOtherTabIndex);
  EXPECT_STREQ(expected_result, result.m_str);

  // Successfully return 2 out of 3 tabs.
  EXPECT_CALL(*manager_, IndexFromHWND(kGoodWindow, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(kTabIndex), Return(S_OK)));
  EXPECT_CALL(*manager_, IndexFromHWND(kOtherGoodWindow, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(kOtherTabIndex), Return(S_OK)));
  EXPECT_CALL(*manager_, IndexFromHWND(kBadWindow, NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(user32_, IsWindow(kBadWindow)).WillOnce(Return(FALSE));
  tab_windows.push_back(kBadWindow);
  executor_->set_tab_windows(tab_windows);
  EXPECT_HRESULT_SUCCEEDED(executor_->CallGetTabs(&result));
  wnsprintf(expected_result, 16, L"[%d,%d,%d,%d]", kGoodWindowId, kTabIndex,
      kOtherGoodWindowId, kOtherTabIndex);
  EXPECT_STREQ(expected_result, result.m_str);
}

TEST_F(ExecutorTests, UpdateWindow) {
  testing::LogDisabler no_dchecks;
  executor_->set_id(kGoodWindow);

  // Not running in appropriate thread.
  NotRunningInThisWindowThread(kGoodWindow);
  EXPECT_HRESULT_FAILED(executor_->UpdateWindow(0, 0, 0, 0, NULL));
  // No other failure path, go straight to success...
  RepeatedlyRunningInThisWindowThread(kGoodWindow);

  RECT window_rect = {1, 2, 3, 5};
  EXPECT_CALL(user32_, GetWindowRect(kGoodWindow, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(window_rect), Return(TRUE)));
  // These will be called from GetWindow which is called at the end of Update.
  EXPECT_CALL(user32_, GetForegroundWindow()).
      WillRepeatedly(Return(kGoodWindow));
  EXPECT_CALL(mock_window_utils_, GetTopLevelParent(kGoodWindow)).
      WillRepeatedly(Return(kGoodWindow));
  // Try with no change at first.
  EXPECT_CALL(user32_, MoveWindow(kGoodWindow, 1, 2, 2, 3, TRUE)).Times(1);
  common_api::WindowInfo window_info;
  EXPECT_HRESULT_SUCCEEDED(executor_->UpdateWindow(
      -1, -1, -1, -1, &window_info));
  EXPECT_EQ(TRUE, window_info.focused);
  EXPECT_EQ(window_rect.top, window_info.rect.top);
  EXPECT_EQ(window_rect.left, window_info.rect.left);
  EXPECT_EQ(window_rect.bottom, window_info.rect.bottom);
  EXPECT_EQ(window_rect.right, window_info.rect.right);
  EXPECT_EQ(NULL, window_info.tab_list);

  // Now try with some changes incrementally.
  EXPECT_CALL(user32_, MoveWindow(kGoodWindow, 1, 2, 2, 30, TRUE)).Times(1);
  EXPECT_HRESULT_SUCCEEDED(executor_->UpdateWindow(
      -1, -1, -1, 30, &window_info));
  EXPECT_EQ(NULL, window_info.tab_list);

  EXPECT_CALL(user32_, MoveWindow(kGoodWindow, 1, 2, 20, 3, TRUE)).Times(1);
  EXPECT_HRESULT_SUCCEEDED(executor_->UpdateWindow(
      -1, -1, 20, -1, &window_info));
  EXPECT_EQ(NULL, window_info.tab_list);

  EXPECT_CALL(user32_, MoveWindow(kGoodWindow, 1, 0, 2, 5, TRUE)).Times(1);
  EXPECT_HRESULT_SUCCEEDED(executor_->UpdateWindow(
      -1, 0, -1, -1, &window_info));
  EXPECT_EQ(NULL, window_info.tab_list);

  EXPECT_CALL(user32_, MoveWindow(kGoodWindow, 0, 2, 3, 3, TRUE)).Times(1);
  EXPECT_HRESULT_SUCCEEDED(executor_->UpdateWindow(
      0, -1, -1, -1, &window_info));
  EXPECT_EQ(NULL, window_info.tab_list);

  EXPECT_CALL(user32_, MoveWindow(kGoodWindow, 8, 13, 21, 34, TRUE)).Times(1);
  EXPECT_HRESULT_SUCCEEDED(executor_->UpdateWindow(
      8, 13, 21, 34, &window_info));
  EXPECT_EQ(NULL, window_info.tab_list);
}

TEST_F(ExecutorTests, RemoveWindow) {
  testing::LogDisabler no_dchecks;
  executor_->set_id(kGoodWindow);

  // Not running in appropriate thread.
  NotRunningInThisWindowThread(kGoodWindow);
  EXPECT_HRESULT_FAILED(executor_->RemoveWindow());

  // Now let the manager succeed.
  RepeatedlyRunningInThisWindowThread(kGoodWindow);
  MockTabManager();
  EXPECT_CALL(*manager_, CloseAllTabs()).WillOnce(Return(S_OK));
  EXPECT_CALL(user32_, PostMessage(kGoodWindow, WM_CLOSE, 0, 0)).
    Times(0);
  EXPECT_HRESULT_SUCCEEDED(executor_->RemoveWindow());
}

TEST_F(ExecutorTests, GetTabInfo) {
  testing::LogDisabler no_dchecks;
  MockSite();
  executor_->set_id(kGoodWindow);

  // Not running in appropriate thread.
  NotRunningInThisWindowThread(kGoodWindow);
  EXPECT_HRESULT_FAILED(executor_->GetTabInfo(NULL));

  // Now can't get ready state.
  RepeatedlyRunningInThisWindowThread(kGoodWindow);
  EXPECT_CALL(*mock_site_, GetReadyState(NotNull())).WillOnce(Return(E_FAIL));
  tab_api::TabInfo tab_info;
  EXPECT_HRESULT_FAILED(executor_->GetTabInfo(&tab_info));
  DCHECK_EQ((BSTR)NULL, tab_info.url);
  DCHECK_EQ((BSTR)NULL, tab_info.title);
  DCHECK_EQ((BSTR)NULL, tab_info.fav_icon_url);

  // And can't get the top level browser.
  EXPECT_CALL(*mock_site_, GetReadyState(NotNull())).WillOnce(DoAll(
      SetArgumentPointee<0>(READYSTATE_COMPLETE), Return(S_OK)));
  EXPECT_CALL(*mock_site_, GetTopLevelBrowser(NotNull())).
      WillOnce(Return(E_FAIL));
  tab_info.Clear();
  EXPECT_HRESULT_FAILED(executor_->GetTabInfo(&tab_info));
  DCHECK_EQ((BSTR)NULL, tab_info.url);
  DCHECK_EQ((BSTR)NULL, tab_info.title);
  DCHECK_EQ((BSTR)NULL, tab_info.fav_icon_url);

  // Success time!
  EXPECT_CALL(*mock_site_, GetReadyState(NotNull())).WillOnce(DoAll(
      SetArgumentPointee<0>(READYSTATE_COMPLETE), Return(S_OK)));
  CComObject<StrictMock<testing::MockIWebBrowser2>>::CreateInstance(&browser_);
  DCHECK(browser_ != NULL);
  browser_keeper_ = browser_;
  EXPECT_CALL(*mock_site_, GetTopLevelBrowser(NotNull())).
      WillRepeatedly(DoAll(CopyInterfaceToArgument<0>(browser_keeper_),
                           Return(S_OK)));
  EXPECT_CALL(*browser_, get_LocationURL(NotNull())).
      WillOnce(DoAll(CopyBSTRToArgument<0>(kUrl1), Return(S_OK)));
  EXPECT_CALL(*browser_, get_LocationName(NotNull())).
      WillOnce(DoAll(CopyBSTRToArgument<0>(kTitle1), Return(S_OK)));

  tab_info.Clear();
  EXPECT_HRESULT_SUCCEEDED(executor_->GetTabInfo(&tab_info));
  EXPECT_STREQ(kUrl1, tab_info.url);
  EXPECT_STREQ(kTitle1, tab_info.title);
  EXPECT_EQ(kCeeeTabStatusComplete, tab_info.status);

  // With other values
  RepeatedlyRunningInThisWindowThread(kOtherGoodWindow);
  // Reset the HWND
  executor_->set_id(kOtherGoodWindow);
  EXPECT_CALL(*mock_site_, GetReadyState(NotNull())).WillOnce(DoAll(
      SetArgumentPointee<0>(READYSTATE_LOADING), Return(S_OK)));
  EXPECT_CALL(*browser_, get_LocationURL(NotNull())).
      WillOnce(DoAll(CopyBSTRToArgument<0>(kUrl2), Return(S_OK)));
  EXPECT_CALL(*browser_, get_LocationName(NotNull())).
      WillOnce(DoAll(CopyBSTRToArgument<0>(kTitle2), Return(S_OK)));

  tab_info.Clear();
  EXPECT_HRESULT_SUCCEEDED(executor_->GetTabInfo(&tab_info));
  EXPECT_STREQ(kUrl2, tab_info.url);
  EXPECT_STREQ(kTitle2, tab_info.title);
  EXPECT_EQ(kCeeeTabStatusLoading, tab_info.status);
}

TEST_F(ExecutorTests, GetTabIndex) {
  testing::LogDisabler no_dchecks;
  MockSite();
  executor_->set_id(kGoodWindow);

  // Not running in appropriate thread.
  EXPECT_CALL(mock_window_utils_, IsWindowThread(kGoodWindow)).
      WillOnce(Return(false));
  EXPECT_HRESULT_FAILED(executor_->GetTabIndex(kGoodWindowId, NULL));

  // Success.
  EXPECT_CALL(mock_window_utils_, IsWindowThread(kGoodWindow)).
      WillOnce(Return(true));
  MockTabManager();
  long index = 0;
  EXPECT_HRESULT_SUCCEEDED(executor_->GetTabIndex(kOtherGoodWindowId, &index));
  EXPECT_EQ(kTabIndex, index);
}

TEST_F(ExecutorTests, Navigate) {
  testing::LogDisabler no_dchecks;
  MockSite();
  executor_->set_id(kGoodWindow);

  // Not running in appropriate thread.
  NotRunningInThisWindowThread(kGoodWindow);
  EXPECT_HRESULT_FAILED(executor_->Navigate(NULL, 0, NULL));

  // Now make GetWebBrowser fail.
  RepeatedlyRunningInThisWindowThread(kGoodWindow);
  EXPECT_CALL(*executor_, GetWebBrowser(NotNull())).
      WillRepeatedly(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->Navigate(NULL, 0, NULL));

  // Now get URL fails.
  MockBrowser();
  EXPECT_CALL(*browser_, get_LocationURL(NotNull())).WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->Navigate(NULL, 0, NULL));

  // Now succeed by not needing to navigate since same URL.
  EXPECT_CALL(*browser_, get_LocationURL(NotNull())).
      WillRepeatedly(DoAll(CopyBSTRToArgument<0>(kUrl1), Return(S_OK)));
  EXPECT_HRESULT_SUCCEEDED(executor_->Navigate(CComBSTR(kUrl1), 0, NULL));

  // And finally succeed completely.
  EXPECT_CALL(*browser_, Navigate(_, _, _, _, _)).WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(executor_->Navigate(CComBSTR(kUrl2), 0, NULL));
  // And fail if navigate fails.
  EXPECT_CALL(*browser_, Navigate(_, _, _, _, _)).WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->Navigate(CComBSTR(kUrl2), 0, NULL));
}

TEST_F(ExecutorTests, RemoveTab) {
  testing::LogDisabler no_dchecks;
  MockSite();

  // Since we're in a WindowExecutor and not a TabExecutor, we don't get our
  // HWND and there's no call to parent.
  executor_->set_id(kOtherGoodWindow);

  // Not running in appropriate thread.
  NotRunningInThisWindowThread(kOtherGoodWindow);
  EXPECT_HRESULT_FAILED(executor_->RemoveTab(kGoodWindowId));

  // Fail to get the tab index.
  RepeatedlyRunningInThisWindowThread(kOtherGoodWindow);
  EXPECT_CALL(user32_, GetParent(_)).WillRepeatedly(Return(kGoodWindow));
  MockTabManager();
  EXPECT_CALL(*manager_, IndexFromHWND(kGoodWindow, NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->RemoveTab(kGoodWindowId));

  // Fail to get the tab interface.
  EXPECT_CALL(*manager_, IndexFromHWND(kGoodWindow, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(kTabIndex), Return(S_OK)));
  EXPECT_CALL(*manager_, GetItem(kTabIndex, NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->RemoveTab(kGoodWindowId));

  // Success.
  CComObject<StrictMock<testing::MockITabWindowIe7>>* mock_tab;
  CComObject<StrictMock<testing::MockITabWindowIe7>>::CreateInstance(&mock_tab);
  CComPtr<ITabWindowIe7> mock_tab_holder(mock_tab);
  EXPECT_CALL(*manager_, GetItem(kTabIndex, NotNull())).WillRepeatedly(DoAll(
      SetArgumentPointee<1>(mock_tab), AddRef(mock_tab), Return(S_OK)));
  EXPECT_CALL(*mock_tab, Close()).WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(executor_->RemoveTab(kGoodWindowId));

  // Failure to close.
  EXPECT_CALL(*mock_tab, Close()).WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->RemoveTab(kGoodWindowId));
}

TEST_F(ExecutorTests, SelectTab) {
  testing::LogDisabler no_dchecks;
  MockSite();

  executor_->set_id(kOtherGoodWindow);

  // Not running in appropriate thread.
  NotRunningInThisWindowThread(kOtherGoodWindow);
  EXPECT_HRESULT_FAILED(executor_->SelectTab(kGoodWindowId));

  // Fail to get the tab index.
  RepeatedlyRunningInThisWindowThread(kOtherGoodWindow);
  EXPECT_CALL(user32_, GetParent(_)).WillRepeatedly(Return(kGoodWindow));
  MockTabManager();
  EXPECT_CALL(*manager_, IndexFromHWND(kGoodWindow, NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->SelectTab(kGoodWindowId));

  // Success!
  EXPECT_CALL(*manager_, IndexFromHWND(kGoodWindow, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(kTabIndex), Return(S_OK)));
  EXPECT_CALL(*manager_, SelectTab(kTabIndex)).WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(executor_->SelectTab(kGoodWindowId));

  // Failure to Select.
  EXPECT_CALL(*manager_, SelectTab(kTabIndex)).WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->SelectTab(kGoodWindowId));
}

TEST_F(ExecutorTests, MoveTab) {
  testing::LogDisabler no_dchecks;
  MockTabManager();
  MockSite();
  RepeatedlyRunningInThisWindowThread(kGoodWindow);
  executor_->set_id(kGoodWindow);

  // Fail to get tab count.
  EXPECT_CALL(*manager_, GetCount(NotNull())).WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->MoveTab(kOtherGoodWindowId, 0));

  // Nothing to be done.
  LONG nb_tabs = 3;
  EXPECT_CALL(*manager_, GetCount(NotNull())).WillRepeatedly(DoAll(
      SetArgumentPointee<0>(nb_tabs), Return(S_OK)));
  EXPECT_HRESULT_SUCCEEDED(executor_->MoveTab(kOtherGoodWindowId, 2));
  EXPECT_HRESULT_SUCCEEDED(executor_->MoveTab(kOtherGoodWindowId, 99));

  // Fail to get the first tab interface.
  EXPECT_CALL(*manager_, GetItem(0, NotNull())).WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->MoveTab(kOtherGoodWindowId, 0));

  // Fail to get the tab id.
  CComObject<StrictMock<testing::MockITabWindowIe7>>* mock_tab0;
  CComObject<StrictMock<testing::MockITabWindowIe7>>::CreateInstance(
      &mock_tab0);
  CComPtr<ITabWindowIe7> mock_tab_holder0(mock_tab0);
  EXPECT_CALL(*manager_, GetItem(0, NotNull())).WillRepeatedly(DoAll(
      SetArgumentPointee<1>(mock_tab0), AddRef(mock_tab0), Return(S_OK)));
  EXPECT_CALL(*mock_tab0, GetID(NotNull())).WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->MoveTab(kOtherGoodWindowId, 0));

  // Fail to get the second tab interface.
  EXPECT_CALL(*mock_tab0, GetID(NotNull())).WillRepeatedly(
      DoAll(SetArgumentPointee<0>(kGoodWindowId), Return(S_OK)));
  EXPECT_CALL(*manager_, GetItem(nb_tabs - 1, NotNull()))
      .WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->MoveTab(kOtherGoodWindowId, 0));

  // Fail to get the second tab id.
  CComObject<StrictMock<testing::MockITabWindowIe7>>* mock_tab1;
  CComObject<StrictMock<testing::MockITabWindowIe7>>::CreateInstance(
      &mock_tab1);
  CComPtr<ITabWindowIe7> mock_tab_holder1(mock_tab1);
  EXPECT_CALL(*manager_, GetItem(nb_tabs - 1, NotNull())).WillRepeatedly(DoAll(
      SetArgumentPointee<1>(mock_tab1), AddRef(mock_tab1), Return(S_OK)));
  EXPECT_CALL(*mock_tab1, GetID(NotNull())).WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->MoveTab(kOtherGoodWindowId, 0));

  // Fail to reposition.
  EXPECT_CALL(*mock_tab1, GetID(NotNull())).WillRepeatedly(
      DoAll(SetArgumentPointee<0>(kOtherGoodWindowId), Return(S_OK)));
  EXPECT_CALL(*manager_, RepositionTab(kOtherGoodWindowId, kGoodWindowId, 0))
      .WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->MoveTab(kOtherGoodWindowId, 0));

  // Success!!
  EXPECT_CALL(*mock_tab1, GetID(NotNull())).WillRepeatedly(
      DoAll(SetArgumentPointee<0>(kOtherGoodWindowId), Return(S_OK)));
  EXPECT_CALL(*manager_, RepositionTab(kOtherGoodWindowId, kGoodWindowId, 0))
      .WillRepeatedly(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(executor_->MoveTab(kOtherGoodWindowId, 0));
}

TEST_F(ExecutorTests, GetCookieValue) {
  testing::LogDisabler no_dchecks;
  MockWinInet mock_wininet;

  CComBSTR url(L"http://foobar.com");
  CComBSTR name(L"HELLOWORLD");
  // Bad parameters.
  EXPECT_HRESULT_FAILED(executor_->CallGetCookieValue(url, name, NULL));

  // Failure to get cookie.
  EXPECT_CALL(mock_wininet, InternetGetCookieExW(_, _, IsNull(), _, _, _))
      .WillOnce(DoAll(WithoutArgs(Invoke(
          SetFakeLastError<ERROR_INVALID_PARAMETER>)), Return(FALSE)));
  CComBSTR value;
  EXPECT_HRESULT_FAILED(executor_->CallGetCookieValue(url, name, &value));
  EXPECT_EQ((BSTR)NULL, value);
  ::SetLastError(ERROR_SUCCESS);

  // Nonexistent cookie.
  EXPECT_CALL(mock_wininet, InternetGetCookieExW(_, _, IsNull(), _, _, _))
      .WillOnce(DoAll(WithoutArgs(Invoke(
          SetFakeLastError<ERROR_NO_MORE_ITEMS>)), Return(FALSE)));
  EXPECT_EQ(S_FALSE, executor_->CallGetCookieValue(url, name, &value));
  EXPECT_EQ((BSTR)NULL, value);
  ::SetLastError(ERROR_SUCCESS);

  // Malformed cookie.
  EXPECT_CALL(mock_wininet, InternetGetCookieExW(_, _, IsNull(), _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(100), Return(TRUE)));
  EXPECT_CALL(mock_wininet, InternetGetCookieExW(_, _, NotNull(), _, _, _))
      .WillRepeatedly(Invoke(TestingExecutor::MockInternetGetCookieExW));
  executor_->set_cookie_data(L"malformed_cookie_data");
  EXPECT_EQ(E_FAIL, executor_->CallGetCookieValue(url, name, &value));
  EXPECT_EQ((BSTR)NULL, value);
  executor_->set_cookie_data(L"AnotherCookie=FOOBAR");
  EXPECT_EQ(E_FAIL, executor_->CallGetCookieValue(url, name, &value));
  EXPECT_EQ((BSTR)NULL, value);

  // Well-behaved cookie.
  executor_->set_cookie_data(L"HELLOWORLD=1234567890");
  EXPECT_EQ(S_OK, executor_->CallGetCookieValue(url, name, &value));
  EXPECT_STREQ(L"1234567890", value);
  executor_->set_cookie_data(L"=1234567890");
  EXPECT_EQ(S_OK, executor_->CallGetCookieValue(url, CComBSTR(L""), &value));
  EXPECT_STREQ(L"1234567890", value);
  executor_->set_cookie_data(L"HELLOWORLD=");
  EXPECT_EQ(S_OK, executor_->CallGetCookieValue(url, name, &value));
  EXPECT_STREQ(L"", value);
  executor_->set_cookie_data(L"=");
  EXPECT_EQ(S_OK, executor_->CallGetCookieValue(url, CComBSTR(L""), &value));
  EXPECT_STREQ(L"", value);
}

TEST_F(ExecutorTests, GetCookieValueFlagsByIeVersion) {
  testing::LogDisabler no_dchecks;
  MockWinInet mock_wininet;
  MockIeUtil mock_ie_util;

  CComBSTR url(L"http://foobar.com");
  CComBSTR name(L"HELLOWORLD");

  // Test IE7 and below.
  DWORD expected_flags = 0;
  EXPECT_CALL(mock_wininet, InternetGetCookieExW(_, _, IsNull(), _,
                                                 expected_flags, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(100), Return(TRUE)));
  EXPECT_CALL(mock_wininet, InternetGetCookieExW(_, _, NotNull(), _,
                                                 expected_flags, _))
      .WillRepeatedly(Invoke(TestingExecutor::MockInternetGetCookieExW));

  EXPECT_CALL(mock_ie_util, GetIeVersion())
    .WillOnce(Return(ie_util::IEVERSION_IE6));
  executor_->set_cookie_data(L"HELLOWORLD=1234567890");
  CComBSTR value;
  EXPECT_EQ(S_OK, executor_->CallGetCookieValue(url, name, &value));

  EXPECT_CALL(mock_ie_util, GetIeVersion())
      .WillOnce(Return(ie_util::IEVERSION_IE7));
  executor_->set_cookie_data(L"HELLOWORLD=1234567890");
  EXPECT_EQ(S_OK, executor_->CallGetCookieValue(url, name, &value));

  // Test IE8 and above.
  expected_flags = INTERNET_COOKIE_HTTPONLY;
  EXPECT_CALL(mock_wininet, InternetGetCookieExW(_, _, IsNull(), _,
                                                 expected_flags, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(100), Return(TRUE)));
  EXPECT_CALL(mock_wininet, InternetGetCookieExW(_, _, NotNull(), _,
                                                 expected_flags, _))
      .WillRepeatedly(Invoke(TestingExecutor::MockInternetGetCookieExW));

  EXPECT_CALL(mock_ie_util, GetIeVersion())
    .WillOnce(Return(ie_util::IEVERSION_IE8));
  executor_->set_cookie_data(L"HELLOWORLD=1234567890");
  EXPECT_EQ(S_OK, executor_->CallGetCookieValue(url, name, &value));

  EXPECT_CALL(mock_ie_util, GetIeVersion())
      .WillOnce(Return(ie_util::IEVERSION_IE9));
  executor_->set_cookie_data(L"HELLOWORLD=1234567890");
  EXPECT_EQ(S_OK, executor_->CallGetCookieValue(url, name, &value));

}

TEST_F(ExecutorTests, GetCookie) {
  testing::LogDisabler no_dchecks;
  CComBSTR url(L"http://foobar.com");
  CComBSTR name(L"HELLOWORLD");

  EXPECT_HRESULT_FAILED(executor_->GetCookie(url, name, NULL));

  // Failure to get cookie.
  EXPECT_CALL(*executor_, GetCookieValue(_, _, NotNull()))
      .WillOnce(Return(E_FAIL));
  cookie_api::CookieInfo cookie_info;
  cookie_info.name = NULL;
  cookie_info.value = NULL;
  EXPECT_HRESULT_FAILED(executor_->GetCookie(url, name, &cookie_info));
  EXPECT_EQ((BSTR)NULL, cookie_info.name);
  EXPECT_EQ((BSTR)NULL, cookie_info.value);

  // Nonexistent cookie.
  EXPECT_CALL(*executor_, GetCookieValue(_, _, NotNull()))
      .WillOnce(Return(S_FALSE));
  EXPECT_EQ(S_FALSE, executor_->GetCookie(url, name, &cookie_info));
  EXPECT_EQ((BSTR)NULL, cookie_info.name);
  EXPECT_EQ((BSTR)NULL, cookie_info.value);

  // Success.
  EXPECT_CALL(*executor_, GetCookieValue(_, _, NotNull()))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<2>(::SysAllocString(L"abcde")),
          Return(S_OK)));
  EXPECT_EQ(S_OK, executor_->GetCookie(url, name, &cookie_info));
  EXPECT_STREQ(L"HELLOWORLD", cookie_info.name);
  EXPECT_STREQ(L"abcde", cookie_info.value);
  EXPECT_EQ(S_OK, executor_->GetCookie(url, CComBSTR("ABC"), &cookie_info));
  EXPECT_STREQ(L"ABC", cookie_info.name);
  EXPECT_STREQ(L"abcde", cookie_info.value);
}

TEST_F(ExecutorTests, RegisterCookieStore) {
  testing::LogDisabler no_dchecks;

  executor_->set_cookie_store_is_registered(false);
  EXPECT_EQ(S_FALSE, executor_->CookieStoreIsRegistered());
  EXPECT_HRESULT_SUCCEEDED(executor_->RegisterCookieStore());
  EXPECT_EQ(S_OK, executor_->CookieStoreIsRegistered());
  EXPECT_HRESULT_SUCCEEDED(executor_->RegisterCookieStore());
  EXPECT_EQ(S_OK, executor_->CookieStoreIsRegistered());
}

TEST_F(ExecutorTests, ShowInfobar) {
  testing::LogDisabler no_dchecks;
  CComBSTR url(L"/infobar/test.html");
  CComBSTR empty_url(L"");
  CComBSTR extension_id(L"abcdefjh");

  // Calling when manager has not been initialized.
  EXPECT_HRESULT_FAILED(executor_->ShowInfobar(url, NULL));

  // Initializing the manager and calling it with different parameters.
  MockInfobarManager* manager;
  executor_->InitializeInfobarManager(&manager);

  EXPECT_CALL(*manager, Show(infobar_api::TOP_INFOBAR, _,
                             std::wstring(url.m_str), true)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(mock_window_utils_, GetTopLevelParent(_)).
      WillRepeatedly(Return(kGoodWindow));
  CeeeWindowHandle window_handle;

  EXPECT_HRESULT_SUCCEEDED(executor_->ShowInfobar(url, &window_handle));
  EXPECT_EQ(reinterpret_cast<CeeeWindowHandle>(kGoodWindow), window_handle);

  EXPECT_CALL(*manager, HideAll()).Times(1);
  EXPECT_HRESULT_SUCCEEDED(executor_->ShowInfobar(empty_url, &window_handle));
  EXPECT_EQ(0, window_handle);

  EXPECT_CALL(*manager, Show(infobar_api::TOP_INFOBAR, _,
                             std::wstring(url.m_str), true)).
      WillOnce(Return(E_FAIL));
  EXPECT_HRESULT_FAILED(executor_->ShowInfobar(url, &window_handle));
  EXPECT_EQ(0, window_handle);

  EXPECT_CALL(*manager, HideAll()).Times(1);
  EXPECT_HRESULT_SUCCEEDED(executor_->OnTopFrameBeforeNavigate(url));

  EXPECT_HRESULT_SUCCEEDED(executor_->SetExtensionId(extension_id));
  std::wstring full_url(L"chrome-extension://");
  full_url += extension_id.m_str;
  full_url += url.m_str;
  EXPECT_CALL(*manager, Show(infobar_api::TOP_INFOBAR, _, full_url, true)).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(executor_->ShowInfobar(url, &window_handle));

  // Have to destroy the manager before LogDisabler from this test goes out of
  // scope.
  EXPECT_CALL(user32_, IsWindow(NULL)).WillRepeatedly(Return(FALSE));
  executor_->TerminateInfobarManager();
}

}  // namespace
