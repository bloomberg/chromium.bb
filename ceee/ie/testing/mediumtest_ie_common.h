// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A common test fixture for testing against a captive shell browser
// control instance - which is as close to habitating the belly of the IE
// beast as can be done with a modicum of safety and sanitation.
#ifndef CEEE_IE_TESTING_MEDIUMTEST_IE_COMMON_H_
#define CEEE_IE_TESTING_MEDIUMTEST_IE_COMMON_H_

#include <atlbase.h>
#include <atlcrack.h>
#include <atlwin.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtmdid.h>

#include <set>
#include <string>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/base_paths_win.h"
#include "gtest/gtest.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/instance_count_mixin.h"

namespace testing {

// A test resource that's a simple single-resource page.
extern const wchar_t* kSimplePage;
// A test resource that's a frameset referencing the two frames below.
extern const wchar_t* kTwoFramesPage;
extern const wchar_t* kFrameOne;
extern const wchar_t* kFrameTwo;

// Another copy of above on new URLs.
extern const wchar_t* kAnotherTwoFramesPage;
extern const wchar_t* kAnotherFrameOne;
extern const wchar_t* kAnotherFrameTwo;

// A test resource that's a top-level frameset with two nested iframes
// the inner one of which refers frame_one.html.
extern const wchar_t* kDeepFramesPage;
extern const wchar_t* kLevelOneFrame;
extern const wchar_t* kLevelTwoFrame;

// A test resource that have javascript generate frames that look orphan.
extern const wchar_t* kOrphansPage;

// Constructs a file: url to the test file our source directory.
std::wstring GetTestUrl(const wchar_t* resource_name);

// Returns the path to our temp folder.
std::wstring GetTempPath();

extern _ATL_FUNC_INFO handler_type_idispatch_;
extern _ATL_FUNC_INFO handler_type_idispatch_variantptr_;

// Base class that implements the rudiments of sinking events from
// an IWebBrowser.
class BrowserEventSinkBase
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<BrowserEventSinkBase>,
      public InstanceCountMixin<BrowserEventSinkBase>,
      public IDispEventSimpleImpl<0,
                                  BrowserEventSinkBase,
                                  &DIID_DWebBrowserEvents2>,
      public IPropertyNotifySink {
 public:
  BEGIN_COM_MAP(BrowserEventSinkBase)
    COM_INTERFACE_ENTRY(IPropertyNotifySink)
  END_COM_MAP()

  BEGIN_SINK_MAP(BrowserEventSinkBase)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,
                    OnNavigateComplete, &handler_type_idispatch_variantptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,
                    OnDocumentComplete, &handler_type_idispatch_variantptr_)
  END_SINK_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT();

  BrowserEventSinkBase() : prop_notify_cookie_(-1) {
  }

  virtual void GetDescription(std::string* description) const;
  HRESULT Initialize(IWebBrowser2* browser);

  void TearDown();

  // @name IPropertyNotifySink implementation.
  // @{
  STDMETHOD(OnChanged)(DISPID changed_property);
  STDMETHOD(OnRequestEdit)(DISPID changed_property);
  // @}

  // @name DWebBrowserEvents event handler implementation.
  //    Override these as needed.
  // @{
  STDMETHOD_(void, OnNavigateComplete)(IDispatch* browser_disp,
                                       VARIANT* url_var);
  STDMETHOD_(void, OnDocumentComplete)(IDispatch* browser_disp, VARIANT* url);

  static bool has_state(READYSTATE state) {
    return seen_states_.find(state) != seen_states_.end();
  }
  static void add_state(READYSTATE state) {
    seen_states_.insert(state);
  }
  static void remove_state(READYSTATE state) {
    seen_states_.erase(state);
  }
  static void clear_states() {
    seen_states_.clear();
  }

 private:
  // This bitset records the readystate values seen in OnChange.
  // The point of this is to make the WaitForReadyState function
  // below work in the case where the browser takes multiple
  // readystate changes while processing a single event. This
  // will occur in the refresh case, where the top-level browser
  // and individual document objects transition from COMPLETE to
  // LOADING and then back to COMPLETE on processing a single event.
  static std::set<READYSTATE> seen_states_;

  // Advise cookie for property notify sink.
  DWORD prop_notify_cookie_;

  CComPtr<IWebBrowser2> browser_;
};

// Test fixture base class.
class ShellBrowserTestBase
    : public testing::Test,
      public CWindowImpl<ShellBrowserTestBase> {
 public:
  // Default timeout for pumping events.
  static const UINT kWaitTimeoutMs = 2000;

  ShellBrowserTestBase()
      : wait_timeout_ms_(kWaitTimeoutMs) {
  }

  static void SetUpTestCase();
  static void TearDownTestCase();

  void SetUp();
  void TearDown();

  // Navigates the browser to the given URL and waits for
  // the top-level browser's readystate to reach complete.
  bool NavigateBrowser(const std::wstring& url);

  // Wait for the top-level browser's readystate to reach
  // READYSTATE_COMPLETE, READYSTATE_LOADING or wait_for,
  // respectively.
  bool WaitForReadystateComplete();
  bool WaitForReadystateLoading();
  bool WaitForReadystate(READYSTATE wait_for);

  BEGIN_MSG_MAP_EX(BrowserEventTest)
  END_MSG_MAP()

 protected:
  bool WaitForReadystateWithTimerId(READYSTATE waiting_for, UINT_PTR timer_id);
  virtual HRESULT CreateEventSink(IWebBrowser2* browser) = 0;

  // Timeout for pumping messages in WaitForReadyState.
  UINT wait_timeout_ms_;

  CComPtr<IUnknown> host_;
  CComPtr<IWebBrowser2> browser_;

  BrowserEventSinkBase* event_sink_;
  CComPtr<IUnknown> event_sink_keeper_;
};

template <class EventSinkImpl>
class ShellBrowserTestImpl: public ShellBrowserTestBase {
 public:
 protected:
  virtual HRESULT CreateEventSink(IWebBrowser2* browser) {
    EventSinkImpl* sink;
    HRESULT hr = EventSinkImpl::CreateInitialized(&sink,
                                                  browser,
                                                  &event_sink_keeper_);
    event_sink_ = sink;
    return hr;
  }

  // Handy accessor to retrieve the event sink by actual type.
  EventSinkImpl* event_sink() const {
    return static_cast<EventSinkImpl*>(event_sink_);
  }
};

}  // namespace testing

#endif  // CEEE_IE_TESTING_MEDIUMTEST_IE_COMMON_H_
