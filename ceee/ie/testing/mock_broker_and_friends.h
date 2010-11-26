// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock implementation of Broker and related objects.
#ifndef CEEE_IE_TESTING_MOCK_BROKER_AND_FRIENDS_H_
#define CEEE_IE_TESTING_MOCK_BROKER_AND_FRIENDS_H_

#include <string>

#include "ceee/ie/broker/api_dispatcher.h"
#include "ceee/ie/broker/broker_rpc_client.h"
#include "ceee/ie/broker/executors_manager.h"
#include "ceee/ie/broker/window_events_funnel.h"
#include "ceee/ie/plugin/bho/cookie_events_funnel.h"
#include "ceee/ie/plugin/bho/tab_events_funnel.h"
#include "ceee/ie/plugin/bho/webnavigation_events_funnel.h"
#include "ceee/ie/plugin/bho/webrequest_events_funnel.h"
#include "gmock/gmock.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/instance_count_mixin.h"

#include "broker_lib.h"  // NOLINT
#include "toolband.h"  // NOLINT

namespace testing {

class MockBrokerImpl : public ICeeeBroker {
 public:
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, Execute, HRESULT(BSTR, BSTR*));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, FireEvent, HRESULT(BSTR, BSTR));
};

class MockBrokerRegistrarImpl : public ICeeeBrokerRegistrar {
 public:
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, RegisterWindowExecutor,
                             HRESULT(long, IUnknown*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, UnregisterExecutor, HRESULT(long));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, RegisterTabExecutor,
                             HRESULT(long, IUnknown*));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, SetTabIdForHandle,
                             HRESULT(long, CeeeWindowHandle));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, SetTabToolBandIdForHandle,
                             HRESULT(long, CeeeWindowHandle));
};

class MockBroker
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockBroker>,
      public InstanceCountMixin<MockBroker>,
      public StrictMock<MockBrokerRegistrarImpl>,
      public StrictMock<MockBrokerImpl> {
 public:
  BEGIN_COM_MAP(MockBroker)
    COM_INTERFACE_ENTRY(ICeeeBrokerRegistrar)
    COM_INTERFACE_ENTRY(ICeeeBroker)
  END_COM_MAP()

  HRESULT Initialize(MockBroker** self) {
    *self = this;
    return S_OK;
  }
};

class MockExecutorIUnknown
  : public CComObjectRootEx<CComSingleThreadModel>,
    public InitializingCoClass<MockExecutorIUnknown>,
    public InstanceCountMixin<MockExecutorIUnknown>,
    public IObjectWithSiteImpl<MockExecutorIUnknown> {
 public:
  BEGIN_COM_MAP(MockExecutorIUnknown)
    COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  HRESULT Initialize(MockExecutorIUnknown** self) {
    *self = this;
    return S_OK;
  }
};

class MockCeeeWindowExecutorImpl : public ICeeeWindowExecutor {
 public:
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, Initialize, HRESULT(CeeeWindowHandle));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, GetWindow,
                             HRESULT(BOOL, CeeeWindowInfo*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetTabs, HRESULT(BSTR*));
  MOCK_METHOD5_WITH_CALLTYPE(__stdcall, UpdateWindow, HRESULT(long, long,
      long, long, CeeeWindowInfo*));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, RemoveWindow, HRESULT());
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, GetTabIndex, HRESULT(CeeeWindowHandle,
      long*));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, MoveTab, HRESULT(CeeeWindowHandle,
      long));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, RemoveTab, HRESULT(CeeeWindowHandle));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SelectTab, HRESULT(CeeeWindowHandle));
};

class MockWindowExecutor
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockWindowExecutor>,
      public InstanceCountMixin<MockWindowExecutor>,
      public IObjectWithSiteImpl<MockWindowExecutor>,
      public StrictMock<MockCeeeWindowExecutorImpl> {
 public:
  BEGIN_COM_MAP(MockWindowExecutor)
    COM_INTERFACE_ENTRY(ICeeeWindowExecutor)
    COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  HRESULT Initialize(MockWindowExecutor** self) {
    *self = this;
    return S_OK;
  }
};

class MockCeeeTabExecutorImpl : public ICeeeTabExecutor {
 public:
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, Initialize, HRESULT(CeeeWindowHandle));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetTabInfo,
                             HRESULT(CeeeTabInfo*));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, Navigate, HRESULT(BSTR, long, BSTR));
  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, InsertCode, HRESULT(
      BSTR, BSTR, BOOL, CeeeTabCodeType));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, PopupizeFrameWindow, HRESULT(long));
};

class MockTabExecutor
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockTabExecutor>,
      public InstanceCountMixin<MockTabExecutor>,
      public IObjectWithSiteImpl<MockTabExecutor>,
      public StrictMock<MockCeeeTabExecutorImpl> {
 public:
  BEGIN_COM_MAP(MockTabExecutor)
    COM_INTERFACE_ENTRY(ICeeeTabExecutor)
    COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  HRESULT Initialize(MockTabExecutor** self) {
    *self = this;
    return S_OK;
  }
};

// Lets us test protected members and mock certain calls.
class MockExecutorsManager : public ExecutorsManager {
 public:
  // true for no thread.
  MockExecutorsManager() : ExecutorsManager(true) {
  }
  // Publicized to get/set the instance to be used a the singleton.
  using ExecutorsManager::test_instance_;
  MOCK_METHOD2(RegisterWindowExecutor, HRESULT(ThreadId thread_id,
                                               IUnknown* executor));
  MOCK_METHOD2(RegisterTabExecutor, HRESULT(ThreadId thread_id,
                                            IUnknown* executor));
  MOCK_METHOD1(RemoveExecutor, HRESULT(ThreadId thread_id));

  MOCK_METHOD2(SetTabIdForHandle, void(int tab_id, HWND tab_handle));
  MOCK_METHOD2(SetTabToolBandIdForHandle, void(int tool_band_id,
                                               HWND tab_handle));

  MOCK_METHOD1(IsKnownWindowImpl, bool(HWND));
  MOCK_METHOD1(FindTabChildImpl, HWND(HWND));
};

class MockTabEventsFunnel : public TabEventsFunnel {
 public:
  MOCK_METHOD4(OnMoved, HRESULT(HWND tab, int window_id, int from_index,
                                int to_index));
  MOCK_METHOD1(OnRemoved, HRESULT(HWND tab));
  MOCK_METHOD2(OnSelectionChanged, HRESULT(HWND tab, int window_id));
  MOCK_METHOD3(OnCreated, HRESULT(HWND tab, BSTR url, bool completed));
  MOCK_METHOD3(OnUpdated, HRESULT(HWND tab, BSTR url,
                                  READYSTATE ready_state));
  MOCK_METHOD2(OnTabUnmapped, HRESULT(HWND tab, int tab_id));
};

class MockWindowEventsFunnel : public WindowEventsFunnel {
 public:
  MOCK_METHOD1(OnCreated, HRESULT(HWND));
  MOCK_METHOD1(OnFocusChanged, HRESULT(int));
  MOCK_METHOD1(OnRemoved, HRESULT(HWND));
};

class MockCeeeCookieExecutorImpl : public ICeeeCookieExecutor {
 public:
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, GetCookie,
                             HRESULT(BSTR, BSTR, CeeeCookieInfo*));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, RegisterCookieStore, HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, CookieStoreIsRegistered, HRESULT());
};

class MockCookieExecutor
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockCookieExecutor>,
      public InstanceCountMixin<MockCookieExecutor>,
      public IObjectWithSiteImpl<MockCookieExecutor>,
      public StrictMock<MockCeeeCookieExecutorImpl> {
 public:
  BEGIN_COM_MAP(MockCookieExecutor)
    COM_INTERFACE_ENTRY(ICeeeCookieExecutor)
    COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  HRESULT Initialize(MockCookieExecutor** self) {
    *self = this;
    return S_OK;
  }
};

class MockCookieEventsFunnel : public CookieEventsFunnel {
 public:
  MOCK_METHOD2(OnChanged, HRESULT(bool removed,
                                  const cookie_api::CookieInfo& cookie));
};

class MockCeeeInfobarExecutorImpl : public ICeeeInfobarExecutor {
 public:
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SetExtensionId, HRESULT(BSTR));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, ShowInfobar,
                             HRESULT(BSTR, CeeeWindowHandle*));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnTopFrameBeforeNavigate,
                             HRESULT(BSTR));
};

class MockInfobarExecutor
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockInfobarExecutor>,
      public InstanceCountMixin<MockInfobarExecutor>,
      public IObjectWithSiteImpl<MockInfobarExecutor>,
      public StrictMock<MockCeeeInfobarExecutorImpl> {
 public:
  BEGIN_COM_MAP(MockInfobarExecutor)
    COM_INTERFACE_ENTRY(ICeeeInfobarExecutor)
    COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  HRESULT Initialize(MockInfobarExecutor** self) {
    *self = this;
    return S_OK;
  }
};

class MockTabInfobarExecutor
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockTabInfobarExecutor>,
      public InstanceCountMixin<MockTabInfobarExecutor>,
      public IObjectWithSiteImpl<MockTabInfobarExecutor>,
      public StrictMock<MockCeeeTabExecutorImpl>,
      public StrictMock<MockCeeeInfobarExecutorImpl> {
 public:
  BEGIN_COM_MAP(MockTabInfobarExecutor)
    COM_INTERFACE_ENTRY(ICeeeTabExecutor)
    COM_INTERFACE_ENTRY(ICeeeInfobarExecutor)
    COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  HRESULT Initialize(MockTabInfobarExecutor** self) {
    *self = this;
    return S_OK;
  }
};

class MockApiDispatcher : public ApiDispatcher {
 public:
  MOCK_METHOD2(HandleApiRequest, void(BSTR, BSTR*));
  MOCK_METHOD2(RegisterInvocation, void(const char* function_name,
                                        InvocationFactory factory));
  MOCK_METHOD3(RegisterEphemeralEventHandler,
      void(const char*, EphemeralEventHandler, InvocationResult*));
  MOCK_METHOD3(GetExecutor, void(HWND, REFIID, void**));
  MOCK_METHOD2(FireEvent, void(BSTR event_name, BSTR event_args));

  MOCK_CONST_METHOD1(GetTabHandleFromId, HWND(int));
  MOCK_CONST_METHOD1(GetTabHandleFromToolBandId, HWND(int));
  MOCK_CONST_METHOD1(GetWindowHandleFromId, HWND(int));
  MOCK_CONST_METHOD1(GetTabIdFromHandle, int(HWND));
  MOCK_CONST_METHOD1(GetWindowIdFromHandle, int(HWND));
};

// A mock class for an API invocation class that derives from ApiResultCreator,
// defined in ceee\ie\broker\api_dispatcher.h. This class enables the injection
// of a mock API result instance when the API is invoked.
template<class ResultType, class MockResultType, class BaseClass>
class MockApiInvocation : public BaseClass {
 public:
  MockApiInvocation() : request_id_(kRequestId) {}

  // Calls the ContinueExecution method of the base class, using the mock
  // invocation result and mock API dispatcher.
  HRESULT CallContinueExecution(const std::string& input_args) {
    HRESULT hr = ContinueExecution(input_args, invocation_result_.get(),
                                   GetDispatcher());

    // NOTE: the ContinueExecution method has already deleted the invocation
    // result if its return is not S_FALSE. In that case, we have to release
    // the pointer so that we don't delete the same object twice.
    if (hr != S_FALSE)
      invocation_result_.release();

    return hr;
  }

  // We need to create the results before we get asked for one
  // so that we can set expectations on it. And we need to allocate them
  // because the callers of CreateApiResult take ownership of the memory.
  void AllocateNewResult(int request_id) {
    request_id_ = request_id;
    invocation_result_.reset(new StrictMock<MockResultType>(request_id));
  }
  virtual ResultType* CreateApiResult(int request_id) {
    EXPECT_EQ(request_id, request_id_);
    EXPECT_NE(static_cast<ResultType*>(NULL), invocation_result_.get());
    return invocation_result_.release();  // The caller becomes the owner.
  }
  virtual ApiDispatcher* GetDispatcher() {
    return &mock_api_dispatcher_;
  }

  // public so that the tests can set expectations on them.
  scoped_ptr<StrictMock<MockResultType> > invocation_result_;
  StrictMock<MockApiDispatcher> mock_api_dispatcher_;

 private:
  int request_id_;
};

class MockWebNavigationEventsFunnel : public WebNavigationEventsFunnel {
 public:
  MOCK_METHOD5(OnBeforeNavigate, HRESULT(CeeeWindowHandle tab_handle,
                                         BSTR url,
                                         int frame_id,
                                         int request_id,
                                         const base::Time& time_stamp));
  MOCK_METHOD4(OnBeforeRetarget, HRESULT(CeeeWindowHandle source_tab_handle,
                                         BSTR source_url,
                                         BSTR target_url,
                                         const base::Time& time_stamp));
  MOCK_METHOD6(OnCommitted, HRESULT(CeeeWindowHandle tab_handle,
                                    BSTR url,
                                    int frame_id,
                                    const char* transition_type,
                                    const char* transition_qualifiers,
                                    const base::Time& time_stamp));
  MOCK_METHOD4(OnCompleted, HRESULT(CeeeWindowHandle tab_handle,
                                    BSTR url,
                                    int frame_id,
                                    const base::Time& time_stamp));
  MOCK_METHOD4(OnDOMContentLoaded, HRESULT(CeeeWindowHandle tab_handle,
                                           BSTR url,
                                           int frame_id,
                                           const base::Time& time_stamp));
  MOCK_METHOD5(OnErrorOccurred, HRESULT(CeeeWindowHandle tab_handle,
                                        BSTR url,
                                        int frame_id,
                                        BSTR error,
                                        const base::Time& time_stamp));
};

class MockWebRequestEventsFunnel : public WebRequestEventsFunnel {
 public:
  MOCK_METHOD5(OnBeforeRedirect, HRESULT(int request_id,
                                         const wchar_t* url,
                                         DWORD status_code,
                                         const wchar_t* redirect_url,
                                         const base::Time& time_stamp));
  MOCK_METHOD6(OnBeforeRequest, HRESULT(int request_id,
                                        const wchar_t* url,
                                        const char* method,
                                        CeeeWindowHandle tab_handle,
                                        const char* type,
                                        const base::Time& time_stamp));
  MOCK_METHOD4(OnCompleted, HRESULT(int request_id,
                                    const wchar_t* url,
                                    DWORD status_code,
                                    const base::Time& time_stamp));
  MOCK_METHOD4(OnErrorOccurred, HRESULT(int request_id,
                                        const wchar_t* url,
                                        const wchar_t* error,
                                        const base::Time& time_stamp));
  MOCK_METHOD4(OnHeadersReceived, HRESULT(int request_id,
                                          const wchar_t* url,
                                          DWORD status_code,
                                          const base::Time& time_stamp));
  MOCK_METHOD4(OnRequestSent, HRESULT(int request_id,
                                      const wchar_t* url,
                                      const char* ip,
                                      const base::Time& time_stamp));
};

class MockBrokerRpcClient : public BrokerRpcClient {
 public:
  MOCK_METHOD1(Connect, HRESULT(bool));
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(is_connected, bool());
  MOCK_METHOD2(FireEvent, HRESULT(const char*, const char*));
  MOCK_METHOD2(SendUmaHistogramTimes, HRESULT(const char*, int));
  MOCK_METHOD5(SendUmaHistogramData, HRESULT(const char*, int, int, int, int));
};


}  // namespace testing

#endif  // CEEE_IE_TESTING_MOCK_BROKER_AND_FRIENDS_H_
