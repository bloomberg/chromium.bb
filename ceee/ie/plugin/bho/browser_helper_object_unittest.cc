// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE browser helper object implementation.
#include "ceee/ie/plugin/bho/browser_helper_object.h"

#include <exdisp.h>
#include <shlguid.h>

#include "ceee/common/initializing_coclass.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/ie/testing/mock_browser_and_friends.h"
#include "ceee/ie/testing/mock_chrome_frame_host.h"
#include "ceee/testing/utils/dispex_mocks.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "broker_lib.h"  // NOLINT

namespace {

using testing::_;
using testing::CopyBSTRToArgument;
using testing::CopyInterfaceToArgument;
using testing::DoAll;
using testing::Exactly;
using testing::GetConnectionCount;
using testing::InstanceCountMixin;
using testing::MockBrokerRpcClient;
using testing::MockChromeFrameHost;
using testing::MockDispatchEx;
using testing::MockIOleWindow;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;
using testing::TestBrowser;
using testing::TestBrowserSite;

// Tab Ids passed to the API
const int kGoodTabId = 1;
const CeeeWindowHandle kGoodTabHandle = kGoodTabId + 1;
const HWND kGoodTab = (HWND)kGoodTabHandle;

class TestFrameEventHandler
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<StrictMock<TestFrameEventHandler> >,
      public InstanceCountMixin<TestFrameEventHandler>,
      public IFrameEventHandler {
 public:
  BEGIN_COM_MAP(TestFrameEventHandler)
    COM_INTERFACE_ENTRY_IID(IID_IFrameEventHandler, IFrameEventHandler)
  END_COM_MAP()

  HRESULT Initialize(TestFrameEventHandler** self) {
    *self = this;
    return S_OK;
  }

  MOCK_METHOD1(GetUrl, void(BSTR* url));
  MOCK_METHOD1(SetUrl, HRESULT(BSTR url));
  // no need to mock it yet and it is called from a DCHECK so...
  virtual READYSTATE GetReadyState() { return READYSTATE_COMPLETE; }
  MOCK_METHOD1(AddSubHandler, HRESULT(IFrameEventHandler* handler));
  MOCK_METHOD1(RemoveSubHandler, HRESULT(IFrameEventHandler* handler));
  MOCK_METHOD0(TearDown, void());
  MOCK_METHOD3(InsertCode, HRESULT(BSTR code, BSTR file,
                                   CeeeTabCodeType type));
  MOCK_METHOD0(RedoDoneInjections, void());
};

class TestingBrowserHelperObject
    : public BrowserHelperObject,
      public InstanceCountMixin<TestingBrowserHelperObject>,
      public InitializingCoClass<TestingBrowserHelperObject> {
 public:
  HRESULT Initialize(TestingBrowserHelperObject** self) {
    // Make sure this is done early so we can mock it.
    EXPECT_HRESULT_SUCCEEDED(MockChromeFrameHost::CreateInitializedIID(
        &mock_chrome_frame_host_, IID_IChromeFrameHost,
        &mock_chrome_frame_host_keeper_));
    chrome_frame_host_ = mock_chrome_frame_host_;
    *self = this;
    return S_OK;
  }

  virtual TabEventsFunnel& tab_events_funnel() {
    return mock_tab_events_funnel_;
  }

  virtual HRESULT GetBrokerRegistrar(ICeeeBrokerRegistrar** broker) {
    broker_keeper_.CopyTo(broker);
    return S_OK;
  }

  virtual HRESULT CreateExecutor(IUnknown** executor) {
    executor_keeper_.CopyTo(executor);
    return S_OK;
  }

  virtual HRESULT CreateChromeFrameHost() {
    EXPECT_TRUE(chrome_frame_host_ != NULL);
    return S_OK;
  }

  virtual HRESULT ConnectRpcBrokerClient() {
    MockBrokerRpcClient* rpc_client = new MockBrokerRpcClient();
    EXPECT_CALL(*rpc_client, is_connected()).WillOnce(Return(true));
    EXPECT_CALL(*rpc_client, SendUmaHistogramTimes(_, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*rpc_client, Disconnect()).Times(1);
    broker_rpc_.reset(rpc_client);
    return S_OK;
  }

  virtual HRESULT GetTabWindow(IServiceProvider* service_provider) {
    tab_window_ = reinterpret_cast<HWND>(kGoodTab);
    return S_OK;
  }

  virtual void SetTabId(int tab_id) {
    tab_id_ = tab_id;
  }

  virtual WebProgressNotifier* CreateWebProgressNotifier() {
    // Without calling Initialize(), the class won't do anything.
    return new WebProgressNotifier();
  }

  MOCK_METHOD0(SetupNewTabInfo, bool());
  MOCK_METHOD3(CreateFrameEventHandler, HRESULT(IWebBrowser2* browser,
                                                IWebBrowser2* parent_browser,
                                                IFrameEventHandler** handler));
  MOCK_METHOD1(BrowserContainsChromeFrame, bool(IWebBrowser2* browser));

  MOCK_METHOD2(IsHashChange, bool(BSTR, BSTR));
  bool CallIsHashChange(BSTR url1, BSTR url2) {
    return BrowserHelperObject::IsHashChange(url1, url2);
  }

  MOCK_METHOD2(GetParentBrowser, HRESULT(IWebBrowser2*, IWebBrowser2**));

  // Neuter the proxy registration/unregistration.
  HRESULT RegisterProxies() {
    return S_OK;
  }
  void UnregisterProxies() {
  }

  // Pulicize
  using BrowserHelperObject::HandleNavigateComplete;

  StrictMock<testing::MockTabEventsFunnel> mock_tab_events_funnel_;
  MockChromeFrameHost* mock_chrome_frame_host_;
  CComPtr<IChromeFrameHost> mock_chrome_frame_host_keeper_;

  testing::MockExecutorIUnknown* executor_;
  CComPtr<IUnknown> executor_keeper_;

  testing::MockBroker* broker_;
  CComPtr<ICeeeBrokerRegistrar> broker_keeper_;
};

class BrowserHelperObjectTest: public testing::Test {
 public:
  BrowserHelperObjectTest() : bho_(NULL), site_(NULL), browser_(NULL) {
  }
  ~BrowserHelperObjectTest() {
  }

  virtual void SetUp() {
    // Create the instance to test.
    ASSERT_HRESULT_SUCCEEDED(
        TestingBrowserHelperObject::CreateInitialized(&bho_, &bho_with_site_));
    bho_with_site_ = bho_;

    // TODO(mad@chromium.org): Test this method.
    EXPECT_CALL(*bho_, SetupNewTabInfo()).WillRepeatedly(Return(true));

    // We always go beyond Chrome Frame start and event funnel init.
    // Create the broker registrar related objects
    ASSERT_HRESULT_SUCCEEDED(testing::MockExecutorIUnknown::CreateInitialized(
        &bho_->executor_, &bho_->executor_keeper_));
    ASSERT_HRESULT_SUCCEEDED(testing::MockBroker::CreateInitialized(
        &bho_->broker_, &bho_->broker_keeper_));

    // We always go beyond Chrome Frame start, broker reg and event funnel init.
    // TODO(mad@chromium.org): Also cover failure cases from those.
    ExpectBrokerRegistration();
    ExpectChromeFrameStart();

    // Assert on successful TearDown.
    ExpectBrokerUnregistration();
    ExpectChromeFrameTearDown();
  }

  virtual void TearDown() {
    bho_->executor_ = NULL;
    bho_->executor_keeper_.Release();

    bho_->broker_ = NULL;
    bho_->broker_keeper_.Release();

    bho_ = NULL;
    bho_with_site_.Release();

    site_ = NULL;
    site_keeper_.Release();

    browser_ = NULL;
    browser_keeper_.Release();

    handler_ = NULL;
    handler_keeper_.Release();

    // Everything should have been relinquished.
    ASSERT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
  }

  void CreateSite() {
    ASSERT_HRESULT_SUCCEEDED(
        TestBrowserSite::CreateInitialized(&site_, &site_keeper_));
  }

  void CreateBrowser() {
    ASSERT_HRESULT_SUCCEEDED(
        TestBrowser::CreateInitialized(&browser_, &browser_keeper_));

    // Fail get_Parent calls for the root.
    EXPECT_CALL(*bho_, GetParentBrowser(browser_keeper_.p, NotNull())).
        WillRepeatedly(Return(E_NOTIMPL));

    if (site_)
      site_->browser_ = browser_keeper_;
  }

  void CreateHandler() {
    ASSERT_HRESULT_SUCCEEDED(
        TestFrameEventHandler::CreateInitializedIID(
            &handler_, IID_IFrameEventHandler, &handler_keeper_));
  }

  bool BhoHasSite() {
    // Check whether BHO has a site set.
    CComPtr<IUnknown> site;
    if (SUCCEEDED(bho_with_site_->GetSite(
        IID_IUnknown, reinterpret_cast<void**>(&site))))
      return true;
    if (site != NULL)
      return true;

    return false;
  }

  void ExpectChromeFrameStart() {
    EXPECT_CALL(*(bho_->mock_chrome_frame_host_), SetEventSink(_)).
        Times(1);
    EXPECT_CALL(*(bho_->mock_chrome_frame_host_), SetChromeProfileName(_)).
        Times(1);
    EXPECT_CALL(*(bho_->mock_chrome_frame_host_), StartChromeFrame()).
        WillOnce(Return(S_OK));
  }

  CComBSTR CreateTabInfo(int tab_id) {
    std::ostringstream iss;
    iss << L"{\"id\":" << tab_id << L"}";
    return CComBSTR(iss.str().c_str());
  }
  void ExpectChromeFrameGetSessionId() {
    EXPECT_CALL(*(bho_->mock_chrome_frame_host_), GetSessionId(NotNull())).
        WillOnce(DoAll(SetArgumentPointee<0>(kGoodTabId), Return(S_OK)));
    EXPECT_CALL(*(bho_->broker_), SetTabIdForHandle(kGoodTabId,
        kGoodTabHandle)).WillOnce(Return(S_OK));
  }

  void ExpectChromeFrameTearDown() {
    EXPECT_CALL(*(bho_->mock_chrome_frame_host_), SetEventSink(NULL)).
        Times(1);
    EXPECT_CALL(*(bho_->mock_chrome_frame_host_), TearDown()).
        WillOnce(Return(S_OK));
  }

  void ExpectBrokerRegistration() {
    EXPECT_CALL(*bho_->broker_, RegisterTabExecutor(::GetCurrentThreadId(),
        bho_->executor_keeper_.p)).WillOnce(Return(S_OK));
  }

  void ExpectBrokerUnregistration() {
    EXPECT_CALL(*bho_->broker_, UnregisterExecutor(::GetCurrentThreadId())).
        WillOnce(Return(S_OK));
  }

  void ExpectHandleNavigation(TestFrameEventHandler* handler,
                              bool hash_change) {
    EXPECT_CALL(*handler, GetUrl(_)).Times(1).
        WillOnce(CopyBSTRToArgument<0>(kUrl2));
    EXPECT_CALL(*bho_, IsHashChange(StrEq(kUrl1), StrEq(kUrl2))).
        WillOnce(Return(hash_change));
    // We should get the URL poked at the handler.
    EXPECT_CALL(*handler, SetUrl(StrEq(kUrl1))).WillOnce(Return(S_OK));
  }

  void ExpectTopBrowserNavigation(bool hash_change, bool first_call) {
    // We also get a tab update notification.
    if (first_call) {
      EXPECT_CALL(bho_->mock_tab_events_funnel_,
                  OnCreated(_, StrEq(kUrl1), false)).Times(1);
    }
    EXPECT_CALL(bho_->mock_tab_events_funnel_,
                OnUpdated(_, StrEq(kUrl1), READYSTATE_UNINITIALIZED)).Times(1);
    if (hash_change) {
      EXPECT_CALL(bho_->mock_tab_events_funnel_,
                  OnUpdated(_, StrEq(kUrl1), READYSTATE_COMPLETE)).Times(1);
    }
  }

  void ExpectFireOnRemovedEvent() {
    EXPECT_CALL(bho_->mock_tab_events_funnel_, OnRemoved(_));
  }

  void ExpectFireOnUnmappedEvent() {
    EXPECT_CALL(bho_->mock_tab_events_funnel_, OnTabUnmapped(_, _));
  }

  static const wchar_t* kUrl1;
  static const wchar_t* kUrl2;

  // Logging quenched for all tests.
  testing::LogDisabler no_dchecks_;

  TestingBrowserHelperObject* bho_;
  CComPtr<IObjectWithSite> bho_with_site_;

  testing::TestBrowserSite* site_;
  CComPtr<IUnknown> site_keeper_;

  TestBrowser* browser_;
  CComPtr<IWebBrowser2> browser_keeper_;

  TestFrameEventHandler* handler_;
  CComPtr<IFrameEventHandler> handler_keeper_;
};

const wchar_t* BrowserHelperObjectTest::kUrl1 =
L"http://www.google.com/search?q=Google+Buys+Iceland";
const wchar_t* BrowserHelperObjectTest::kUrl2 = L"http://www.google.com";


// Setting the BHO site with a non-service provider fails.
TEST_F(BrowserHelperObjectTest, SetSiteWithNoServiceProviderFails) {
  // Create an object that doesn't implement IServiceProvider.
  MockDispatchEx* site = NULL;
  CComPtr<IUnknown> site_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockDispatchEx>::CreateInitialized(&site,
                                                             &site_keeper));
  // Setting a site that doesn't implement IServiceProvider fails.
  ASSERT_HRESULT_FAILED(bho_with_site_->SetSite(site_keeper));
  ASSERT_FALSE(BhoHasSite());
}

// Setting the BHO site with no browser fails.
TEST_F(BrowserHelperObjectTest, SetSiteWithNullBrowserFails) {
  CreateSite();

  // Setting a site with no browser fails.
  ASSERT_HRESULT_FAILED(bho_with_site_->SetSite(site_keeper_));
  ASSERT_FALSE(BhoHasSite());
}

// Setting the BHO site with a non-browser fails.
TEST_F(BrowserHelperObjectTest, SetSiteWithNonBrowserFails) {
  CreateSite();

  // Endow the site with a non-browser service.
  MockDispatchEx* mock_non_browser = NULL;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockDispatchEx>::CreateInitialized(&mock_non_browser,
                                                             &site_->browser_));
  // Setting a site with a non-browser fails.
  ASSERT_HRESULT_FAILED(bho_with_site_->SetSite(site_keeper_));
  ASSERT_FALSE(BhoHasSite());
}

// Setting the BHO site with a browser that doesn't implement the
// DIID_DWebBrowserEvents2 connection point fails.
TEST_F(BrowserHelperObjectTest, SetSiteWithNoEventsFails) {
  CreateSite();
  CreateBrowser();

  // Disable the connection point.
  browser_->no_events_ = true;

  // No connection point site fails.
  ASSERT_HRESULT_FAILED(bho_with_site_->SetSite(site_keeper_));
  ASSERT_FALSE(BhoHasSite());
}

TEST_F(BrowserHelperObjectTest, SetSiteWithBrowserSucceeds) {
  CreateSite();
  CreateBrowser();

  size_t num_connections = 0;
  ASSERT_HRESULT_SUCCEEDED(GetConnectionCount(browser_keeper_,
                                              DIID_DWebBrowserEvents2,
                                              &num_connections));
  ASSERT_EQ(0, num_connections);

  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));
  // Check that the we set up a connection.
  ASSERT_HRESULT_SUCCEEDED(GetConnectionCount(browser_keeper_,
                                              DIID_DWebBrowserEvents2,
                                              &num_connections));
  ASSERT_EQ(1, num_connections);

  // Check the site's retained.
  CComPtr<IUnknown> set_site;
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->GetSite(
      IID_IUnknown, reinterpret_cast<void**>(&set_site)));
  ASSERT_TRUE(set_site == site_keeper_);

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));

  // And check that the connection was severed.
  ASSERT_HRESULT_SUCCEEDED(GetConnectionCount(browser_keeper_,
                                              DIID_DWebBrowserEvents2,
                                              &num_connections));
  ASSERT_EQ(0, num_connections);
}

TEST_F(BrowserHelperObjectTest, OnNavigateCompleteHandled) {
  CreateSite();
  CreateBrowser();
  CreateHandler();
  ExpectChromeFrameGetSessionId();

  // The site needs to return the top-level browser.
  site_->browser_ = browser_;

  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));

  EXPECT_CALL(*bho_, CreateFrameEventHandler(browser_, NULL, NotNull())).
      WillOnce(DoAll(CopyInterfaceToArgument<2>(handler_keeper_),
                     Return(S_OK)));
  EXPECT_CALL(*bho_, BrowserContainsChromeFrame(browser_)).
      WillOnce(Return(false));
  ExpectHandleNavigation(handler_, true);
  ExpectTopBrowserNavigation(true, true);
  browser_->FireOnNavigateComplete(browser_, &CComVariant(kUrl1));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

TEST_F(BrowserHelperObjectTest, RenavigationNotifiesUrl) {
  CreateSite();
  CreateBrowser();
  CreateHandler();
  ExpectChromeFrameGetSessionId();

  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));

  // Make as if a handler has been attached to the browser.
  ASSERT_HRESULT_SUCCEEDED(
      bho_->AttachBrowser(browser_, NULL, handler_keeper_));

  EXPECT_CALL(*handler_, GetUrl(_)).Times(1).
      WillOnce(CopyBSTRToArgument<0>(kUrl2));
  EXPECT_CALL(*bho_, IsHashChange(StrEq(kUrl1), StrEq(kUrl2))).
      WillOnce(Return(false));
  // We should get the "new" URL poked at the handler.
  EXPECT_CALL(*handler_, SetUrl(StrEq(kUrl1))).Times(1);

  // We also get a tab update notification.
  EXPECT_CALL(bho_->mock_tab_events_funnel_,
              OnCreated(_, StrEq(kUrl1), false)).Times(1);
  EXPECT_CALL(bho_->mock_tab_events_funnel_,
              OnUpdated(_, StrEq(kUrl1), READYSTATE_UNINITIALIZED)).Times(1);
  browser_->FireOnNavigateComplete(browser_, &CComVariant(kUrl1));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

// Test that we filter OnNavigateComplete invocations with
// non-IWebBrowser2 or non BSTR arguments.
TEST_F(BrowserHelperObjectTest, OnNavigateCompleteUnhandled) {
  CreateSite();
  CreateBrowser();

  // Create an object that doesn't implement IWebBrowser2.
  MockDispatchEx* non_browser = NULL;
  CComPtr<IDispatch> non_browser_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      InitializingCoClass<MockDispatchEx>::CreateInitialized(
          &non_browser, &non_browser_keeper));
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));

  // HandleNavigateComplete should not be called by the invocations below.
  EXPECT_CALL(*bho_, CreateFrameEventHandler(_, _, _)).Times(0);

  // Non-browser target.
  browser_->FireOnNavigateComplete(non_browser, &CComVariant(kUrl1));

  // Non-BSTR url parameter.
  browser_->FireOnNavigateComplete(browser_, &CComVariant(non_browser));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

TEST_F(BrowserHelperObjectTest, HandleNavigateComplete) {
  CreateSite();
  CreateBrowser();
  CreateHandler();

  // The site needs to return the top-level browser.
  site_->browser_ = browser_;
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));

  EXPECT_CALL(*bho_, CreateFrameEventHandler(browser_, NULL, NotNull())).
      WillOnce(DoAll(CopyInterfaceToArgument<2>(handler_keeper_),
               Return(S_OK)));
  EXPECT_CALL(*bho_, BrowserContainsChromeFrame(browser_)).
      WillOnce(Return(false));
  ExpectHandleNavigation(handler_, false);
  ExpectTopBrowserNavigation(false, true);
  bho_->HandleNavigateComplete(browser_, CComBSTR(kUrl1));

  // Now handle the case without the creation of a handler.
  EXPECT_HRESULT_SUCCEEDED(bho_->AttachBrowser(browser_, NULL, handler_));
  ExpectHandleNavigation(handler_, false);
  ExpectTopBrowserNavigation(false, false);
  bho_->HandleNavigateComplete(browser_, CComBSTR(kUrl1));

  // Now navigate a sub-frame.
  TestBrowser* browser2;
  CComPtr<IWebBrowser2> browser2_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      TestBrowser::CreateInitialized(&browser2, &browser2_keeper));
  EXPECT_CALL(*bho_, GetParentBrowser(browser2, NotNull())).
      WillRepeatedly(DoAll(CopyInterfaceToArgument<1>(browser_keeper_),
                           Return(S_OK)));
  TestFrameEventHandler* handler2;
  CComPtr<IFrameEventHandler> handler2_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      TestFrameEventHandler::CreateInitializedIID(
          &handler2, IID_IFrameEventHandler, &handler2_keeper));

  EXPECT_CALL(*bho_,
      CreateFrameEventHandler(browser2, browser_, NotNull())).
          WillOnce(DoAll(CopyInterfaceToArgument<2>(handler2_keeper),
                   Return(S_OK)));

  ExpectHandleNavigation(handler2, false);
  bho_->HandleNavigateComplete(browser2, CComBSTR(kUrl1));
  EXPECT_CALL(*handler_, AddSubHandler(handler2)).
      WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(bho_->AttachBrowser(browser2, browser_, handler2));

  // Now, navigating the top browser again.
  ExpectHandleNavigation(handler_, false);
  ExpectTopBrowserNavigation(false, false);
  bho_->HandleNavigateComplete(browser_, CComBSTR(kUrl1));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

TEST_F(BrowserHelperObjectTest, OnNavigationCompletedWithUnrelatedBrowser) {
  CreateSite();
  CreateBrowser();
  CreateHandler();

  // The site needs to return the top-level browser.
  site_->browser_ = browser_;
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));

  // Now navigate to an invalid frame (no parent).
  TestBrowser* browser2;
  CComPtr<IWebBrowser2> browser2_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      TestBrowser::CreateInitialized(&browser2, &browser2_keeper));

  EXPECT_CALL(*bho_, GetParentBrowser(browser2, NotNull())).
      WillOnce(Return(E_FAIL));

  EXPECT_CALL(*bho_,
      CreateFrameEventHandler(browser2, browser_, NotNull())).
          Times(Exactly(0));

  bho_->HandleNavigateComplete(browser2, CComBSTR(kUrl1));
  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

TEST_F(BrowserHelperObjectTest, AttachOrphanedBrowser) {
  CreateSite();
  CreateBrowser();
  CreateHandler();

  // The site needs to return the top-level browser.
  site_->browser_ = browser_;
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));

  // Attach the root.
  EXPECT_HRESULT_SUCCEEDED(bho_->AttachBrowser(browser_, NULL, handler_));

  // Now attach an apparent orphan which is actually the grand child of an
  // existing frame which parent wasn't seen yet.
  TestBrowser* browser3;
  CComPtr<IWebBrowser2> browser3_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      TestBrowser::CreateInitialized(&browser3, &browser3_keeper));

  TestFrameEventHandler* handler3;
  CComPtr<IFrameEventHandler> handler_keeper_3;
  ASSERT_HRESULT_SUCCEEDED(
      TestFrameEventHandler::CreateInitializedIID(
          &handler3, IID_IFrameEventHandler, &handler_keeper_3));

  TestBrowser* browser3_parent;
  CComPtr<IWebBrowser2> browser3_parent_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      TestBrowser::CreateInitialized(&browser3_parent,
                                     &browser3_parent_keeper));

  TestFrameEventHandler* handler3_parent;
  CComPtr<IFrameEventHandler> handler3_parent_keeper;
  ASSERT_HRESULT_SUCCEEDED(
      TestFrameEventHandler::CreateInitializedIID(
          &handler3_parent, IID_IFrameEventHandler, &handler3_parent_keeper));

  EXPECT_CALL(*bho_, GetParentBrowser(browser3_parent, NotNull())).
      WillOnce(DoAll(CopyInterfaceToArgument<1>(browser_keeper_.p),
                     Return(S_OK)));
  EXPECT_CALL(*browser3_parent, get_LocationURL(NotNull())).
      WillOnce(DoAll(CopyBSTRToArgument<0>(kUrl1), Return(S_OK)));
  EXPECT_CALL(*bho_,
      CreateFrameEventHandler(browser3_parent, browser_keeper_.p, NotNull())).
          WillOnce(DoAll(CopyInterfaceToArgument<2>(handler3_parent_keeper),
                         Return(S_OK)));
  EXPECT_CALL(*handler3_parent, SetUrl(StrEq(kUrl1))).WillOnce(Return(S_OK));
  EXPECT_CALL(*handler3_parent, AddSubHandler(handler3)).WillOnce(Return(S_OK));
  EXPECT_HRESULT_SUCCEEDED(bho_->AttachBrowser(browser3, browser3_parent,
                                               handler3));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

TEST_F(BrowserHelperObjectTest, IFrameEventHandlerHost) {
  CreateSite();
  CreateBrowser();
  CreateHandler();

  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));

  // Detaching a non-attached browser should fail.
  EXPECT_HRESULT_FAILED(bho_->DetachBrowser(browser_, NULL, handler_));

  // First-time attach should succeed.
  EXPECT_HRESULT_SUCCEEDED(bho_->AttachBrowser(browser_, NULL, handler_));
  // Second attach should fail.
  EXPECT_HRESULT_FAILED(bho_->AttachBrowser(browser_, NULL, handler_));

  // Subsequent detach should succeed.
  EXPECT_HRESULT_SUCCEEDED(bho_->DetachBrowser(browser_, NULL, handler_));
  // But not twice.
  EXPECT_HRESULT_FAILED(bho_->DetachBrowser(browser_, NULL, handler_));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));

  // TODO(siggi@chromium.org): test hierarchial attach/detach/TearDown.
}

TEST_F(BrowserHelperObjectTest, InsertCode) {
  CreateSite();
  CreateBrowser();
  CreateHandler();
  ExpectChromeFrameGetSessionId();

  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));
  ASSERT_TRUE(BhoHasSite());
  ASSERT_HRESULT_SUCCEEDED(bho_->AttachBrowser(browser_, NULL, handler_));

  CComBSTR code;
  CComBSTR file;
  EXPECT_CALL(*handler_, InsertCode(_, _, kCeeeTabCodeTypeCss))
      .WillOnce(Return(S_OK));
  ASSERT_HRESULT_SUCCEEDED(bho_->InsertCode(code, file, FALSE,
                                            kCeeeTabCodeTypeCss));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

TEST_F(BrowserHelperObjectTest, InsertCodeAllFrames) {
  CreateSite();
  CreateBrowser();
  CreateHandler();
  ExpectChromeFrameGetSessionId();

  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));
  ASSERT_TRUE(BhoHasSite());
  ASSERT_HRESULT_SUCCEEDED(bho_->AttachBrowser(browser_, NULL, handler_));

  // Add a second browser to the BHO and make sure that both get called.
  TestBrowser* browser2;
  CComPtr<IWebBrowser2> browser_keeper_2;
  ASSERT_HRESULT_SUCCEEDED(
      TestBrowser::CreateInitialized(&browser2, &browser_keeper_2));

  TestFrameEventHandler* handler2;
  CComPtr<IFrameEventHandler> handler_keeper_2;
  ASSERT_HRESULT_SUCCEEDED(
      TestFrameEventHandler::CreateInitializedIID(
          &handler2, IID_IFrameEventHandler, &handler_keeper_2));

  ASSERT_HRESULT_SUCCEEDED(bho_->AttachBrowser(browser_keeper_2,
                                               NULL,
                                               handler_keeper_2));

  CComBSTR code;
  CComBSTR file;
  EXPECT_CALL(*handler_, InsertCode(_, _, kCeeeTabCodeTypeJs))
      .WillOnce(Return(S_OK));
  EXPECT_CALL(*handler2, InsertCode(_, _, kCeeeTabCodeTypeJs))
      .WillOnce(Return(S_OK));
  ASSERT_HRESULT_SUCCEEDED(bho_->InsertCode(code, file, TRUE,
                                            kCeeeTabCodeTypeJs));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

TEST_F(BrowserHelperObjectTest, IsHashChange) {
  CreateSite();
  CreateBrowser();
  CreateHandler();

  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));

  CComBSTR url1("http://www.google.com/");
  CComBSTR url2("http://www.google.com/#");
  CComBSTR url3("http://www.google.com/#test");
  CComBSTR url4("http://www.google.com/#bingo");
  CComBSTR url5("http://www.bingo.com/");
  CComBSTR url6("http://www.twitter.com/#test");
  CComBSTR empty;

  // Passing cases.
  EXPECT_TRUE(bho_->CallIsHashChange(url1, url2));
  EXPECT_TRUE(bho_->CallIsHashChange(url1, url3));
  EXPECT_TRUE(bho_->CallIsHashChange(url2, url3));
  EXPECT_TRUE(bho_->CallIsHashChange(url3, url4));

  // Failing cases.
  EXPECT_FALSE(bho_->CallIsHashChange(url1, empty));
  EXPECT_FALSE(bho_->CallIsHashChange(empty, url1));
  EXPECT_FALSE(bho_->CallIsHashChange(url1, url1));
  EXPECT_FALSE(bho_->CallIsHashChange(url1, url5));
  EXPECT_FALSE(bho_->CallIsHashChange(url1, url6));
  EXPECT_FALSE(bho_->CallIsHashChange(url3, url6));
  EXPECT_FALSE(bho_->CallIsHashChange(url5, url6));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

TEST_F(BrowserHelperObjectTest, SetToolBandSessionId) {
  CreateSite();
  CreateBrowser();

  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(site_keeper_));
  EXPECT_CALL(*(bho_->broker_), SetTabToolBandIdForHandle(kGoodTabId,
        kGoodTabHandle)).WillOnce(Return(S_OK));
  EXPECT_EQ(S_OK, bho_->SetToolBandSessionId(kGoodTabId));

  EXPECT_CALL(*(bho_->broker_), SetTabToolBandIdForHandle(kGoodTabId,
        kGoodTabHandle)).WillOnce(Return(E_FAIL));
  EXPECT_EQ(E_FAIL, bho_->SetToolBandSessionId(kGoodTabId));

  ExpectFireOnRemovedEvent();
  ExpectFireOnUnmappedEvent();
  ASSERT_HRESULT_SUCCEEDED(bho_with_site_->SetSite(NULL));
}

}  // namespace
