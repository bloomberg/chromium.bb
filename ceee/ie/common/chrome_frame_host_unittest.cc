// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for chrome frame host.
#include "ceee/ie/common/chrome_frame_host.h"

#include "base/string_util.h"
#include "ceee/ie/common/mock_ceee_module_util.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/mock_static.h"
#include "ceee/testing/utils/dispex_mocks.h"
#include "chrome/common/url_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::FireEvent;

using testing::_;
using testing::CopyInterfaceToArgument;
using testing::DoAll;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;
using testing::MockDispatchEx;

class MockChromeFrameHost : public ChromeFrameHost {
 public:
  // Mock the creator functions to control them.
  MOCK_METHOD1(CreateChromeFrame, HRESULT(IChromeFrame** chrome_frame));
  // And the event handlers to test event subscription.
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnCfLoad, void(IDispatch* event));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnCfLoadError, void(IDispatch* event));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnCfMessage, void(IDispatch* event));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnCfReadyStateChanged, void(LONG));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall,
                             OnCfPrivateMessage,
                             void(IDispatch* event, BSTR origin));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall,
                             OnCfExtensionReady,
                             void(BSTR path, int response));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnCfGetEnabledExtensionsComplete,
                             void(SAFEARRAY* extensions));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, OnCfChannelError, void(void));
};

class TestChromeFrameHost
    : public StrictMock<MockChromeFrameHost>,
      public InitializingCoClass<TestChromeFrameHost> {
 public:
  using InitializingCoClass<TestChromeFrameHost>::CreateInitialized;

  TestChromeFrameHost() : active_x_host_creation_error_(S_OK) {
    ++instance_count_;
  }
  ~TestChromeFrameHost() {
    --instance_count_;
  }

  void set_active_x_host_creation_error(HRESULT hr) {
    active_x_host_creation_error_ = hr;
  }

  HRESULT Initialize(TestChromeFrameHost** self) {
    *self = this;
    return S_OK;
  }


  HRESULT CreateActiveXHost(IAxWinHostWindow** host) {
    if (FAILED(active_x_host_creation_error_))
      return active_x_host_creation_error_;

    return ChromeFrameHost::CreateActiveXHost(host);
  }

  // For some of the mocks, we want to test the original behavior.
  void CallOnCfLoad(IDispatch* event) {
    ChromeFrameHost::OnCfLoad(event);
  }
  void CallOnCfReadyStateChanged(LONG state) {
    ChromeFrameHost::OnCfReadyStateChanged(state);
  }
  void CallOnCfPrivateMessage(IDispatch* event, BSTR origin) {
    ChromeFrameHost::OnCfPrivateMessage(event, origin);
  }
  void CallOnCfExtensionReady(BSTR path, int response) {
    ChromeFrameHost::OnCfExtensionReady(path, response);
  }
  void CallOnCfGetEnabledExtensionsComplete(SAFEARRAY* enabled_extensions) {
    ChromeFrameHost::OnCfGetEnabledExtensionsComplete(enabled_extensions);
  }
  void CallOnCfChannelError() {
    ChromeFrameHost::OnCfChannelError();
  }

 public:
  HRESULT active_x_host_creation_error_;

  static size_t instance_count_;
};

size_t TestChromeFrameHost::instance_count_ = 0;

class IChromeFrameImpl : public IDispatchImpl<IChromeFrame,
                                              &IID_IChromeFrame,
                                              &LIBID_ChromeTabLib> {
 public:
  // @name IChromeFrame implementation
  // @{
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, get_src, HRESULT(BSTR *src));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, put_src, HRESULT(BSTR src));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, postMessage, HRESULT(
      BSTR message, VARIANT target));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, get_onload, HRESULT(
      VARIANT *onload_handler));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, put_onload, HRESULT(
      VARIANT onload_handler));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, get_onloaderror, HRESULT(
      VARIANT *onerror_handler));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, put_onloaderror, HRESULT(
      VARIANT onerror_handler));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, get_onmessage, HRESULT(
      VARIANT *onmessage_handler));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, put_onmessage, HRESULT(
      VARIANT onmessage_handler));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, get_readyState, HRESULT(
      LONG *ready_state));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, addEventListener, HRESULT(
      BSTR event_type, IDispatch *listener, VARIANT use_capture));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, removeEventListener, HRESULT(
      BSTR event_type, IDispatch *listener, VARIANT use_capture));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, get_version, HRESULT(BSTR *version));
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, postPrivateMessage, HRESULT(
      BSTR message, BSTR origin, BSTR target));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, get_useChromeNetwork, HRESULT(
      VARIANT_BOOL *pVal));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, put_useChromeNetwork, HRESULT(
      VARIANT_BOOL newVal));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, installExtension, HRESULT(
      BSTR crx_path));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, loadExtension, HRESULT(
      BSTR path));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, getEnabledExtensions, HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, registerBhoIfNeeded, HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, getSessionId, HRESULT(int*));
  // @}
};

class MockChromeFrame
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass< StrictMock<MockChromeFrame> >,
      public IObjectWithSiteImpl<MockChromeFrame>,
      public StrictMock<IChromeFrameImpl>,
      public IConnectionPointContainerImpl<MockChromeFrame>,
      public IConnectionPointImpl<MockChromeFrame, &DIID_DIChromeFrameEvents> {
 public:
  BEGIN_COM_MAP(MockChromeFrame)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IChromeFrame)
    COM_INTERFACE_ENTRY(IConnectionPointContainer)
  END_COM_MAP()

  BEGIN_CONNECTION_POINT_MAP(MockChromeFrame)
    CONNECTION_POINT_ENTRY(DIID_DIChromeFrameEvents)
  END_CONNECTION_POINT_MAP()

  MockChromeFrame() : no_events_(false) {
    ++instance_count_;
  }
  ~MockChromeFrame() {
    --instance_count_;
  }

  void set_no_events(bool no_events) {
    no_events_ = no_events;
  }

  HRESULT Initialize(MockChromeFrame** self) {
    *self = this;
    return S_OK;
  }

  // Override from IConnectionPointContainerImpl
  STDMETHOD(FindConnectionPoint)(REFIID iid, IConnectionPoint** cp) {
    typedef IConnectionPointContainerImpl<MockChromeFrame> CPC;

    if (iid == DIID_DIChromeFrameEvents && no_events_)
      return CONNECT_E_NOCONNECTION;

    return CPC::FindConnectionPoint(iid, cp);
  }

  typedef IConnectionPointImpl<MockChromeFrame, &DIID_DIChromeFrameEvents> CP;
  void FireEvent1(DISPID id, IDispatch* event) {
    return FireEvent(static_cast<CP*>(this), id, 1, &CComVariant(event));
  }

  void FireCfLoad(IDispatch* event) {
    FireEvent1(CF_EVENT_DISPID_ONLOAD, event);
  }

  void FireCfLoadError(IDispatch* event) {
    FireEvent1(CF_EVENT_DISPID_ONLOADERROR, event);
  }

  void FireCfMessage(IDispatch* event) {
    FireEvent1(CF_EVENT_DISPID_ONMESSAGE, event);
  }

  void FireCfReadyStateChanged(LONG ready_state) {
    return FireEvent(static_cast<CP*>(this),
                     CF_EVENT_DISPID_ONREADYSTATECHANGED,
                     1,
                     &CComVariant(ready_state));
  }

  void FireCfPrivateMessage(IDispatch* event, BSTR origin) {
    CComVariant args[] = { origin, event };
    return FireEvent(static_cast<CP*>(this),
                     CF_EVENT_DISPID_ONPRIVATEMESSAGE,
                     arraysize(args),
                     args);
  }

  void FireCfExtensionReady(BSTR path, int response) {
    CComVariant args[] = { response , path};
    return FireEvent(static_cast<CP*>(this),
                     CF_EVENT_DISPID_ONEXTENSIONREADY,
                     arraysize(args),
                     args);
  }

  void FireCfGetEnabledExtensionsComplete(SAFEARRAY* array) {
    VARIANT args[] = { { VT_ARRAY | VT_BSTR } };
    return FireEvent(static_cast<CP*>(this),
                     CF_EVENT_DISPID_ONGETENABLEDEXTENSIONSCOMPLETE,
                     arraysize(args),
                     args);
  }

  void FireCfChannelError() {
    return FireEvent(static_cast<CP*>(this),
                     CF_EVENT_DISPID_ONCHANNELERROR,
                     0,
                     NULL);
  }

 public:
  // Quench our event sink.
  bool no_events_;

  static size_t instance_count_;
};

size_t MockChromeFrame::instance_count_ = 0;

class IChromeFrameHostEventsMockImpl : public IChromeFrameHostEvents {
 public:
  MOCK_METHOD3(OnCfPrivateMessage, HRESULT(BSTR, BSTR, BSTR));
  MOCK_METHOD2(OnCfExtensionReady, HRESULT(BSTR, int));
  MOCK_METHOD1(OnCfGetEnabledExtensionsComplete,
               HRESULT(SAFEARRAY* extensions));
  MOCK_METHOD1(OnCfGetExtensionApisToAutomate,
               HRESULT(BSTR* enabled_functions));
  MOCK_METHOD1(OnCfReadyStateChanged, HRESULT(LONG));
  MOCK_METHOD0(OnCfChannelError, HRESULT(void));
};

class MockChromeFrameHostEvents
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<MockChromeFrameHostEvents>,
      public StrictMock<IChromeFrameHostEventsMockImpl> {
 public:
  BEGIN_COM_MAP(MockChromeFrameHostEvents)
    COM_INTERFACE_ENTRY(IUnknown)
  END_COM_MAP()

  HRESULT Initialize(MockChromeFrameHostEvents** self) {
    *self = this;
    return S_OK;
  }
};

class ChromeFrameHostTest: public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(
        TestChromeFrameHost::CreateInitialized(&host_, &host_keeper_));
  }

  virtual void TearDown() {
    if (host_)
      host_->TearDown();
    host_ = NULL;
    host_keeper_.Release();

    chrome_frame_ = NULL;
    chrome_frame_keeper_ = NULL;

    ASSERT_EQ(0, TestChromeFrameHost::instance_count_);
    ASSERT_EQ(0, MockChromeFrame::instance_count_);
  }

  void ExpectCreateChromeFrame(HRESULT hr) {
    if (SUCCEEDED(hr)) {
      ASSERT_HRESULT_SUCCEEDED(
          MockChromeFrame::CreateInitialized(&chrome_frame_,
                                             &chrome_frame_keeper_));
      EXPECT_CALL(*host_, CreateChromeFrame(_)).
          WillRepeatedly(
            DoAll(
                CopyInterfaceToArgument<0>(chrome_frame_keeper_),
                Return(S_OK)));
    } else {
      EXPECT_CALL(*host_, CreateChromeFrame(_)).
          WillRepeatedly(Return(hr));
    }
  }

 public:
  TestChromeFrameHost* host_;
  CComPtr<IUnknown> host_keeper_;

  MockChromeFrame* chrome_frame_;
  CComPtr<IChromeFrame> chrome_frame_keeper_;

  // Quench logging for all tests.
  testing::LogDisabler no_dchecks_;
};

TEST_F(ChromeFrameHostTest, StartChromeFrameSuccess) {
  ExpectCreateChromeFrame(S_OK);

  ASSERT_HRESULT_SUCCEEDED(host_->StartChromeFrame());
}

TEST_F(ChromeFrameHostTest, StartChromeFrameFailsOnActiveXHostCreationFailure) {
  ExpectCreateChromeFrame(S_OK);

  host_->set_active_x_host_creation_error(E_OUTOFMEMORY);
  ASSERT_EQ(E_OUTOFMEMORY, host_->StartChromeFrame());
}

TEST_F(ChromeFrameHostTest, StartChromeFrameFailsOnAdviseFailure) {
  ExpectCreateChromeFrame(S_OK);

  chrome_frame_->set_no_events(true);
  ASSERT_HRESULT_FAILED(host_->StartChromeFrame());
}

TEST_F(ChromeFrameHostTest, StartChromeFrameFailsOnCreationFailure) {
  ExpectCreateChromeFrame(E_OUTOFMEMORY);

  ASSERT_HRESULT_FAILED(host_->StartChromeFrame());
}

TEST_F(ChromeFrameHostTest, ChromeFramePrivilegedInServiceProviderChain) {
  ExpectCreateChromeFrame(S_OK);

  ASSERT_HRESULT_SUCCEEDED(host_->StartChromeFrame());

  // Get the service provider on our mock Chrome frame.
  CComPtr<IServiceProvider> sp;
  ASSERT_HRESULT_SUCCEEDED(
      chrome_frame_->GetSite(IID_IServiceProvider,
                             reinterpret_cast<void**>(&sp)));

  CComPtr<IChromeFramePrivileged> priv;
  ASSERT_HRESULT_SUCCEEDED(sp->QueryService(SID_ChromeFramePrivileged,
                                            IID_IChromeFramePrivileged,
                                            reinterpret_cast<void**>(&priv)));
  ASSERT_TRUE(priv != NULL);

  boolean wants_priv = FALSE;
  ASSERT_HRESULT_SUCCEEDED(priv->GetWantsPrivileged(&wants_priv));
  ASSERT_TRUE(wants_priv);

  CComBSTR profile_name;
  ASSERT_HRESULT_SUCCEEDED(priv->GetChromeProfileName(&profile_name));
  ASSERT_TRUE(profile_name == NULL);

  static const wchar_t* kProfileName = L"iexplore";
  host_->SetChromeProfileName(kProfileName);
  ASSERT_HRESULT_SUCCEEDED(priv->GetChromeProfileName(&profile_name));
  ASSERT_STREQ(kProfileName, profile_name);
}

TEST_F(ChromeFrameHostTest, PostMessage) {
  ExpectCreateChromeFrame(S_OK);
  ASSERT_HRESULT_SUCCEEDED(host_->StartChromeFrame());

  // Make sure we properly queue before the document gets loaded.
  CComBSTR queue_it_1("queue_it_1");
  CComBSTR queue_it_2("queue_it_2");
  CComBSTR target_1("target_1");
  CComBSTR target_2("target_2");

  EXPECT_CALL(*chrome_frame_, postPrivateMessage(_, _, _)).Times(0);
  EXPECT_HRESULT_SUCCEEDED(host_->PostMessage(queue_it_1, target_1));
  EXPECT_HRESULT_SUCCEEDED(host_->PostMessage(queue_it_2, target_2));

  // Only the queued messages should be posted.
  EXPECT_CALL(*chrome_frame_, postPrivateMessage(StrEq(queue_it_1.m_str),
      _, StrEq(target_1.m_str))).WillOnce(Return(S_OK));
  EXPECT_CALL(*chrome_frame_, postPrivateMessage(StrEq(queue_it_2.m_str),
      _, StrEq(target_2.m_str))).WillOnce(Return(S_OK));

  MockDispatchEx* event;
  CComDispatchDriver event_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockDispatchEx>::CreateInitialized(&event,
                                                             &event_keeper));
  host_->CallOnCfLoad(event);

  // Nothing left to be posted once we have loaded.
  EXPECT_CALL(*chrome_frame_, postPrivateMessage(_, _, _)).Times(0);
  host_->CallOnCfLoad(event);

  // Messages should go directly to Chrome Frame now, whether they are
  // queueable or not.
  EXPECT_CALL(*chrome_frame_, postPrivateMessage(StrEq(queue_it_1.m_str),
      _, StrEq(target_1.m_str))).WillOnce(Return(S_OK));
  EXPECT_CALL(*chrome_frame_, postPrivateMessage(StrEq(queue_it_2.m_str),
      _, StrEq(target_2.m_str))).WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(host_->PostMessage(queue_it_1, target_1));
  EXPECT_HRESULT_SUCCEEDED(host_->PostMessage(queue_it_2, target_2));

  // Nothing left to be posted once we have loaded.
  EXPECT_CALL(*chrome_frame_, postPrivateMessage(_, _, _)).Times(0);
  host_->CallOnCfLoad(event);
}

TEST_F(ChromeFrameHostTest, OnCfReadyStateChanged) {
  ExpectCreateChromeFrame(S_OK);
  ASSERT_HRESULT_SUCCEEDED(host_->StartChromeFrame());

  // Should call event sink on all states, but only once
  // event sink is set.
  host_->CallOnCfReadyStateChanged(READYSTATE_UNINITIALIZED);
  host_->CallOnCfReadyStateChanged(READYSTATE_LOADING);
  host_->CallOnCfReadyStateChanged(READYSTATE_LOADED);
  host_->CallOnCfReadyStateChanged(READYSTATE_INTERACTIVE);
  host_->CallOnCfReadyStateChanged(READYSTATE_COMPLETE);

  MockChromeFrameHostEvents* event_sink;
  CComPtr<IUnknown> event_sink_sp;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockChromeFrameHostEvents>::CreateInitialized(
          &event_sink, &event_sink_sp));

  host_->SetEventSink(event_sink);
  EXPECT_CALL(*event_sink, OnCfReadyStateChanged(_)).Times(5);
  host_->CallOnCfReadyStateChanged(READYSTATE_UNINITIALIZED);
  host_->CallOnCfReadyStateChanged(READYSTATE_LOADING);
  host_->CallOnCfReadyStateChanged(READYSTATE_LOADED);
  host_->CallOnCfReadyStateChanged(READYSTATE_INTERACTIVE);
  host_->CallOnCfReadyStateChanged(READYSTATE_COMPLETE);
}

TEST_F(ChromeFrameHostTest, ChromeFrameEventsCaptured) {
  ExpectCreateChromeFrame(S_OK);
  ASSERT_HRESULT_SUCCEEDED(host_->StartChromeFrame());

  // Create a handy-dandy dispatch mock object.
  MockDispatchEx* event;
  CComDispatchDriver event_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockDispatchEx>::CreateInitialized(&event,
                                                             &event_keeper));

  EXPECT_CALL(*host_, OnCfLoad(event)).Times(1);
  chrome_frame_->FireCfLoad(event);

  EXPECT_CALL(*host_, OnCfLoadError(event)).Times(1);
  chrome_frame_->FireCfLoadError(event);

  EXPECT_CALL(*host_, OnCfMessage(event)).Times(1);
  chrome_frame_->FireCfMessage(event);

  EXPECT_CALL(*host_, OnCfReadyStateChanged(READYSTATE_LOADING)).Times(1);
  chrome_frame_->FireCfReadyStateChanged(READYSTATE_LOADING);

  EXPECT_CALL(*host_, OnCfReadyStateChanged(READYSTATE_COMPLETE)).Times(1);
  chrome_frame_->FireCfReadyStateChanged(READYSTATE_COMPLETE);

  static const wchar_t* kOrigin = L"From Russia with Love";
  EXPECT_CALL(*host_, OnCfPrivateMessage(event, StrEq(kOrigin))).Times(1);
  chrome_frame_->FireCfPrivateMessage(event, CComBSTR(kOrigin));

  static const wchar_t* kPath = L"they all lead to Rome";
  EXPECT_CALL(*host_, OnCfExtensionReady(StrEq(kPath), 42)).Times(1);
  chrome_frame_->FireCfExtensionReady(CComBSTR(kPath), 42);

  EXPECT_CALL(*host_, OnCfGetEnabledExtensionsComplete(_)).Times(1);
  chrome_frame_->FireCfGetEnabledExtensionsComplete(NULL);

  EXPECT_CALL(*host_, OnCfChannelError()).Times(1);
  chrome_frame_->FireCfChannelError();
}

// Used to compare two arrays of strings, as when GetIDsOfNames is called
MATCHER_P(SingleEntryLPOLESTRArraysEqual, single_entry, "") {
  return std::wstring(single_entry) == arg[0];
}

TEST_F(ChromeFrameHostTest, EventSync) {
  // Make sure all calls are safe without an event sink.
  BSTR functions_enabled = NULL;
  CComQIPtr<IChromeFramePrivileged> host_privileged(host_);
  EXPECT_HRESULT_SUCCEEDED(host_privileged->GetExtensionApisToAutomate(
      &functions_enabled));
  EXPECT_EQ(NULL, functions_enabled);

  host_->CallOnCfExtensionReady(L"", 0);
  host_->CallOnCfGetEnabledExtensionsComplete(NULL);
  host_->CallOnCfPrivateMessage(NULL, NULL);
  host_->CallOnCfPrivateMessage(NULL, NULL);
  host_->CallOnCfChannelError();

  // Now make sure the calls are properly propagated to the event sync.
  MockChromeFrameHostEvents* event_sink;
  CComPtr<IUnknown> event_sink_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockChromeFrameHostEvents>::CreateInitialized(
      &event_sink, &event_sink_keeper));
  // We cheat a bit here because there is no IID to QI for
  // IChromeFrameHostEvents.
  host_->SetEventSink(event_sink);

  static const BSTR kCheatCode = L"Cheat code";
  EXPECT_CALL(*event_sink, OnCfGetExtensionApisToAutomate(NotNull())).WillOnce(
      DoAll(SetArgumentPointee<0>(kCheatCode), Return(S_OK)));
  EXPECT_HRESULT_SUCCEEDED(host_privileged->GetExtensionApisToAutomate(
      &functions_enabled));
  DCHECK_EQ(kCheatCode, functions_enabled);

  // Other calls should be OK, as long as the string isn't set to a non-NULL
  // value.
  EXPECT_CALL(*event_sink, OnCfGetExtensionApisToAutomate(NotNull())).WillOnce(
      DoAll(SetArgumentPointee<0>(static_cast<LPOLESTR>(0)), Return(S_FALSE)));
  EXPECT_HRESULT_SUCCEEDED(host_privileged->GetExtensionApisToAutomate(
      &functions_enabled));

  EXPECT_CALL(*event_sink, OnCfExtensionReady(StrEq(L""), 0)).Times(1);
  host_->CallOnCfExtensionReady(L"", 0);

  EXPECT_CALL(*event_sink, OnCfGetEnabledExtensionsComplete(NULL)).Times(1);
  host_->CallOnCfGetEnabledExtensionsComplete(NULL);

  EXPECT_CALL(*event_sink, OnCfChannelError()).Times(1);
  host_->CallOnCfChannelError();

  MockDispatchEx* mock_dispatch = NULL;
  CComDispatchDriver dispatch_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockDispatchEx>::CreateInitialized(&mock_dispatch,
                                                             &dispatch_keeper));
  static LPCOLESTR kOrigin = L"Origin";
  VARIANT origin;
  origin.vt = VT_BSTR;
  // This will be freed by the calling API. If we free it here, we're going to
  // end up with a double-free.
  origin.bstrVal = SysAllocString(kOrigin);
  DISPID origin_dispid = 42;
  EXPECT_CALL(*mock_dispatch, GetIDsOfNames(_,
      SingleEntryLPOLESTRArraysEqual(L"origin"), 1, _, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<4>(origin_dispid), Return(S_OK)));
  EXPECT_CALL(*mock_dispatch, Invoke(origin_dispid, _, _, DISPATCH_PROPERTYGET,
      Field(&DISPPARAMS::cArgs, Eq(0)), _, _, _)).WillOnce(DoAll(
      SetArgumentPointee<5>(origin), Return(S_OK)));

  static LPCOLESTR kData = L"Data";
  VARIANT data;
  data.vt = VT_BSTR;
  // This will be freed by the calling API. If we free it here, we're going to
  // end up with a double-free.
  data.bstrVal = SysAllocString(kData);
  DISPID data_dispid = 24;
  EXPECT_CALL(*mock_dispatch, GetIDsOfNames(_,
      SingleEntryLPOLESTRArraysEqual(L"data"), 1, _, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<4>(data_dispid), Return(S_OK)));
  EXPECT_CALL(*mock_dispatch, Invoke(data_dispid, _, _, DISPATCH_PROPERTYGET,
      Field(&DISPPARAMS::cArgs, Eq(0)), _, _, _)).WillOnce(DoAll(
      SetArgumentPointee<5>(data), Return(S_OK)));

  static LPOLESTR kTarget = L"Target";
  EXPECT_CALL(*event_sink, OnCfPrivateMessage(
      StrEq(kData), StrEq(kOrigin), StrEq(kTarget))).Times(1);

  host_->CallOnCfPrivateMessage(dispatch_keeper, kTarget);

  // Clean the VARIANTs.
  ZeroMemory(&origin, sizeof(origin));
  ZeroMemory(&data, sizeof(data));
}

}  // namespace
