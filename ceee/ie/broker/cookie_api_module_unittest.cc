// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Cookie API implementation unit tests.

// MockWin32 can't be included after ChromeFrameHost because of an include
// incompatibility with atlwin.h.
#include "ceee/testing/utils/mock_win32.h"  // NOLINT

#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/api_module_constants.h"
#include "ceee/ie/broker/cookie_api_module.h"
#include "ceee/ie/broker/tab_api_module.h"
#include "ceee/ie/broker/window_api_module.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/test_utils.h"
#include "chrome/browser/extensions/extension_cookies_api_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_cookies_api_constants;

namespace {

using cookie_api::CookieApiResult;
using cookie_api::CookieChanged;
using cookie_api::CookieInfo;
using cookie_api::GetCookie;
using cookie_api::GetAllCookies;
using cookie_api::SetCookie;
using cookie_api::RemoveCookie;
using cookie_api::GetAllCookieStores;
using cookie_api::IterativeCookieApiResult;
using testing::_;
using testing::AddRef;
using testing::AtLeast;
using testing::Invoke;
using testing::InstanceCountMixin;
using testing::MockApiDispatcher;
using testing::MockApiInvocation;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;

using window_api::WindowApiResult;

const int kRequestId = 21;
const int kThreadId = 1;
const int kNumWindows = 4;
const int kNumProcesses = 3;
const HWND kAllWindows[kNumWindows] = {(HWND)1, (HWND)17, (HWND)5, (HWND)8};
const HWND kBadWindows[] = {(HWND)23, (HWND)2};
const DWORD kWindowProcesses[kNumWindows] = {2, 3, 2, 7};
const DWORD kAllProcesses[kNumProcesses] = {2, 3, 7};
const HANDLE kProcessHandles[kNumProcesses] = {
  HANDLE(0xFF0002),
  HANDLE(0xFF0003),
  HANDLE(0xFF0007)
};

// Provides stubs for some CookieApiResult member functions.
class CookieApiResultStub {
 public:
  CookieApiResultStub() : tab_list_values_(NULL), num_tab_list_values_(0) {}

  static int GetTabIdFromHandle(HWND handle) {
    return reinterpret_cast<int>(handle);
  }

  bool GetTabListForWindow(HWND window, scoped_ptr<ListValue>* tab_list) {
    scoped_ptr<ListValue> tab_list_local(new ListValue());
    for (int i = 0; i < num_tab_list_values_; ++i) {
      tab_list_local->Append(Value::CreateIntegerValue(tab_list_values_[i]));
    }
    tab_list->reset(tab_list_local.release());
    return true;
  }

  void SetTabListExpectation(const int* tab_list_values,
                             int num_tab_list_values) {
    tab_list_values_ = tab_list_values;
    num_tab_list_values_ = num_tab_list_values;
  }

 private:
  const int* tab_list_values_;
  int num_tab_list_values_;
};

class MockCookieApiResult : public CookieApiResult {
 public:
  explicit MockCookieApiResult(int request_id) : CookieApiResult(request_id) {}

  MOCK_METHOD0(PostResult, void());
  MOCK_METHOD1(PostError, void(const std::string&));
  MOCK_METHOD5(GetCookieInfo, HRESULT(const std::string&, const std::string&,
                                      HWND, bool, CookieInfo*));
  MOCK_METHOD1(RegisterCookieStore, HRESULT(HWND));
  MOCK_METHOD1(CookieStoreIsRegistered, HRESULT(HWND));
  MOCK_METHOD4(GetAnyWindowInStore, bool(const std::string&, bool, HWND*,
                                         bool*));
  MOCK_METHOD2(GetTabListForWindow, bool(HWND, scoped_ptr<ListValue>*));
  MOCK_METHOD2(GetAnyTabInWindow, HWND(HWND, bool));
  MOCK_METHOD2(GetTabProtectedMode, bool(HWND, bool*));

  virtual ApiDispatcher* GetDispatcher() {
    return &mock_api_dispatcher_;
  }

  HRESULT CallGetCookieInfo(const std::string& url,
                            const std::string& name,
                            HWND window, bool is_protected_mode,
                            CookieInfo* cookie_info) {
    return CookieApiResult::GetCookieInfo(url, name, window, is_protected_mode,
                                          cookie_info);
  }

  bool CallGetAnyWindowInStore(const std::string& store_id,
                               bool allow_unregistered_store, HWND* window,
                               bool* is_protected_mode) {
    return CookieApiResult::GetAnyWindowInStore(
        store_id, allow_unregistered_store, window, is_protected_mode);
  }

  bool CallGetTabListForWindow(HWND window, scoped_ptr<ListValue>* tab_list) {
    return CookieApiResult::GetTabListForWindow(window, tab_list);
  }

  HWND CallGetAnyTabInWindow(HWND window, bool is_protected_mode) {
    return CookieApiResult::GetAnyTabInWindow(window, is_protected_mode);
  }

  bool CallGetTabProtectedMode(HWND tab_window, bool* is_protected_mode) {
    return CookieApiResult::GetTabProtectedMode(tab_window, is_protected_mode);
  }

  StrictMock<MockApiDispatcher> mock_api_dispatcher_;
  CookieApiResultStub stub_;
};

// Mocks the iterative version of result. Unlike in the mock of the 'straight'
// version, here PostResult and Post Error are not mocked because they serve
// to accumulate result, which can be examined by standard means.
class MockIterativeCookieApiResult : public IterativeCookieApiResult {
 public:
  explicit MockIterativeCookieApiResult(int request_id)
      : IterativeCookieApiResult(request_id) {}

  MOCK_METHOD0(CallRealPostResult, void());
  MOCK_METHOD1(CallRealPostError, void(const std::string&));
  MOCK_METHOD5(GetCookieInfo, HRESULT(const std::string&, const std::string&,
                                      HWND, bool, CookieInfo*));
  MOCK_METHOD2(CreateCookieStoreValues, bool(DWORD, const WindowSet&));
  MOCK_METHOD1(RegisterCookieStore, HRESULT(HWND));
  MOCK_METHOD1(CookieStoreIsRegistered, HRESULT(HWND));
  MOCK_METHOD4(GetAnyWindowInStore, bool(const std::string&, bool, HWND*,
                                         bool*));
  MOCK_METHOD2(GetTabListForWindow, bool(HWND, scoped_ptr<ListValue>*));
  MOCK_METHOD2(GetTabProtectedMode, bool(HWND, bool*));

  virtual void PostError(const std::string& error) {
    ++error_counter_;
    IterativeCookieApiResult::PostError(error);
  }

  virtual void PostResult() {
    ++success_counter_;
    IterativeCookieApiResult::PostResult();
  }

  virtual ApiDispatcher* GetDispatcher() {
    return &mock_api_dispatcher_;
  }

  HRESULT CallGetCookieInfo(const std::string& url,
                            const std::string& name,
                            HWND window, bool is_protected_mode,
                            CookieInfo* cookie_info) {
    return CookieApiResult::GetCookieInfo(url, name, window, is_protected_mode,
                                          cookie_info);
  }

  virtual bool CallCreateCookieStoreValues(DWORD process_id,
                                           const WindowSet& windows) {
    return IterativeCookieApiResult::CreateCookieStoreValues(
        process_id, windows);
  }

  bool CallGetAnyWindowInStore(const std::string& store_id,
                               bool allow_unregistered_store, HWND* window,
                               bool* is_protected_mode) {
    return CookieApiResult::GetAnyWindowInStore(
        store_id, allow_unregistered_store, window, is_protected_mode);
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
  CookieApiResultStub stub_;

 private:
  static int success_counter_;
  static int error_counter_;
};

int MockIterativeCookieApiResult::success_counter_ = 0;
int MockIterativeCookieApiResult::error_counter_ = 0;

class MockCookieChanged : public CookieChanged {
 public:
  void AllocateApiResult() {
    api_result_.reset(
        new StrictMock<MockCookieApiResult>(CookieApiResult::kNoRequestId));
  }

  virtual CookieApiResult* CreateApiResult() {
    DCHECK(api_result_.get() != NULL);
    return api_result_.release();
  }

  scoped_ptr<StrictMock<MockCookieApiResult> > api_result_;
};

// Mock static functions defined in CookieApiResult.
MOCK_STATIC_CLASS_BEGIN(MockCookieApiResultStatics)
  MOCK_STATIC_INIT_BEGIN(MockCookieApiResultStatics)
    MOCK_STATIC_INIT2(CookieApiResult::FindAllProcessesAndWindows,
                      FindAllProcessesAndWindows);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC1(void, , FindAllProcessesAndWindows,
               CookieApiResult::ProcessWindowMap*);
MOCK_STATIC_CLASS_END(MockCookieApiResultStatics)

// Mock static functions defined in WindowApiResult.
MOCK_STATIC_CLASS_BEGIN(MockWindowApiResultStatics)
  MOCK_STATIC_INIT_BEGIN(MockWindowApiResultStatics)
    MOCK_STATIC_INIT2(WindowApiResult::TopIeWindow,
                      TopIeWindow);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC0(HWND, , TopIeWindow);
MOCK_STATIC_CLASS_END(MockWindowApiResultStatics)

class CookieApiTests: public testing::Test {
 public:
  virtual void SetUp() {
    if (!testing::MockExecutorsManager::test_instance_) {
      // To be freed at exit by Singleton class.
      testing::MockExecutorsManager::test_instance_ =
          new StrictMock<testing::MockExecutorsManager>;
    }
    executors_manager_ =
        reinterpret_cast<StrictMock<testing::MockExecutorsManager>*>(
            testing::MockExecutorsManager::test_instance_);
  }
 protected:
  template <class T> void ExpectInvalidArguments(
      StrictMock<MockApiInvocation<CookieApiResult,
                                   MockCookieApiResult,
                                   T> >& invocation,
      const ListValue& args) {
    invocation.AllocateNewResult(kRequestId);
    EXPECT_CALL(*invocation.invocation_result_,
                PostError(StrEq(
                    api_module_constants::kInvalidArgumentsError))).Times(1);
    invocation.Execute(args, kRequestId);
  }

  StrictMock<testing::MockUser32> user32_;
  // Lifespan controlled by Singleton template.
  StrictMock<testing::MockExecutorsManager>* executors_manager_;
};

TEST_F(CookieApiTests, RegisterInvocations) {
  StrictMock<MockApiDispatcher> disp;
  EXPECT_CALL(disp,
              RegisterInvocation(NotNull(), NotNull())).Times(AtLeast(5));
  cookie_api::RegisterInvocations(&disp);
}

TEST_F(CookieApiTests, CreateCookieValue) {
  CookieInfo cookie_info;
  cookie_info.name = ::SysAllocString(L"foo");
  cookie_info.value = ::SysAllocString(L"bar");
  cookie_info.domain = ::SysAllocString(L"google.com");
  cookie_info.host_only = FALSE;
  cookie_info.path = ::SysAllocString(L"/testpath");
  cookie_info.secure = TRUE;
  cookie_info.session = TRUE;
  cookie_info.expiration_date = 0;
  cookie_info.store_id = ::SysAllocString(L"test_store_id");

  CookieApiResult result(CookieApiResult::kNoRequestId);
  EXPECT_TRUE(result.CreateCookieValue(cookie_info));
  EXPECT_EQ(Value::TYPE_DICTIONARY, result.value()->GetType());
  const DictionaryValue* cookie =
      reinterpret_cast<const DictionaryValue*>(result.value());

  std::string string_value;
  bool boolean_value;
  EXPECT_TRUE(cookie->GetString(keys::kNameKey, &string_value));
  EXPECT_EQ("foo", string_value);
  EXPECT_TRUE(cookie->GetString(keys::kValueKey, &string_value));
  EXPECT_EQ("bar", string_value);
  EXPECT_TRUE(cookie->GetString(keys::kDomainKey, &string_value));
  EXPECT_EQ("google.com", string_value);
  EXPECT_TRUE(cookie->GetBoolean(keys::kHostOnlyKey, &boolean_value));
  EXPECT_FALSE(boolean_value);
  EXPECT_TRUE(cookie->GetString(keys::kPathKey, &string_value));
  EXPECT_EQ("/testpath", string_value);
  EXPECT_TRUE(cookie->GetBoolean(keys::kSecureKey, &boolean_value));
  EXPECT_TRUE(boolean_value);
  EXPECT_TRUE(cookie->GetBoolean(keys::kHttpOnlyKey, &boolean_value));
  EXPECT_FALSE(boolean_value);
  EXPECT_TRUE(cookie->GetBoolean(keys::kSessionKey, &boolean_value));
  EXPECT_TRUE(boolean_value);
  EXPECT_TRUE(cookie->GetString(keys::kStoreIdKey, &string_value));
  EXPECT_EQ("test_store_id", string_value);
  EXPECT_FALSE(cookie->HasKey(keys::kExpirationDateKey));
}

TEST_F(CookieApiTests, GetCookieInfo) {
  StrictMock<MockCookieApiResult> result(CookieApiResult::kNoRequestId);
  HWND test_frame_window = reinterpret_cast<HWND>(1);
  HWND test_tab_window = reinterpret_cast<HWND>(2);

  // Test no tab windows found.
  EXPECT_CALL(result, GetAnyTabInWindow(test_frame_window, true))
      .WillOnce(Return(HWND(NULL)));
  EXPECT_CALL(result, PostError(_)).Times(1);
  EXPECT_HRESULT_FAILED(result.CallGetCookieInfo("helloworld", "foo",
                                                 test_frame_window, true,
                                                 NULL));

  EXPECT_CALL(result, GetAnyTabInWindow(test_frame_window, false))
      .WillRepeatedly(Return(test_tab_window));

  // Test failed executor access.
  EXPECT_CALL(result.mock_api_dispatcher_,
              GetExecutor(test_tab_window, _, NotNull())).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_CALL(result, PostError(_)).Times(1);
  EXPECT_HRESULT_FAILED(result.CallGetCookieInfo("helloworld", "foo",
                                                 test_frame_window, false,
                                                 NULL));
  // Test executor.
  testing::MockCookieExecutor* mock_cookie_executor;
  ScopedComPtr<ICeeeCookieExecutor> mock_cookie_executor_keeper;
  EXPECT_HRESULT_SUCCEEDED(testing::MockCookieExecutor::CreateInitialized(
      &mock_cookie_executor, mock_cookie_executor_keeper.Receive()));
  EXPECT_CALL(result.mock_api_dispatcher_,
              GetExecutor(test_tab_window, _, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(
                               mock_cookie_executor_keeper.get()),
                           AddRef(mock_cookie_executor_keeper.get())));
  // Failing Executor.
  // The executor classes are already strict from their base class impl.
  EXPECT_CALL(*mock_cookie_executor, GetCookie(_, _, _)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(result, PostError(_)).Times(1);
  EXPECT_HRESULT_FAILED(result.CallGetCookieInfo("helloworld", "foo",
                                                 test_frame_window, false,
                                                 NULL));
  // Success.
  EXPECT_CALL(*mock_cookie_executor, GetCookie(_, _, _)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(result, PostError(_)).Times(0);
  EXPECT_EQ(S_OK, result.CallGetCookieInfo("helloworld", "foo",
                                           test_frame_window, false, NULL));
}

TEST_F(CookieApiTests, GetAnyWindowInStore) {
  StrictMock<MockCookieApiResult> result(CookieApiResult::kNoRequestId);
  EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(_)).
      WillRepeatedly(Return(true));

  HWND window;
  bool is_protected_mode;
  EXPECT_CALL(result, PostError(_)).Times(1);
  EXPECT_FALSE(result.CallGetAnyWindowInStore("test_store_id", false, &window,
                                              &is_protected_mode));

  std::set<HWND> empty_window_set;
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, FindTopLevelWindows(_, _)).
      WillOnce(SetArgumentPointee<1>(empty_window_set));
  EXPECT_CALL(result, PostError(_)).Times(1);
  EXPECT_FALSE(result.CallGetAnyWindowInStore("1", false, &window,
                                              &is_protected_mode));

  std::set<HWND> test_windows(kAllWindows, kAllWindows + kNumWindows);
  EXPECT_CALL(window_utils, FindTopLevelWindows(_, _)).
      WillRepeatedly(SetArgumentPointee<1>(test_windows));
  EXPECT_CALL(user32_, GetWindowThreadProcessId(_, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(1),
                     Return(kThreadId)));
  // Test unregistered cookie store.
  EXPECT_CALL(result, CookieStoreIsRegistered(_)).
      WillOnce(Return(E_FAIL));
  EXPECT_CALL(result, PostError(_)).Times(1);
  EXPECT_FALSE(result.CallGetAnyWindowInStore("1", false, &window,
                                              &is_protected_mode));

  EXPECT_CALL(result, CookieStoreIsRegistered(_)).
      WillRepeatedly(Return(S_FALSE));
  EXPECT_FALSE(result.CallGetAnyWindowInStore("1", false, &window,
                                              &is_protected_mode));
  EXPECT_TRUE(result.CallGetAnyWindowInStore("1", true, &window,
                                              &is_protected_mode));
  EXPECT_EQ((HWND)1, window);
  EXPECT_FALSE(is_protected_mode);
  EXPECT_TRUE(result.CallGetAnyWindowInStore("1low", true, &window,
                                             &is_protected_mode));
  EXPECT_EQ((HWND)1, window);
  EXPECT_TRUE(is_protected_mode);

  // Test registered cookie store.
  EXPECT_CALL(result, CookieStoreIsRegistered(_)).
      WillRepeatedly(Return(S_OK));
  EXPECT_TRUE(result.CallGetAnyWindowInStore("1low", false, &window,
                                             &is_protected_mode));
  EXPECT_EQ((HWND)1, window);
  EXPECT_TRUE(is_protected_mode);
  EXPECT_TRUE(result.CallGetAnyWindowInStore("1", true, &window,
                                             &is_protected_mode));
  EXPECT_EQ((HWND)1, window);
  EXPECT_FALSE(is_protected_mode);
}

TEST_F(CookieApiTests, CreateCookieStoreValues) {
  testing::LogDisabler no_dchecks;
  StrictMock<MockIterativeCookieApiResult> result(
      CookieApiResult::kNoRequestId);
  CookieApiResult::WindowSet windows;
  EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(_)).
      WillRepeatedly(Return(true));

  // Test empty window set.
  EXPECT_CALL(result, CallRealPostError(_)).Times(1);
  EXPECT_FALSE(result.CallCreateCookieStoreValues(2, windows));
  result.FlushAllPosts();
  EXPECT_EQ(NULL, result.value());

  // Test failed cookie store registration.
  windows.insert(kAllWindows[0]);
  EXPECT_CALL(result, RegisterCookieStore(kAllWindows[0])).
      WillOnce(Return(E_FAIL));
  // We don't need to expect a PostError here, because the mocked call to
  // RegisterCookieStore would have called PostError on failure.
  EXPECT_FALSE(result.CallCreateCookieStoreValues(2, windows));
  EXPECT_EQ(NULL, result.value());

  // Test failed tab list access.
  EXPECT_CALL(result, RegisterCookieStore(kAllWindows[0])).
      WillRepeatedly(Return(S_OK));
  EXPECT_CALL(result, GetTabListForWindow(kAllWindows[0], NotNull())).
      WillOnce(Return(false));
  EXPECT_CALL(result, CallRealPostError(_)).Times(1);
  EXPECT_FALSE(result.CallCreateCookieStoreValues(2, windows));
  result.FlushAllPosts();
  EXPECT_EQ(NULL, result.value());

  // Test success cases.
  const int tab_list_values[] = {27, 1, 123, 9};
  result.stub_.SetTabListExpectation(
      tab_list_values, arraysize(tab_list_values));
  EXPECT_CALL(result, GetTabListForWindow(_, NotNull())).
      WillRepeatedly(Invoke(
          &result.stub_, &CookieApiResultStub::GetTabListForWindow));

  // Test bad tabs.
  EXPECT_CALL(result, GetTabProtectedMode(_, _)).Times(2).
      WillRepeatedly(Return(false));
  EXPECT_TRUE(result.CallCreateCookieStoreValues(2, windows));
  EXPECT_EQ(NULL, result.value());

  // Test 2 stores.
  EXPECT_CALL(result, GetTabProtectedMode(HWND(27), _)).
      WillOnce(DoAll(SetArgumentPointee<1>(true), Return(true)));
  EXPECT_CALL(result, GetTabProtectedMode(HWND(123), _)).
      WillOnce(DoAll(SetArgumentPointee<1>(false), Return(true)));
  EXPECT_CALL(result.mock_api_dispatcher_, GetTabIdFromHandle(_)).Times(2).
      WillRepeatedly(Invoke(
          CookieApiResultStub::GetTabIdFromHandle));
  EXPECT_CALL(result, CallRealPostResult()).Times(1);
  EXPECT_TRUE(result.CallCreateCookieStoreValues(2, windows));
  result.FlushAllPosts();
  ASSERT_TRUE(result.value() != NULL &&
              result.value()->IsType(Value::TYPE_LIST));
  const ListValue* cookie_stores =
      static_cast<const ListValue*>(result.value());
  DictionaryValue* cookie_store;
  std::string id;
  ListValue* tab_list;
  int tab;
  EXPECT_TRUE(cookie_stores->GetDictionary(0, &cookie_store));
  EXPECT_TRUE(cookie_store->GetString(keys::kIdKey, &id));
  EXPECT_EQ(std::string("2"), id);
  EXPECT_TRUE(cookie_store->GetList(keys::kTabIdsKey, &tab_list));
  EXPECT_EQ(1, tab_list->GetSize());
  EXPECT_TRUE(tab_list->GetInteger(0, &tab));
  EXPECT_EQ(123, tab);

  EXPECT_TRUE(cookie_stores->GetDictionary(1, &cookie_store));
  EXPECT_TRUE(cookie_store->GetString(keys::kIdKey, &id));
  EXPECT_EQ(std::string("2low"), id);
  EXPECT_TRUE(cookie_store->GetList(keys::kTabIdsKey, &tab_list));
  EXPECT_EQ(1, tab_list->GetSize());
  EXPECT_TRUE(tab_list->GetInteger(0, &tab));
  EXPECT_EQ(27, tab);
}

TEST_F(CookieApiTests, GetTabListForWindow) {
  StrictMock<MockCookieApiResult> result(CookieApiResult::kNoRequestId);
  HWND test_frame_window = reinterpret_cast<HWND>(1);
  scoped_ptr<ListValue> tab_list;

  // Test failed executor access.
  EXPECT_CALL(result.mock_api_dispatcher_,
              GetExecutor(test_frame_window, _, NotNull())).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_FALSE(result.CallGetTabListForWindow(test_frame_window, &tab_list));

  // Test executor.
  testing::MockWindowExecutor* mock_window_executor;
  ScopedComPtr<ICeeeWindowExecutor> mock_window_executor_keeper;
  EXPECT_HRESULT_SUCCEEDED(testing::MockWindowExecutor::CreateInitialized(
      &mock_window_executor, mock_window_executor_keeper.Receive()));
  EXPECT_CALL(result.mock_api_dispatcher_,
              GetExecutor(test_frame_window, _, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<2>(
                               mock_window_executor_keeper.get()),
                           AddRef(mock_window_executor_keeper.get())));
  // Failing Executor.
  // The executor classes are already strict from their base class impl.
  EXPECT_CALL(*mock_window_executor, GetTabs(NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_FALSE(result.CallGetTabListForWindow(test_frame_window, &tab_list));
  // Success.
  BSTR tab_handles = SysAllocString(L"[27, 1, 123, 9]");
  EXPECT_CALL(*mock_window_executor, GetTabs(NotNull())).
      WillOnce(DoAll(SetArgumentPointee<0>(tab_handles), Return(S_OK)));
  EXPECT_TRUE(result.CallGetTabListForWindow(test_frame_window, &tab_list));
  // The cookie_store takes ownership of this pointer, so there's no need to
  // free it.
  tab_handles = NULL;

  EXPECT_EQ(4, tab_list->GetSize());
  int element = 0;
  EXPECT_TRUE(tab_list->GetInteger(0, &element));
  EXPECT_EQ(27, element);
  EXPECT_TRUE(tab_list->GetInteger(1, &element));
  EXPECT_EQ(1, element);
  EXPECT_TRUE(tab_list->GetInteger(2, &element));
  EXPECT_EQ(123, element);
  EXPECT_TRUE(tab_list->GetInteger(3, &element));
  EXPECT_EQ(9, element);
}

TEST_F(CookieApiTests, GetAnyTabInWindow) {
  StrictMock<MockCookieApiResult> result(CookieApiResult::kNoRequestId);
  HWND test_frame_window = reinterpret_cast<HWND>(1);

  // Test failed tab list access.
  EXPECT_CALL(result, GetTabListForWindow(test_frame_window, NotNull())).
      WillOnce(Return(false));
  EXPECT_EQ(HWND(NULL),
            result.CallGetAnyTabInWindow(test_frame_window, false));

  const int tab_list_values[] = {27, 2, 123, 1};
  result.stub_.SetTabListExpectation(
      tab_list_values, arraysize(tab_list_values));
  EXPECT_CALL(result, GetTabListForWindow(test_frame_window, NotNull())).
      WillRepeatedly(Invoke(
          &result.stub_, &CookieApiResultStub::GetTabListForWindow));

  // Test no appropriate tabs.
  EXPECT_CALL(result, GetTabProtectedMode(_, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(false), Return(true)));
  EXPECT_EQ(HWND(NULL),
            result.CallGetAnyTabInWindow(test_frame_window, true));

  EXPECT_CALL(result, GetTabProtectedMode(_, NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(true), Return(true)));
  EXPECT_EQ(HWND(NULL),
            result.CallGetAnyTabInWindow(test_frame_window, false));

  // Test success.
  EXPECT_CALL(result, GetTabProtectedMode(HWND(27), NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(true), Return(true)));
  EXPECT_CALL(result, GetTabProtectedMode(HWND(123), NotNull())).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(false), Return(true)));
  EXPECT_EQ(HWND(27),
            result.CallGetAnyTabInWindow(test_frame_window, true));
  EXPECT_EQ(HWND(123),
            result.CallGetAnyTabInWindow(test_frame_window, false));
}

TEST_F(CookieApiTests, GetTabProtectedMode) {
  StrictMock<MockCookieApiResult> result(CookieApiResult::kNoRequestId);
  HWND test_window = reinterpret_cast<HWND>(1);
  StrictMock<testing::MockWindowUtils> window_utils;
  bool is_protected_mode = false;

  // Test bad tab window.
  EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(_)).
      WillOnce(Return(false));
  EXPECT_FALSE(result.CallGetTabProtectedMode(test_window,
                                              &is_protected_mode));

  EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(_)).
      WillRepeatedly(Return(true));
  EXPECT_CALL(window_utils, WindowHasNoThread(test_window)).
      WillOnce(Return(true));
  EXPECT_FALSE(result.CallGetTabProtectedMode(test_window,
                                              &is_protected_mode));

  EXPECT_CALL(window_utils, WindowHasNoThread(test_window)).
      WillRepeatedly(Return(false));
  EXPECT_CALL(window_utils, IsWindowClass(test_window, _)).
      WillOnce(Return(false));
  EXPECT_FALSE(result.CallGetTabProtectedMode(test_window,
                                              &is_protected_mode));

  EXPECT_CALL(window_utils, IsWindowClass(test_window, _)).
      WillRepeatedly(Return(true));

  // Test failed executor access.
  EXPECT_CALL(result.mock_api_dispatcher_,
              GetExecutor(test_window, _, NotNull())).
      WillOnce(SetArgumentPointee<2>(static_cast<void*>(NULL)));
  EXPECT_FALSE(result.CallGetTabProtectedMode(test_window,
                                              &is_protected_mode));

  // Test executor.
  testing::MockTabExecutor* mock_tab_executor;
  ScopedComPtr<ICeeeTabExecutor> mock_tab_executor_keeper;
  EXPECT_HRESULT_SUCCEEDED(testing::MockTabExecutor::CreateInitialized(
      &mock_tab_executor, mock_tab_executor_keeper.Receive()));
  EXPECT_CALL(result.mock_api_dispatcher_,
              GetExecutor(test_window, _, NotNull())).
      WillRepeatedly(DoAll(
          SetArgumentPointee<2>(mock_tab_executor_keeper.get()),
          AddRef(mock_tab_executor_keeper.get())));
  // Failing Executor.
  // The executor classes are already strict from their base class impl.
  EXPECT_CALL(*mock_tab_executor, GetTabInfo(NotNull())).
      WillOnce(Return(E_FAIL));
  EXPECT_FALSE(result.CallGetTabProtectedMode(test_window,
                                              &is_protected_mode));
  // Success.
  tab_api::TabInfo tab_info;
  tab_info.protected_mode = TRUE;
  EXPECT_CALL(*mock_tab_executor, GetTabInfo(NotNull())).
      WillOnce(DoAll(SetArgumentPointee<0>(tab_info), Return(S_OK)));
  EXPECT_TRUE(result.CallGetTabProtectedMode(test_window,
                                             &is_protected_mode));
  EXPECT_TRUE(is_protected_mode);
}

TEST_F(CookieApiTests, FindAllProcessesAndWindowsNoWindows) {
  std::set<HWND> empty_window_set;
  StrictMock<testing::MockWindowUtils> window_utils;
  EXPECT_CALL(window_utils, FindTopLevelWindows(_, _)).
      WillOnce(SetArgumentPointee<1>(empty_window_set));
  EXPECT_CALL(user32_, GetWindowThreadProcessId(_, _)).Times(0);

  CookieApiResult::ProcessWindowMap all_windows;
  CookieApiResult::FindAllProcessesAndWindows(&all_windows);

  EXPECT_EQ(0, all_windows.size());
}

TEST_F(CookieApiTests, FindAllProcessesAndWindowsMultipleProcesses) {
  testing::LogDisabler no_dchecks;
  EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(_)).
      WillRepeatedly(Return(true));

  std::set<HWND> test_windows(kAllWindows, kAllWindows + kNumWindows);
  test_windows.insert(kBadWindows[0]);
  test_windows.insert(kBadWindows[1]);
  StrictMock<testing::MockWindowUtils> window_utils;

  EXPECT_CALL(window_utils, FindTopLevelWindows(_, _)).
      WillOnce(SetArgumentPointee<1>(test_windows));
  for (int i = 0; i < kNumWindows; ++i) {
    EXPECT_CALL(user32_, GetWindowThreadProcessId(kAllWindows[i], _)).
        WillOnce(DoAll(SetArgumentPointee<1>(kWindowProcesses[i]),
                       Return(kThreadId)));
  }
  // Test that threads and processes with ID 0 don't get added.
  EXPECT_CALL(user32_, GetWindowThreadProcessId(kBadWindows[0], _)).
      WillOnce(Return(0));
  EXPECT_CALL(user32_, GetWindowThreadProcessId(kBadWindows[1], _)).
      WillOnce(DoAll(SetArgumentPointee<1>(0), Return(kThreadId)));

  CookieApiResult::ProcessWindowMap all_windows;
  CookieApiResult::FindAllProcessesAndWindows(&all_windows);

  EXPECT_EQ(kNumProcesses, all_windows.size());

  CookieApiResult::WindowSet& window_set = all_windows[kAllProcesses[0]];
  EXPECT_EQ(2, window_set.size());
  EXPECT_TRUE(window_set.find(kAllWindows[0]) != window_set.end());
  EXPECT_TRUE(window_set.find(kAllWindows[2]) != window_set.end());

  window_set = all_windows[kAllProcesses[1]];
  EXPECT_EQ(1, window_set.size());
  EXPECT_TRUE(window_set.find(kAllWindows[1]) != window_set.end());

  window_set = all_windows[kAllProcesses[2]];
  EXPECT_EQ(1, window_set.size());
  EXPECT_TRUE(window_set.find(kAllWindows[3]) != window_set.end());
}

TEST_F(CookieApiTests, GetCookiesInvalidArgumentsResultInErrors) {
  testing::LogDisabler no_dchecks;
  ListValue args;
  StrictMock<MockApiInvocation<CookieApiResult,
                               MockCookieApiResult,
                               GetCookie> > invocation;
  // First test that required arguments are enforced.
  ExpectInvalidArguments(invocation, args);
  DictionaryValue* details = new DictionaryValue();
  args.Append(details);
  ExpectInvalidArguments(invocation, args);
  // TODO(cindylau@chromium.org): Check for invalid URL error.
  details->SetString(keys::kUrlKey, "helloworld");
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              PostError(StrEq(ExtensionErrorUtils::FormatErrorMessage(
                  keys::kInvalidUrlError, "helloworld")))).
      Times(1);
  invocation.Execute(args, kRequestId);
  details->SetString(keys::kUrlKey, "http://www.foobar.com");
  ExpectInvalidArguments(invocation, args);
  details->SetString(keys::kNameKey, "foo");
  details->SetInteger(keys::kStoreIdKey, 1);
  ExpectInvalidArguments(invocation, args);

  // GetAnyWindowInStore fails.
  std::string store_id("test_cookie_store");
  details->SetString(keys::kStoreIdKey, store_id);
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              GetAnyWindowInStore(StrEq(store_id), false, _, _)).
       WillOnce(Return(false));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(0);
  invocation.Execute(args, kRequestId);
}

TEST_F(CookieApiTests, GetCookiesTopIeWindowIsDefaultCookieStore) {
  // TODO(cindylau@chromium.org): The expected behavior here will
  // change; we should use the current execution context's cookie
  // store, not the top window.
  testing::LogDisabler no_dchecks;
  StrictMock<MockApiInvocation<CookieApiResult,
                               MockCookieApiResult,
                               GetCookie> > invocation;

  ListValue args;
  DictionaryValue* details = new DictionaryValue();
  args.Append(details);
  details->SetString(keys::kUrlKey, "http://www.foobar.com");
  details->SetString(keys::kNameKey, "foo");

  invocation.AllocateNewResult(kRequestId);
  MockWindowApiResultStatics window_statics;
  EXPECT_CALL(window_statics, TopIeWindow()).WillOnce(Return(HWND(42)));
  EXPECT_CALL(*invocation.invocation_result_, GetCookieInfo(_, _, _, _, _)).
      WillOnce(Return(S_OK));
  EXPECT_CALL(*invocation.invocation_result_, PostResult()).Times(1);
  invocation.Execute(args, kRequestId);
}

TEST_F(CookieApiTests, GetAllCookieStores) {
  testing::LogDisabler no_dchecks;
  EXPECT_CALL(*executors_manager_, IsKnownWindowImpl(_)).
      WillRepeatedly(Return(true));

  StrictMock<MockCookieApiResultStatics> result_statics;
  CookieApiResult::ProcessWindowMap all_windows;
  ListValue args;

  StrictMock<MockApiInvocation<IterativeCookieApiResult,
                               MockIterativeCookieApiResult,
                               GetAllCookieStores> > invocation;

  // First test the trivial case of no cookie stores. One call to success and
  // no calls to error function.
  EXPECT_CALL(result_statics, FindAllProcessesAndWindows(_)).
      WillOnce(SetArgumentPointee<0>(all_windows));
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostResult()).Times(1);
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostError(_)).Times(0);
  invocation.Execute(args, kRequestId);

  // Now test cases with multiple windows.
  for (int i = 0; i < kNumWindows; ++i) {
    all_windows[kWindowProcesses[i]].insert(kAllWindows[i]);
  }
  EXPECT_CALL(result_statics, FindAllProcessesAndWindows(_)).
      WillRepeatedly(SetArgumentPointee<0>(all_windows));

  // Test error case: can't access a single cookie store.
  MockIterativeCookieApiResult::ResetCounters();
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, CreateCookieStoreValues(_, _)).
      WillRepeatedly(Return(false));
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostResult()).Times(0);
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostError(_)).Times(1);
  invocation.Execute(args, kRequestId);

  // Test error case: getting window tabs fails each time and so an error is
  // reported. Count errors and make sure everything is reported as error.
  MockIterativeCookieApiResult::ResetCounters();
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_, CreateCookieStoreValues(_, _)).
      WillRepeatedly(Invoke(invocation.invocation_result_.get(),
          &MockIterativeCookieApiResult::CallCreateCookieStoreValues));
  EXPECT_CALL(*invocation.invocation_result_, RegisterCookieStore(_)).
      WillRepeatedly(Return(S_OK));
  EXPECT_CALL(*invocation.invocation_result_,
              GetTabListForWindow(_, NotNull())).WillRepeatedly(Return(false));
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostResult()).Times(0);
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostError(_)).Times(1);
  invocation.Execute(args, kRequestId);
  EXPECT_EQ(MockIterativeCookieApiResult::success_counter(), 0);
  EXPECT_EQ(MockIterativeCookieApiResult::error_counter(), all_windows.size());

  // Now test the case of multiple open windows/processes. Each invocation is
  // successful and a result is produced.
  MockIterativeCookieApiResult::ResetCounters();
  invocation.AllocateNewResult(kRequestId);

  const int tab_list_values[] = {27, 1};
  invocation.invocation_result_->stub_.SetTabListExpectation(
      tab_list_values, arraysize(tab_list_values));
  EXPECT_CALL(*invocation.invocation_result_,
              GetTabListForWindow(_, NotNull())).
      WillRepeatedly(Invoke(
          &invocation.invocation_result_->stub_,
          &CookieApiResultStub::GetTabListForWindow));
  EXPECT_CALL(*invocation.invocation_result_, CreateCookieStoreValues(_, _)).
      WillRepeatedly(Invoke(invocation.invocation_result_.get(),
          &MockIterativeCookieApiResult::CallCreateCookieStoreValues));
  EXPECT_CALL(*invocation.invocation_result_, RegisterCookieStore(_)).
      WillRepeatedly(Return(S_OK));
  EXPECT_CALL(*invocation.invocation_result_, GetTabProtectedMode(_, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(false), Return(true)));
  EXPECT_CALL(invocation.invocation_result_->mock_api_dispatcher_,
              GetTabIdFromHandle(_)).
      WillRepeatedly(Invoke(
          CookieApiResultStub::GetTabIdFromHandle));
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostResult()).Times(1);
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostError(_)).Times(0);
  invocation.Execute(args, kRequestId);
  EXPECT_EQ(MockIterativeCookieApiResult::success_counter(),
            all_windows.size());
  EXPECT_EQ(MockIterativeCookieApiResult::error_counter(), 0);

  // Now test the case of multiple open windows/processes. One invocation
  // fails, but everything else will be OK.
  MockIterativeCookieApiResult::ResetCounters();
  invocation.AllocateNewResult(kRequestId);

  invocation.invocation_result_->stub_.SetTabListExpectation(
      tab_list_values, arraysize(tab_list_values));
  EXPECT_CALL(*invocation.invocation_result_,
              GetTabListForWindow(_, NotNull())).
      WillRepeatedly(Invoke(
          &invocation.invocation_result_->stub_,
          &CookieApiResultStub::GetTabListForWindow));
  EXPECT_CALL(*invocation.invocation_result_,
              GetTabListForWindow(kAllWindows[0], NotNull())).
      WillOnce(Return(false));
  EXPECT_CALL(*invocation.invocation_result_, CreateCookieStoreValues(_, _)).
      WillRepeatedly(Invoke(invocation.invocation_result_.get(),
          &MockIterativeCookieApiResult::CallCreateCookieStoreValues));
  EXPECT_CALL(*invocation.invocation_result_, RegisterCookieStore(_)).
      WillRepeatedly(Return(S_OK));
  EXPECT_CALL(*invocation.invocation_result_, GetTabProtectedMode(_, _)).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(false), Return(true)));
  EXPECT_CALL(invocation.invocation_result_->mock_api_dispatcher_,
              GetTabIdFromHandle(_)).
      WillRepeatedly(Invoke(
          CookieApiResultStub::GetTabIdFromHandle));
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostResult()).Times(1);
  EXPECT_CALL(*invocation.invocation_result_, CallRealPostError(_)).Times(0);
  invocation.Execute(args, kRequestId);
  EXPECT_EQ(MockIterativeCookieApiResult::success_counter(),
            all_windows.size() - 1);
  EXPECT_EQ(MockIterativeCookieApiResult::error_counter(), 1);
}

TEST_F(CookieApiTests, GetAllCookiesNotImplemented) {
  testing::LogDisabler no_dchecks;
  ListValue args;
  StrictMock<MockApiInvocation<CookieApiResult,
                               MockCookieApiResult,
                               GetAllCookies> > invocation;

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              PostError(StrEq("Not implemented."))).Times(1);
  invocation.Execute(args, kRequestId);
}

TEST_F(CookieApiTests, SetCookieNotImplemented) {
  testing::LogDisabler no_dchecks;
  ListValue args;
  StrictMock<MockApiInvocation<CookieApiResult,
                               MockCookieApiResult,
                               SetCookie> > invocation;

  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              PostError(StrEq("Not implemented."))).Times(1);
  invocation.Execute(args, kRequestId);
}

TEST_F(CookieApiTests, RemoveCookieNotImplemented) {
  testing::LogDisabler no_dchecks;
  ListValue args;
  StrictMock<MockApiInvocation<CookieApiResult,
                               MockCookieApiResult,
                               RemoveCookie> > invocation;
  invocation.AllocateNewResult(kRequestId);
  EXPECT_CALL(*invocation.invocation_result_,
              PostError(StrEq("Not implemented."))).Times(1);
  invocation.Execute(args, kRequestId);
}

TEST_F(CookieApiTests, CookieChangedEventHandler) {
  testing::LogDisabler no_dchecks;
  MockCookieChanged cookie_changed;
  std::string converted_args;
  // Empty args.
  std::string input_args = "";
  EXPECT_EQ(false,
            cookie_changed.EventHandlerImpl(input_args, &converted_args));
  // Invalid args.
  input_args = "[false, {hello]";
  EXPECT_EQ(false,
            cookie_changed.EventHandlerImpl(input_args, &converted_args));
  input_args = "[3]";
  EXPECT_EQ(false,
            cookie_changed.EventHandlerImpl(input_args, &converted_args));
  // Valid args.
  input_args = "[{\"removed\": false, \"cookie\": {\"storeId\": \"1\"}}]";

  // Invalid store ID.
  cookie_changed.AllocateApiResult();
  EXPECT_CALL(*cookie_changed.api_result_,
              GetAnyWindowInStore(StrEq("1"), true, _, _))
      .WillOnce(Return(false));
  EXPECT_EQ(false,
            cookie_changed.EventHandlerImpl(input_args, &converted_args));

  // Cookie store access errors.
  cookie_changed.AllocateApiResult();
  EXPECT_CALL(*cookie_changed.api_result_,
              GetAnyWindowInStore(StrEq("1"), true, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(HWND(5)),
                      SetArgumentPointee<3>(false),
                      Return(true)));
  EXPECT_CALL(*cookie_changed.api_result_,
              CookieStoreIsRegistered(HWND(5))).WillOnce(Return(E_FAIL));
  EXPECT_EQ(false,
            cookie_changed.EventHandlerImpl(input_args, &converted_args));

  cookie_changed.AllocateApiResult();
  EXPECT_CALL(*cookie_changed.api_result_,
              GetAnyWindowInStore(StrEq("1"), true, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(HWND(5)),
                      SetArgumentPointee<3>(false),
                      Return(true)));
  EXPECT_CALL(*cookie_changed.api_result_,
              CookieStoreIsRegistered(HWND(5))).WillOnce(Return(S_FALSE));
  EXPECT_CALL(*cookie_changed.api_result_,
              RegisterCookieStore(HWND(5))).WillOnce(Return(E_FAIL));
  EXPECT_EQ(false,
            cookie_changed.EventHandlerImpl(input_args, &converted_args));

  // Registered cookie store.
  cookie_changed.AllocateApiResult();
  EXPECT_CALL(*cookie_changed.api_result_,
              GetAnyWindowInStore(StrEq("1"), true, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(HWND(5)),
                      SetArgumentPointee<3>(false),
                      Return(true)));
  EXPECT_CALL(*cookie_changed.api_result_,
              CookieStoreIsRegistered(HWND(5))).WillOnce(Return(S_OK));
  EXPECT_EQ(true,
            cookie_changed.EventHandlerImpl(input_args, &converted_args));

  // Unregistered cookie store.
  cookie_changed.AllocateApiResult();
  EXPECT_CALL(*cookie_changed.api_result_,
              GetAnyWindowInStore(StrEq("1"), true, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(HWND(5)),
                      SetArgumentPointee<3>(false),
                      Return(true)));
  EXPECT_CALL(*cookie_changed.api_result_,
              CookieStoreIsRegistered(HWND(5))).WillOnce(Return(S_FALSE));
  EXPECT_CALL(*cookie_changed.api_result_,
              RegisterCookieStore(HWND(5))).WillOnce(Return(S_OK));
  EXPECT_EQ(true,
            cookie_changed.EventHandlerImpl(input_args, &converted_args));
}

}  // namespace
