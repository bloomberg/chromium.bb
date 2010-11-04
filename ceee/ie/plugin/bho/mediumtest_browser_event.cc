// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A test that hosts and excercises the webbrowser control to test
// its event firing behavior.
#include <atlcrack.h>
#include <atlsync.h>
#include <atlwin.h>
#include <set>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/base_paths_win.h"
#include "ceee/ie/testing/mediumtest_ie_common.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/instance_count_mixin.h"


namespace {

using testing::InstanceCountMixin;
using testing::InstanceCountMixinBase;

using testing::BrowserEventSinkBase;
using testing::GetTestUrl;
using testing::GetTempPath;
using testing::ShellBrowserTestImpl;

// {AFF1D082-6B03-4b29-9521-E52240F6333B}
const GUID IID_Dummy =
    { 0xaff1d082, 0x6b03, 0x4b29,
        { 0x95, 0x21, 0xe5, 0x22, 0x40, 0xf6, 0x33, 0x3b } };

class TestBrowserEventSink;

class TestFrameEventHandler
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<TestFrameEventHandler>,
      public InstanceCountMixin<TestFrameEventHandler>,
      public IPropertyNotifySink,
      public IAdviseSink {
 public:
  BEGIN_COM_MAP(TestFrameEventHandler)
    COM_INTERFACE_ENTRY(IPropertyNotifySink)
    COM_INTERFACE_ENTRY(IAdviseSink)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT();

  TestFrameEventHandler() : event_sink_(NULL),
      document_property_notify_sink_cookie_(-1),
      advise_sink_cookie_(-1) {
  }

  virtual ~TestFrameEventHandler() {
    ATLTRACE("~TestFrameEventHandler[%ws]\n", url_ ? url_ : L"");
  }

  void DetachFromSink();

  STDMETHOD(OnChanged)(DISPID changed_property);
  STDMETHOD(OnRequestEdit)(DISPID changed_property) {
    ATLTRACE("OnRequestEdit(%d)\n", changed_property);
    return S_OK;
  }

  // IAdviseSink
  STDMETHOD_(void, OnDataChange)(FORMATETC *pFormatetc, STGMEDIUM *pStgmed) {
    ATLTRACE("%s\n", __FUNCTION__);
  }
  STDMETHOD_(void, OnViewChange)(DWORD dwAspect, LONG lindex) {
    ATLTRACE("%s\n", __FUNCTION__);
  }
  STDMETHOD_(void, OnRename)(IMoniker *pmk) {
    ATLTRACE("%s\n", __FUNCTION__);
  }
  STDMETHOD_(void, OnSave)() {
    ATLTRACE("%s\n", __FUNCTION__);
  }
  STDMETHOD_(void, OnClose)();

  virtual void GetDescription(std::string* description) const {
    description->clear();
    description->append("TestFrameEventHandler");
    // TODO(siggi@chromium.org): append URL
  }

  HRESULT Initialize(TestBrowserEventSink* event_sink,
                     IWebBrowser2* browser,
                     BSTR url);
  void FinalRelease();

  void set_url(const wchar_t* url) { url_ = url; }
  const wchar_t* url() const { return url_ ? url_ : L""; }

  template <class Interface>
  HRESULT GetBrowser(Interface** browser) {
    return browser_.QueryInterface(browser);
  }

 private:
  CComBSTR url_;
  CComDispatchDriver document_;
  CComPtr<IWebBrowser2> browser_;
  TestBrowserEventSink* event_sink_;

  DWORD advise_sink_cookie_;
  DWORD document_property_notify_sink_cookie_;
};

class TestBrowserEventSink
    : public BrowserEventSinkBase,
      public InitializingCoClass<TestBrowserEventSink> {
 public:
  // Disambiguate.
  using InitializingCoClass<TestBrowserEventSink>::CreateInitialized;

  HRESULT Initialize(TestBrowserEventSink** self, IWebBrowser2* browser) {
    *self = this;
    return BrowserEventSinkBase::Initialize(browser);
  }

  // We have seen cases where we get destroyed while the ATL::CAxHostWindow
  // may still hold references to frame handlers.
  void FinalRelease() {
    FrameHandlerMap::iterator it(frame_handlers_.begin());
    FrameHandlerMap::iterator end(frame_handlers_.end());

    // Since they will detach from us as we clear them, we must not do it
    // while we loop.
    std::vector<TestFrameEventHandler*> to_be_detached;
    for (; it != end; ++it) {
      to_be_detached.push_back(it->second);
    }
    for (size_t index = 0; index < to_be_detached.size(); ++index) {
      to_be_detached[index]->DetachFromSink();
    }
    ASSERT_EQ(0, frame_handlers_.size());
  }

  virtual void GetDescription(std::string* description) const {
    description->clear();
    description->append("TestBrowserEventSink");
  }

  void AttachHandler(IWebBrowser2* browser, TestFrameEventHandler* handler) {
    ASSERT_TRUE(browser != NULL && handler != NULL);
    CComPtr<IUnknown> browser_unk;
    ASSERT_HRESULT_SUCCEEDED(browser->QueryInterface(&browser_unk));

    // We shouldn't already have one.
    ASSERT_TRUE(NULL == FindHandlerForBrowser(browser));

    frame_handlers_.insert(std::make_pair(browser_unk, handler));
  }

  void DetachHandler(IWebBrowser2* browser, TestFrameEventHandler* handler) {
    ASSERT_TRUE(browser != NULL && handler != NULL);

    // It should already be registered.
    ASSERT_TRUE(NULL != FindHandlerForBrowser(browser));

    CComPtr<IUnknown> browser_unk;
    ASSERT_HRESULT_SUCCEEDED(browser->QueryInterface(&browser_unk));
    ASSERT_EQ(1, frame_handlers_.erase(browser_unk));
  }

  TestFrameEventHandler* FindHandlerForBrowser(IDispatch* browser) {
    CComPtr<IUnknown> browser_unk;
    EXPECT_HRESULT_SUCCEEDED(browser->QueryInterface(&browser_unk));

    FrameHandlerMap::iterator it(frame_handlers_.find(browser_unk));
    if (it == frame_handlers_.end())
      return NULL;

    return it->second;
  }

  TestFrameEventHandler* FindHandlerForUrl(const std::wstring& url) {
    FrameHandlerMap::iterator it(frame_handlers_.begin());
    FrameHandlerMap::iterator end(frame_handlers_.end());

    for (; it != end; ++it) {
      CComQIPtr<IWebBrowser2> browser(it->first);
      EXPECT_TRUE(browser != NULL);
      CComBSTR location_url;
      EXPECT_HRESULT_SUCCEEDED(browser->get_LocationURL(&location_url));

      if (0 == ::UrlCompare(url.c_str(), location_url, TRUE))
        return it->second;
    }

    return NULL;
  }

  // Override.
  STDMETHOD_(void, OnNavigateComplete)(IDispatch* browser_disp,
                                       VARIANT* url_var) {
    CComBSTR url;
    if (V_VT(url_var) == VT_BSTR)
      url = V_BSTR(url_var);

    TestFrameEventHandler* frame_handler = FindHandlerForBrowser(browser_disp);
    if (!frame_handler) {
      CComQIPtr<IWebBrowser2> browser(browser_disp);
      ASSERT_TRUE(browser != NULL);

      CComPtr<IUnknown> frame_handler_keeper;
      ASSERT_HRESULT_SUCCEEDED(
          TestFrameEventHandler::CreateInitialized(this,
                                                   browser,
                                                   url,
                                                   &frame_handler_keeper));
    } else {
      ATLTRACE("FrameHandler[%ws] -> %ws\n", frame_handler->url(), url);
      frame_handler->set_url(url);
    }
  }

 private:
  typedef std::map<IUnknown*, TestFrameEventHandler*> FrameHandlerMap;
  // Keeps a map from a frame or top-level browser's identifying
  // IUnknown to the frame event handler instance attached.
  FrameHandlerMap frame_handlers_;
};


STDMETHODIMP TestFrameEventHandler::OnChanged(DISPID changed_property) {
  ATLTRACE("OnChanged(%d)\n", changed_property);

  if (changed_property == DISPID_READYSTATE) {
    CComVariant ready_state;
    CComDispatchDriver document(document_);
    EXPECT_TRUE(document != NULL);
    EXPECT_HRESULT_SUCCEEDED(document.GetProperty(DISPID_READYSTATE,
                                                  &ready_state));
    EXPECT_EQ(V_VT(&ready_state), VT_I4);
    ATLTRACE("READYSTATE Frame[%ws]: %d\n", url_, ready_state.lVal);

    TestBrowserEventSink::add_state(static_cast<READYSTATE>(ready_state.lVal));
  }

  return S_OK;
}

HRESULT TestFrameEventHandler::Initialize(TestBrowserEventSink* event_sink,
                                          IWebBrowser2* browser,
                                          BSTR url) {
  EXPECT_HRESULT_SUCCEEDED(browser->get_Document(&document_));

  CComQIPtr<IHTMLDocument2> html_document2(document_);
  if (html_document2 != NULL) {
    event_sink_ = event_sink;
    browser_ = browser;
    url_ = url;
    EXPECT_HRESULT_SUCCEEDED(AtlAdvise(document_,
                                       GetUnknown(),
                                       IID_IPropertyNotifySink,
                                       &document_property_notify_sink_cookie_));

    ATLTRACE("TestFrameEventHandler::Initialize[%ws]\n", url_ ? url_ : L"");

    CComQIPtr<IOleObject> document_ole_object(document_);
    EXPECT_TRUE(document_ole_object != NULL);
    EXPECT_HRESULT_SUCCEEDED(
        document_ole_object->Advise(this, &advise_sink_cookie_));

    event_sink_->AttachHandler(browser_, this);
  } else {
    // This happens when we're navigated to e.g. a PDF doc or a folder.
  }

  return S_OK;
}


void TestFrameEventHandler::DetachFromSink() {
  ASSERT_TRUE(event_sink_ != NULL);
  event_sink_->DetachHandler(browser_, this);
  event_sink_ = NULL;
}

void TestFrameEventHandler::FinalRelease() {
  if (event_sink_ && browser_)
    event_sink_->DetachHandler(browser_, this);
  browser_.Release();
  document_.Release();
}

STDMETHODIMP_(void) TestFrameEventHandler::OnClose() {
  EXPECT_HRESULT_SUCCEEDED(AtlUnadvise(document_,
                                       IID_IPropertyNotifySink,
                                       document_property_notify_sink_cookie_));

  CComQIPtr<IOleObject> document_ole_object(document_);
  EXPECT_TRUE(document_ole_object != NULL);
  EXPECT_HRESULT_SUCCEEDED(
      document_ole_object->Unadvise(advise_sink_cookie_));
}

class BrowserEventTest: public ShellBrowserTestImpl<TestBrowserEventSink> {
};

const wchar_t* kSimplePage = L"simple_page.html";

TEST_F(BrowserEventTest, RefreshTopLevelBrowserRetainsFrameHandler) {
  EXPECT_TRUE(NavigateBrowser(GetTestUrl(kSimplePage)));

  // We should have only one frame at this point.
  EXPECT_EQ(1, TestFrameEventHandler::instance_count());

  // Refreshing the top-level browser retains it.
  EXPECT_HRESULT_SUCCEEDED(browser_->Refresh());
  EXPECT_TRUE(WaitForReadystateLoading());
  EXPECT_TRUE(WaitForReadystateComplete());

  // Still there after refresh.
  EXPECT_EQ(1, TestFrameEventHandler::instance_count());
}

const wchar_t* kTwoFramesPage = L"two_frames.html";
const wchar_t* kFrameOne = L"frame_one.html";
const wchar_t* kFrameTwo = L"frame_two.html";

TEST_F(BrowserEventTest, NavigateToFrames) {
  EXPECT_TRUE(NavigateBrowser(GetTestUrl(kTwoFramesPage)));

  // We should have three frame handlers at this point.
  EXPECT_EQ(3, TestFrameEventHandler::instance_count());

  // We should have a handler for each of these.
  TestFrameEventHandler* two_frames =
      event_sink()->FindHandlerForUrl(GetTestUrl(kTwoFramesPage));
  TestFrameEventHandler* frame_one =
      event_sink()->FindHandlerForUrl(GetTestUrl(kFrameOne));
  TestFrameEventHandler* frame_two =
      event_sink()->FindHandlerForUrl(GetTestUrl(kFrameTwo));
  ASSERT_TRUE(two_frames != NULL);
  ASSERT_TRUE(frame_one != NULL);
  ASSERT_TRUE(frame_two != NULL);

  // Noteworthy fact: the top level browser implements an
  // IPropertyNotifySink connection point, but the sub-browsers
  // for the frames do not.
  {
    CComQIPtr<IConnectionPointContainer> cpc;
    ASSERT_HRESULT_SUCCEEDED(two_frames->GetBrowser(&cpc));
    ASSERT_TRUE(cpc != NULL);
    CComPtr<IConnectionPoint> cp;
    EXPECT_HRESULT_SUCCEEDED(
        cpc->FindConnectionPoint(IID_IPropertyNotifySink, &cp));
  }

  {
    CComQIPtr<IConnectionPointContainer> cpc;
    ASSERT_HRESULT_SUCCEEDED(frame_one->GetBrowser(&cpc));
    ASSERT_TRUE(cpc != NULL);
    CComPtr<IConnectionPoint> cp;
    EXPECT_HRESULT_FAILED(
        cpc->FindConnectionPoint(IID_IPropertyNotifySink, &cp));
  }

  {
    CComQIPtr<IConnectionPointContainer> cpc;
    ASSERT_HRESULT_SUCCEEDED(frame_two->GetBrowser(&cpc));
    ASSERT_TRUE(cpc != NULL);
    CComPtr<IConnectionPoint> cp;
    EXPECT_HRESULT_FAILED(
        cpc->FindConnectionPoint(IID_IPropertyNotifySink, &cp));
  }

  // Test sub-frame document IPropertyNotifySink.
  {
    CComPtr<IWebBrowser2> browser;
    ASSERT_HRESULT_SUCCEEDED(frame_two->GetBrowser(&browser));

    CComPtr<IDispatch> document_disp;
    ASSERT_HRESULT_SUCCEEDED(browser->get_Document(&document_disp));

    CComPtr<IConnectionPointContainer> cpc;
    ASSERT_HRESULT_SUCCEEDED(document_disp->QueryInterface(&cpc));
    CComPtr<IConnectionPoint> cp;
    ASSERT_HRESULT_SUCCEEDED(
        cpc->FindConnectionPoint(IID_IPropertyNotifySink, &cp));
  }
}

TEST_F(BrowserEventTest, ReNavigateToSamePageRetainsEventHandler) {
  const std::wstring url(GetTestUrl(kSimplePage));
  EXPECT_TRUE(NavigateBrowser(url));

  // We should have a frame handler attached now.
  EXPECT_EQ(1, TestFrameEventHandler::instance_count());

  // Retrieve it and make sure it doesn't die.
  TestFrameEventHandler* handler_before =
      event_sink()->FindHandlerForUrl(url);

  ASSERT_TRUE(handler_before != NULL);
  CComPtr<IUnknown> handler_before_keeper(handler_before->GetUnknown());

  // Re-navigate the browser to the same page.
  EXPECT_TRUE(NavigateBrowser(GetTestUrl(kSimplePage)));
  // Note: on re-navigation we don't see the top-level
  // browser's readystate drop, I guess that only happens
  // on transitions between content types. E.g. when a
  // navigation requires the shell browser to instantiate
  // a new type of document, such as going from a
  // HTML doc to a PDF doc or the like.
  EXPECT_FALSE(WaitForReadystateLoading());
  EXPECT_TRUE(WaitForReadystateComplete());

  // We should only have the one frame handler in existence now.
  EXPECT_EQ(1, TestFrameEventHandler::instance_count());

  // Retrieve the new one.
  TestFrameEventHandler* handler_after =
      event_sink()->FindHandlerForUrl(GetTestUrl(kSimplePage));

  ASSERT_EQ(handler_before, handler_after);

  // Release the old one, it should still stay around
  handler_before_keeper.Release();
  EXPECT_EQ(1, TestFrameEventHandler::instance_count());
}

TEST_F(BrowserEventTest, NavigateToDifferentPageRetainsEventHandler) {
  const std::wstring first_url(GetTestUrl(kSimplePage));
  EXPECT_TRUE(NavigateBrowser(first_url));

  // We should have a frame handler attached now.
  EXPECT_EQ(1, TestFrameEventHandler::instance_count());

  // Retrieve it and make sure it doesn't die.
  TestFrameEventHandler* handler_before =
      event_sink()->FindHandlerForUrl(first_url);

  ASSERT_TRUE(handler_before != NULL);
  CComPtr<IUnknown> handler_before_keeper(handler_before->GetUnknown());

  // Navigate the browser to another page.
  const std::wstring second_url(GetTestUrl(kTwoFramesPage));
  EXPECT_TRUE(NavigateBrowser(second_url));
  EXPECT_FALSE(WaitForReadystateLoading());
  EXPECT_TRUE(WaitForReadystateComplete());

  // We should have the three frame handlers in existence now.
  EXPECT_EQ(3, TestFrameEventHandler::instance_count());

  // Retrieve the new one for the top-level browser.
  TestFrameEventHandler* handler_after =
      event_sink()->FindHandlerForUrl(second_url);

  ASSERT_EQ(handler_before, handler_after);

  // Release the old one, it should still stay around
  handler_before_keeper.Release();
  EXPECT_EQ(3, TestFrameEventHandler::instance_count());
}

// The failure may be IE9-specific.
TEST_F(BrowserEventTest, FLAKY_RefreshFrameBrowserRetainsHandler) {
  EXPECT_TRUE(NavigateBrowser(GetTestUrl(kTwoFramesPage)));

  // We should have three frame handlers at this point.
  EXPECT_EQ(3, TestFrameEventHandler::instance_count());

  // Get one of the frames.
  TestFrameEventHandler* frame_two =
      event_sink()->FindHandlerForUrl(GetTestUrl(kFrameTwo));
  ASSERT_TRUE(frame_two != NULL);

  // Now refresh a sub-browser instance, let it settle,
  // observe its frame event handler is still around and
  // has signalled a readystate transition.
  CComPtr<IWebBrowser2> browser;
  ASSERT_HRESULT_SUCCEEDED(frame_two->GetBrowser(&browser));
  ASSERT_HRESULT_SUCCEEDED(browser->Refresh());

  EXPECT_TRUE(WaitForReadystateLoading());
  EXPECT_TRUE(WaitForReadystateComplete());

  // We should have all three frame handlers at this point.
  EXPECT_EQ(3, TestFrameEventHandler::instance_count());
  frame_two = event_sink()->FindHandlerForUrl(GetTestUrl(kFrameTwo));
  EXPECT_TRUE(frame_two != NULL);
}

}  // namespace
