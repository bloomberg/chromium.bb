// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A test that hosts and excercises the webbrowser control to test
// its event firing behavior.
#include <guiddef.h>
#include <mshtml.h>
#include <shlguid.h>
#include "base/utf_string_conversions.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/bho/browser_helper_object.h"
#include "ceee/ie/testing/mediumtest_ie_common.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/ie/testing/mock_chrome_frame_host.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "net/base/net_util.h"

namespace {

using testing::_;
using testing::AnyNumber;
using testing::BrowserEventSinkBase;
using testing::GetTestUrl;
using testing::DoAll;
using testing::InstanceCountMixin;
using testing::kAnotherFrameOne;
using testing::kAnotherFrameTwo;
using testing::kAnotherTwoFramesPage;
using testing::kDeepFramesPage;
using testing::kFrameOne;
using testing::kFrameTwo;
using testing::kLevelOneFrame;
using testing::kLevelTwoFrame;
using testing::kOrphansPage;
using testing::kSimplePage;
using testing::kTwoFramesPage;
using testing::MockBrokerRpcClient;
using testing::MockChromeFrameHost;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::ShellBrowserTestImpl;
using testing::StrictMock;

ScriptHost::DebugApplication debug_app(L"FrameEventHandlerUnittest");

class TestingFrameEventHandler
    : public FrameEventHandler,
      public InstanceCountMixin<TestingFrameEventHandler>,
      public InitializingCoClass<TestingFrameEventHandler> {
 public:
  // Disambiguate.
  using InitializingCoClass<TestingFrameEventHandler>::CreateInitializedIID;

  IWebBrowser2* browser() const { return browser_; }
  IHTMLDocument2* document() const { return document_; }
};

class TestingBrowserHelperObject
    : public BrowserHelperObject,
      public InitializingCoClass<TestingBrowserHelperObject>,
      public InstanceCountMixin<TestingBrowserHelperObject> {
 public:
  TestingBrowserHelperObject() : mock_chrome_frame_host_(NULL) {
  }

  HRESULT Initialize(TestingBrowserHelperObject** self) {
    *self = this;
    return S_OK;
  }

  HRESULT CreateFrameEventHandler(IWebBrowser2* browser,
                                  IWebBrowser2* parent_browser,
                                  IFrameEventHandler** handler) {
    return TestingFrameEventHandler::CreateInitializedIID(
        browser, parent_browser, this, IID_IFrameEventHandler, handler);
  }

  // Overrides original implementation to suppress calls to broker.
  WebProgressNotifier* CreateWebProgressNotifier() {
    scoped_ptr<WebProgressNotifier> web_progress_notifier(
        new TestingWebProgressNotifier());
    HRESULT hr = web_progress_notifier->Initialize(this, tab_window_,
                                                   web_browser_);
    return SUCCEEDED(hr) ? web_progress_notifier.release() : NULL;
  }

  virtual TabEventsFunnel& tab_events_funnel() {
    return mock_tab_events_funnel_;
  }

  virtual HRESULT GetBrokerRegistrar(ICeeeBrokerRegistrar** broker) {
    broker_keeper_.CopyTo(broker);
    return S_OK;
  }

  virtual HRESULT CreateExecutor(IUnknown** executor) {
    CComPtr<IUnknown> unknown(executor_keeper_);
    unknown.CopyTo(executor);
    return S_OK;
  }

  virtual HRESULT CreateChromeFrameHost() {
    HRESULT hr = MockChromeFrameHost::CreateInitializedIID(
        &mock_chrome_frame_host_, IID_IChromeFrameHost,
        chrome_frame_host_.Receive());

    // Neuter the functions we know are going to be called.
    if (SUCCEEDED(hr)) {
      EXPECT_CALL(*mock_chrome_frame_host_, SetChromeProfileName(_))
          .Times(1);
      EXPECT_CALL(*mock_chrome_frame_host_, StartChromeFrame())
          .WillOnce(Return(S_OK));

      EXPECT_CALL(*mock_chrome_frame_host_, SetEventSink(NotNull()))
          .Times(1);
      EXPECT_CALL(*mock_chrome_frame_host_, SetEventSink(NULL))
          .Times(1);

      EXPECT_CALL(*mock_chrome_frame_host_, PostMessage(_, _))
          .WillRepeatedly(Return(S_OK));

      EXPECT_CALL(*mock_chrome_frame_host_, TearDown())
          .WillOnce(Return(S_OK));

      EXPECT_CALL(*mock_chrome_frame_host_, GetSessionId(NotNull()))
          .WillOnce(DoAll(SetArgumentPointee<0>(44), Return(S_OK)));
      EXPECT_CALL(*broker_, SetTabIdForHandle(44, _))
          .WillOnce(Return(S_OK));
    }

    return hr;
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

  // Stub content script manifest loading.
  void LoadManifestFile() {}

  // Make type public in this class.
  typedef BrowserHelperObject::BrowserHandlerMap BrowserHandlerMap;

  BrowserHandlerMap::const_iterator browsers_begin() const {
    return browsers_.begin();
  };
  BrowserHandlerMap::const_iterator browsers_end() const {
    return browsers_.end();
  };

  TestingFrameEventHandler* FindHandlerForUrl(const std::wstring& url) {
    BrowserHandlerMap::iterator it(browsers_.begin());
    BrowserHandlerMap::iterator end(browsers_.end());

    for (; it != end; ++it) {
      CComQIPtr<IWebBrowser2> browser(it->first);
      EXPECT_TRUE(browser != NULL);
      CComBSTR location_url;
      EXPECT_HRESULT_SUCCEEDED(browser->get_LocationURL(&location_url));

      if (0 == ::UrlCompare(url.c_str(), location_url, TRUE))
        return static_cast<TestingFrameEventHandler*>(it->second.get());
    }

    return NULL;
  }

  // Returns true iff we have exactly |num_frames| registering
  // the urls |resources[0..num_frames)|.
  bool ExpectHasFrames(size_t num_frames, const std::wstring* resources) {
    typedef BrowserHandlerMap::const_iterator iterator;
    iterator it(browsers_.begin());

    size_t count = 0;
    for (; it != browsers_.end(); ++it) {
      ++count;

      FrameEventHandler* handler =
          static_cast<FrameEventHandler*>(it->second.get());
      std::wstring url(handler->browser_url());
      const std::wstring* resources_end = resources + num_frames;
      const std::wstring* found = std::find(resources, resources_end, url);

      if (resources_end == found) {
        // A browser navigated to a file: URL reports the
        // raw file path as its URL, convert the file path
        // to a URL and search again.
        FilePath path(handler->browser_url());

        url = UTF8ToWide(net::FilePathToFileURL(path).spec());
        found = std::find(resources, resources_end, url);
      }

      EXPECT_TRUE(resources_end != found)
          << " unexpected frame URL " << url;
    }

    EXPECT_EQ(num_frames, count);
    return num_frames == count;
  }

  template <size_t N>
  bool ExpectHasFrames(const std::wstring (&resources)[N]) {
    return ExpectHasFrames(N, resources);
  }

  MockChromeFrameHost* mock_chrome_frame_host() const {
    return mock_chrome_frame_host_;
  }

  testing::MockTabEventsFunnel* mock_tab_events_funnel() {
    return &mock_tab_events_funnel_;
  }

  // We should use the executor mock that supports infobar in this test because
  // OnBeforeNavigate2 queries the executor for infobar interface.
  testing::MockTabInfobarExecutor* executor_;
  CComPtr<ICeeeTabExecutor> executor_keeper_;

  testing::MockBroker* broker_;
  CComPtr<ICeeeBrokerRegistrar> broker_keeper_;

 private:
  class TestingWebProgressNotifier : public WebProgressNotifier {
   public:
    class TesingNavigationEventsFunne : public WebNavigationEventsFunnel {
     public:
      HRESULT SendEventToBroker(const char*, const char*) {
        return S_OK;
      }
    };

    virtual WebNavigationEventsFunnel* webnavigation_events_funnel() {
      if (!webnavigation_events_funnel_.get())
        webnavigation_events_funnel_.reset(new TesingNavigationEventsFunne());

      return webnavigation_events_funnel_.get();
    }
  };

  MockChromeFrameHost* mock_chrome_frame_host_;
  StrictMock<testing::MockTabEventsFunnel> mock_tab_events_funnel_;
};

class TestBrowserSite
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InstanceCountMixin<TestBrowserSite>,
      public InitializingCoClass<TestBrowserSite>,
      public IServiceProviderImpl<TestBrowserSite> {
 public:
  BEGIN_COM_MAP(TestBrowserSite)
    COM_INTERFACE_ENTRY(IServiceProvider)
  END_COM_MAP()

  BEGIN_SERVICE_MAP(TestBrowserSite)
    SERVICE_ENTRY_CHAIN(browser_)
  END_SERVICE_MAP()

  HRESULT Initialize(TestBrowserSite **self, IWebBrowser2* browser) {
    *self = this;
    browser_ = browser;
    return S_OK;
  }

 public:
  CComPtr<IWebBrowser> browser_;
};

class BrowserEventSink
    : public BrowserEventSinkBase,
      public InitializingCoClass<BrowserEventSink> {
 public:
  // Disambiguate.
  using InitializingCoClass<BrowserEventSink>::CreateInitialized;

  HRESULT Initialize(BrowserEventSink** self, IWebBrowser2* browser) {
    *self = this;
    return BrowserEventSinkBase::Initialize(browser);
  }
};

class BrowserHelperObjectTest: public ShellBrowserTestImpl<BrowserEventSink> {
 public:
  typedef ShellBrowserTestImpl<BrowserEventSink> Super;

  BrowserHelperObjectTest() : bho_(NULL), site_(NULL) {
  }

  virtual void SetUp() {
    Super::SetUp();

    // Never torn down as other threads in the test may need it after
    // teardown.
    ScriptHost::set_default_debug_application(&debug_app);

    ASSERT_HRESULT_SUCCEEDED(
        TestingBrowserHelperObject::CreateInitialized(&bho_, &bho_keeper_));

    ASSERT_HRESULT_SUCCEEDED(
        TestBrowserSite::CreateInitialized(&site_, browser_, &site_keeper_));

    // Create and set expectations for the broker registrar related objects.
    ASSERT_HRESULT_SUCCEEDED(testing::MockTabInfobarExecutor::CreateInitialized(
        &bho_->executor_, &bho_->executor_keeper_));
    ASSERT_HRESULT_SUCCEEDED(
        testing::MockBroker::CreateInitialized(&bho_->broker_,
                                               &bho_->broker_keeper_));
    EXPECT_CALL(*bho_->broker_, RegisterTabExecutor(_,
        bho_->executor_keeper_.p)).WillRepeatedly(Return(S_OK));
    EXPECT_CALL(*bho_->broker_, UnregisterExecutor(_)).
        WillRepeatedly(Return(S_OK));
    EXPECT_CALL(*bho_->mock_tab_events_funnel(), OnCreated(_, _, _)).
        Times(AnyNumber());
    EXPECT_CALL(*bho_->mock_tab_events_funnel(), OnUpdated(_, _, _)).
        Times(AnyNumber());
    EXPECT_CALL(*bho_->executor_, Initialize(_)).WillOnce(Return(S_OK));
    EXPECT_CALL(*bho_->executor_, OnTopFrameBeforeNavigate(_)).
        WillRepeatedly(Return(S_OK));

    ASSERT_HRESULT_SUCCEEDED(bho_keeper_->SetSite(site_->GetUnknown()));
  }

  virtual void TearDown() {
    EXPECT_CALL(*bho_->mock_tab_events_funnel(), OnRemoved(_));
    EXPECT_CALL(*bho_->mock_tab_events_funnel(), OnTabUnmapped(_, _));
    ASSERT_HRESULT_SUCCEEDED(bho_keeper_->SetSite(NULL));

    site_ = NULL;
    site_keeper_.Release();

    bho_->executor_ = NULL;
    bho_->executor_keeper_.Release();

    bho_->broker_ = NULL;
    bho_->broker_keeper_.Release();

    bho_ = NULL;
    bho_keeper_.Release();

    Super::TearDown();
  }

 protected:
  TestingBrowserHelperObject* bho_;
  CComPtr<IObjectWithSite> bho_keeper_;

  TestBrowserSite* site_;
  CComPtr<IServiceProvider> site_keeper_;
};

// This test navigates a webbrowser control instance back and forth
// between a set of resources with our BHO attached. On every navigation
// we're asserting on the urls and number of frame event handlers the BHO
// has recorded, as well as the instance count of the testing frame
// event handler class.
// This is to ensure that:
// 1. Our frame event handlers get attached to all created frames.
// 2. That any "recycled" frame event handlers track their associated
//    document's URL changes.
// 3. That we don't leak discarded frame event handlers.
// 4. That we don't crash during any of this.
TEST_F(BrowserHelperObjectTest,
       FrameHandlerCreationAndDestructionOnNavigation) {
  const std::wstring two_frames_resources[] = {
      GetTestUrl(kTwoFramesPage),
      GetTestUrl(kFrameOne),
      GetTestUrl(kFrameTwo)};

  const std::wstring another_two_frames_resources[] = {
      GetTestUrl(kAnotherTwoFramesPage),
      GetTestUrl(kAnotherFrameOne),
      GetTestUrl(kAnotherFrameTwo)};

  const std::wstring simple_page_resources[] = { GetTestUrl(kSimplePage) };

  EXPECT_TRUE(NavigateBrowser(two_frames_resources[0]));
  EXPECT_TRUE(bho_->ExpectHasFrames(two_frames_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(two_frames_resources),
            TestingFrameEventHandler::instance_count());

  EXPECT_TRUE(NavigateBrowser(another_two_frames_resources[0]));
  EXPECT_TRUE(bho_->ExpectHasFrames(another_two_frames_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(another_two_frames_resources),
            TestingFrameEventHandler::instance_count());

  EXPECT_TRUE(NavigateBrowser(simple_page_resources[0]));
  EXPECT_TRUE(bho_->ExpectHasFrames(simple_page_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(simple_page_resources),
            TestingFrameEventHandler::instance_count());
}

// What motivated this test is the fact that at teardown time,
// sometimes the child->parent relationship between the browser
// instances we've encountered has been disrupted.
// So if you start with a frame hierarchy like
// A <- B <- C
// if you query the parent for C at the timepoint when B is
// reporting a COMPLETE->LOADING readystate change you'll find this:
// A <- B
// A <- C
// e.g. the C frame has been re-parented to the topmost webbrowser.
//
// Strangely I can't get this to repro under programmatic control
// against the webbrowser control. I suspect conditions are simply
// different in IE proper, or else there's something special to
// navigating by user event. I'm still leaving this code in as
// I hope to find a viable repro for this later, and because this
// code exercises some modes of navigation that the above test does
// not.
TEST_F(BrowserHelperObjectTest, DeepFramesAreCorrectlyHandled) {
  const std::wstring simple_page_resources[] = { GetTestUrl(kSimplePage) };
  const std::wstring deep_frames_resources[] = {
      GetTestUrl(kDeepFramesPage),
      GetTestUrl(kLevelOneFrame),
      GetTestUrl(kLevelTwoFrame),
      GetTestUrl(kFrameOne)
    };

  // Navigate to a deep frame hierarchy.
  EXPECT_TRUE(NavigateBrowser(deep_frames_resources[0]));
  EXPECT_TRUE(bho_->ExpectHasFrames(deep_frames_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(deep_frames_resources),
            TestingFrameEventHandler::instance_count());

  // Navigate to a simple page with only a top-level frame.
  EXPECT_TRUE(NavigateBrowser(simple_page_resources[0]));
  EXPECT_TRUE(bho_->ExpectHasFrames(simple_page_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(simple_page_resources),
            TestingFrameEventHandler::instance_count());

  // And back to a deep frame hierarchy.
  EXPECT_TRUE(NavigateBrowser(deep_frames_resources[0]));
  EXPECT_TRUE(bho_->ExpectHasFrames(deep_frames_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(deep_frames_resources),
            TestingFrameEventHandler::instance_count());


  // Refresh a mid-way frame.
  TestingFrameEventHandler* handler =
      bho_->FindHandlerForUrl(GetTestUrl(kLevelOneFrame));
  ASSERT_TRUE(handler);
  EXPECT_HRESULT_SUCCEEDED(handler->browser()->Refresh());
  ASSERT_FALSE(WaitForReadystateLoading());
  ASSERT_TRUE(WaitForReadystateComplete());

  // We should still have the same set of frames.
  EXPECT_TRUE(bho_->ExpectHasFrames(deep_frames_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(deep_frames_resources),
            TestingFrameEventHandler::instance_count());

  // Navigate a mid-way frame to a new resource.
  CComVariant empty;
  EXPECT_HRESULT_SUCCEEDED(
      browser_->Navigate2(&CComVariant(GetTestUrl(kTwoFramesPage).c_str()),
                                       &empty,
                                       &CComVariant(L"level_one"),
                                       &empty, &empty));

  ASSERT_FALSE(WaitForReadystateLoading());
  ASSERT_TRUE(WaitForReadystateComplete());

  // This should now be our resource set.
  const std::wstring mixed_frames_resources[] = {
      GetTestUrl(kDeepFramesPage),
      GetTestUrl(kLevelOneFrame),
      GetTestUrl(kTwoFramesPage),
      GetTestUrl(kFrameOne),
      GetTestUrl(kFrameTwo),
    };
  EXPECT_TRUE(bho_->ExpectHasFrames(mixed_frames_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(mixed_frames_resources),
            TestingFrameEventHandler::instance_count());
}

// What motivated this test is the fact that some webpage can dynamically
// create frames that don't get navigated. We first saw this on the
// http://www.zooborns.com site and found that it has some javascript that
// creates and iframe and manually fill its innerHTML which contains an
// iframe with a src that is navigated to. Our code used to assume that
// when a frame is navigated, its parent was previously navigated so we could
// attach the new frame to its parent that we had seen before. So we needed to
// add code to create a handler for the ancestors of such orphans.
//
TEST_F(BrowserHelperObjectTest, OrphanFrame) {
  const std::wstring simple_page_resources[] = { GetTestUrl(kSimplePage) };
  const std::wstring orphan_page_resources[] = {
      GetTestUrl(kOrphansPage),
      GetTestUrl(kOrphansPage),
      GetTestUrl(kFrameOne)
    };

  // Navigate to an orphanage.
  EXPECT_TRUE(NavigateBrowser(orphan_page_resources[0]));
  EXPECT_TRUE(bho_->ExpectHasFrames(orphan_page_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(orphan_page_resources),
            TestingFrameEventHandler::instance_count());

  // Navigate to a simple page with only a top-level frame.
  EXPECT_TRUE(NavigateBrowser(simple_page_resources[0]));
  EXPECT_TRUE(bho_->ExpectHasFrames(simple_page_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(simple_page_resources),
            TestingFrameEventHandler::instance_count());

  // And back to the orphanage.
  EXPECT_TRUE(NavigateBrowser(orphan_page_resources[0]));
  // On fast machines, we don't wait long enough for everything to be completed.
  // So we may already be OK, or we may need to wait for an extra COMPLETE.
  // When we re-navigate to the ophans page like this, for some reason, we first
  // get one COMPLETE ready state, and then a LOADING and then another COMPLETE.
  if (arraysize(orphan_page_resources) !=
      TestingFrameEventHandler::instance_count()) {
    ASSERT_TRUE(WaitForReadystateComplete());
  }
  EXPECT_TRUE(bho_->ExpectHasFrames(orphan_page_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(orphan_page_resources),
    TestingFrameEventHandler::instance_count());

  // Refresh a deep frame.
  TestingFrameEventHandler* handler =
      bho_->FindHandlerForUrl(GetTestUrl(kFrameOne));
  ASSERT_TRUE(handler);
  EXPECT_HRESULT_SUCCEEDED(handler->browser()->Refresh());
  ASSERT_FALSE(WaitForReadystateLoading());
  ASSERT_TRUE(WaitForReadystateComplete());

  // We should still have the same set of frames.
  EXPECT_TRUE(bho_->ExpectHasFrames(orphan_page_resources));

  // One handler per resource.
  EXPECT_EQ(arraysize(orphan_page_resources),
            TestingFrameEventHandler::instance_count());
}

}  // namespace
