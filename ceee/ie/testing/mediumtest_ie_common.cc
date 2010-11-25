// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A common test fixture for testing against a captive shell browser
// control instance - which is as close to habitating the belly of the IE
// beast as can be done with a modicum of safety and sanitation.
#include "ceee/ie/testing/mediumtest_ie_common.h"

#include <atlcrack.h>
#include <atlsync.h>
#include <atlwin.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtmdid.h>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/base_paths_win.h"
#include "base/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/testing/utils/instance_count_mixin.h"


namespace testing {

// A test resource that's a simple single-resource page.
const wchar_t* kSimplePage = L"simple_page.html";
// A test resource that's a frameset referencing the two frames below.
const wchar_t* kTwoFramesPage = L"two_frames.html";
const wchar_t* kFrameOne = L"frame_one.html";
const wchar_t* kFrameTwo = L"frame_two.html";
// Another test resource that's a frameset referencing the two frames below.
const wchar_t* kAnotherTwoFramesPage = L"another_two_frames.html";
const wchar_t* kAnotherFrameOne = L"another_frame_one.html";
const wchar_t* kAnotherFrameTwo = L"another_frame_two.html";

// A test resource that's a top-level frameset with two nested iframes
// the inner one of which refers frame_one.html.
const wchar_t* kDeepFramesPage = L"deep_frames.html";
const wchar_t* kLevelOneFrame = L"level_one_frame.html";
const wchar_t* kLevelTwoFrame = L"level_two_frame.html";

// A test resource that have javascript generate frames that look orphan.
const wchar_t* kOrphansPage = L"orphans.html";

std::set<READYSTATE> BrowserEventSinkBase::seen_states_;

// Constructs a res: url to the test resource in our module.
std::wstring GetTestUrl(const wchar_t* resource_name) {
  FilePath path;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &path));
  path = path.Append(FILE_PATH_LITERAL("ceee"))
             .Append(FILE_PATH_LITERAL("ie"))
             .Append(FILE_PATH_LITERAL("testing"))
             .Append(FILE_PATH_LITERAL("test_data"))
             .Append(resource_name);

  return UTF8ToWide(net::FilePathToFileURL(path).spec());
}

// Returns the path to our temp folder.
std::wstring GetTempPath() {
  FilePath temp;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &temp));

  return temp.value();
}

_ATL_FUNC_INFO handler_type_idispatch_ =
    { CC_STDCALL, VT_EMPTY, 1, { VT_DISPATCH } };
_ATL_FUNC_INFO handler_type_idispatch_variantptr_ =
    { CC_STDCALL, VT_EMPTY, 2, { VT_DISPATCH, VT_BYREF | VT_VARIANT } };


void BrowserEventSinkBase::GetDescription(std::string* description) const {
  description->clear();
  description->append("TestBrowserEventSink");
}

HRESULT BrowserEventSinkBase::Initialize(IWebBrowser2* browser) {
  browser_ = browser;
  HRESULT hr = DispEventAdvise(browser_);
  if (SUCCEEDED(hr)) {
    hr = AtlAdvise(browser_,
                   GetUnknown(),
                   IID_IPropertyNotifySink,
                   &prop_notify_cookie_);
  }

  return hr;
}

void BrowserEventSinkBase::TearDown() {
  EXPECT_HRESULT_SUCCEEDED(DispEventUnadvise(browser_));
  EXPECT_HRESULT_SUCCEEDED(AtlUnadvise(browser_,
                                       IID_IPropertyNotifySink,
                                       prop_notify_cookie_));
}

STDMETHODIMP BrowserEventSinkBase::OnChanged(DISPID changed_property) {
  ATLTRACE("OnChanged(%d)\n", changed_property);

  READYSTATE ready_state = READYSTATE_UNINITIALIZED;
  browser_->get_ReadyState(&ready_state);
  ATLTRACE("READYSTATE: %d\n", ready_state);
  seen_states_.insert(ready_state);

  return S_OK;
}

STDMETHODIMP BrowserEventSinkBase::OnRequestEdit(DISPID changed_property) {
  ATLTRACE("OnRequestEdit(%d)\n", changed_property);
  return S_OK;
}

STDMETHODIMP_(void) BrowserEventSinkBase::OnNavigateComplete(
    IDispatch* browser_disp, VARIANT* url_var) {
}

STDMETHODIMP_(void) BrowserEventSinkBase::OnDocumentComplete(
    IDispatch* browser_disp, VARIANT* url) {
}

void ShellBrowserTestBase::SetUpTestCase() {
  // CoInitialize(NULL) serves two purposes here:
  // 1. if we're running in a non-initialized apartment, it initializes it.
  // 2. we need to be in an STA to use the shell browser, and if we're
  //    for whatever reason running in an MTA, it will fail.
  ASSERT_HRESULT_SUCCEEDED(::CoInitialize(NULL));
  ASSERT_TRUE(AtlAxWinInit());
}

void ShellBrowserTestBase::TearDownTestCase() {
  EXPECT_TRUE(AtlAxWinTerm());
  ::CoUninitialize();
}

void ShellBrowserTestBase::SetUp() {
  ASSERT_TRUE(Create(HWND_MESSAGE) != NULL);

  // Create the webbrowser control and get the AX host.
  ASSERT_HRESULT_SUCCEEDED(
        AtlAxCreateControl(CComBSTR(CLSID_WebBrowser),
                           m_hWnd,
                           NULL,
                           &host_));

  // Get the control's top-level IWebBrowser.
  CComPtr<IUnknown> control;
  ASSERT_HRESULT_SUCCEEDED(AtlAxGetControl(m_hWnd, &control));
  ASSERT_HRESULT_SUCCEEDED(control->QueryInterface(&browser_));

  ASSERT_HRESULT_SUCCEEDED(CreateEventSink(browser_));
}

void ShellBrowserTestBase::TearDown() {
  // Navigating the browser to a folder creates a non-webbrowser document,
  // which shakes off all our frame handlers.
  EXPECT_TRUE(NavigateBrowser(GetTempPath()));

  // Tear down the rest of our stuff.
  if (event_sink_)
    event_sink_->TearDown();

  event_sink_keeper_.Release();
  host_.Release();
  browser_.Release();

  // Finally blow away the host window.
  if (IsWindow())
    DestroyWindow();

  // Should have retained no objects past this point.
  EXPECT_EQ(0, InstanceCountMixinBase::all_instance_count());
  if (InstanceCountMixinBase::all_instance_count() > 0) {
    InstanceCountMixinBase::LogLeakedInstances();
  }
}

bool ShellBrowserTestBase::NavigateBrowser(const std::wstring& url) {
  CComVariant empty;
  HRESULT hr = browser_->Navigate2(&CComVariant(url.c_str()),
                                   &empty, &empty,
                                   &empty, &empty);
  EXPECT_HRESULT_SUCCEEDED(hr);
  if (FAILED(hr))
    return false;

  return WaitForReadystateComplete();
}

bool ShellBrowserTestBase::WaitForReadystateComplete() {
  return WaitForReadystate(READYSTATE_COMPLETE);
}

bool ShellBrowserTestBase::WaitForReadystateLoading() {
  return WaitForReadystate(READYSTATE_LOADING);
}

bool ShellBrowserTestBase::WaitForReadystateWithTimerId(
    READYSTATE waiting_for, UINT_PTR timer_id) {
  while (true) {
    if (!browser_ || !event_sink_)
      return false;

    // Is the browser in the required state now?
    READYSTATE ready_state = READYSTATE_UNINITIALIZED;
    HRESULT hr = browser_->get_ReadyState(&ready_state);
    if (FAILED(hr))
      return false;

    if (ready_state == waiting_for) {
      event_sink_->remove_state(waiting_for);
      return true;
    }

    // Has the state been seen?
    if (event_sink_->has_state(waiting_for)) {
      event_sink_->remove_state(waiting_for);
      return true;
    }

    MSG msg = {};
    if (!GetMessage(&msg, 0, 0, 0)) {
      // WM_QUIT.
      return false;
    }

    if (msg.message == WM_TIMER &&
        msg.hwnd == NULL &&
        msg.wParam == timer_id) {
      // Timeout.
      return false;
    }

    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }
}

bool ShellBrowserTestBase::WaitForReadystate(READYSTATE waiting_for) {
  if (!event_sink_)
    return false;

  // Clear the seen states.
  event_sink_->clear_states();

  // Bound the wait by setting a timer.
  UINT_PTR timer_id = ::SetTimer(NULL,
                                 0,
                                 wait_timeout_ms_,
                                 NULL);
  EXPECT_NE(0, timer_id);
  bool ret = WaitForReadystateWithTimerId(waiting_for, timer_id);
  EXPECT_TRUE(::KillTimer(NULL, timer_id));

  return ret;
}

}  // namespace testing
