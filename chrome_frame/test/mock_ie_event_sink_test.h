// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_TEST_H_
#define CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_TEST_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/chrome_frame_ui_test_utils.h"
#include "chrome_frame/test/ie_event_sink.h"
#include "chrome_frame/test/test_server.h"
#include "chrome_frame/test/test_with_web_server.h"
#include "chrome_frame/test/win_event_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/xulrunner-sdk/win/include/accessibility/AccessibleEventId.h"

namespace chrome_frame_test {

// Convenience enum for specifying whether a load occurred in IE or CF.
enum LoadedInRenderer {
  IN_IE = 0,
  IN_CF
};

// This mocks an IEEventListener, providing methods for expecting certain
// sequences of events.
class MockIEEventSink : public IEEventListener {
 public:
  MockIEEventSink() {
    CComObject<IEEventSink>::CreateInstance(&event_sink_);
    event_sink_->AddRef();
  }

  ~MockIEEventSink() {
    Detach();
    int reference_count = event_sink_->reference_count();
    LOG_IF(ERROR, reference_count != 1)
        << "Event sink is still referenced externally: ref count = "
        << reference_count;
    event_sink_->Release();
  }

  // Override IEEventListener methods.
  MOCK_METHOD7(OnBeforeNavigate2, void (IDispatch* dispatch,  // NOLINT
                                        VARIANT* url,
                                        VARIANT* flags,
                                        VARIANT* target_frame_name,
                                        VARIANT* post_data,
                                        VARIANT* headers,
                                        VARIANT_BOOL* cancel));
  MOCK_METHOD2(OnNavigateComplete2, void (IDispatch* dispatch,  // NOLINT
                                          VARIANT* url));
  MOCK_METHOD5(OnNewWindow3, void (IDispatch** dispatch,  // NOLINT
                                   VARIANT_BOOL* cancel,
                                   DWORD flags,
                                   BSTR url_context,
                                   BSTR url));
  MOCK_METHOD2(OnNewWindow2, void (IDispatch** dispatch,  // NOLINT
                                   VARIANT_BOOL* cancel));
  MOCK_METHOD5(OnNavigateError, void (IDispatch* dispatch,  // NOLINT
                                      VARIANT* url,
                                      VARIANT* frame_name,
                                      VARIANT* status_code,
                                      VARIANT* cancel));
  MOCK_METHOD2(OnFileDownload, void (VARIANT_BOOL active_doc,  // NOLINT
                                     VARIANT_BOOL* cancel));
  MOCK_METHOD0(OnQuit, void ());  // NOLINT
  MOCK_METHOD1(OnLoadError, void (const wchar_t* url));  // NOLINT
  MOCK_METHOD3(OnMessage, void (const wchar_t* message,  // NOLINT
                                const wchar_t* origin,
                                const wchar_t* source));
  MOCK_METHOD2(OnNewBrowserWindow, void (IDispatch* dispatch,  // NOLINT
                                         const wchar_t* url));

  // Convenience OnLoad method which is called once when a page is loaded with
  // |is_cf| set to whether the renderer is CF or not.
  MOCK_METHOD2(OnLoad, void (bool is_cf, const wchar_t* url));  // NOLINT

  // Attach |dispatch| to the event sink and begin listening to the source's
  // events.
  void Attach(IDispatch* dispatch) {
    event_sink_->set_listener(this);
    event_sink_->Attach(dispatch);
  }

  void Detach() {
    event_sink_->set_listener(NULL);
    event_sink_->Uninitialize();
  }

  // Expect a normal navigation to |url| to occur in CF or IE.
  void ExpectNavigation(bool is_cf, const std::wstring& url);

  // Similar as above, but should be used in cases where IE6 with CF may not
  // generate an OnBeforeNavigate event during the navigation. This occurs
  // during in-page navigation (via anchors), form submission, window.open
  // to target "self", and possibly others.
  void ExpectNavigationOptionalBefore(bool is_cf, const std::wstring& url);

  // Expect a navigation in a new window created by a window.open call to |url|.
  // |parent_cf| signifies whether the parent frame was loaded in CF, while
  // |new_window_cf| signifies whether to expect the new page to be loaded in
  // CF.
  void ExpectJavascriptWindowOpenNavigation(bool parent_cf, bool new_window_cf,
                                            const std::wstring& url);

  // Expect a new window to open. The new event sink will be attached to
  // |new_window_mock|.
  void ExpectNewWindow(MockIEEventSink* new_window_mock);

  // Expects any and all navigations.
  void ExpectAnyNavigations();

  void ExpectDocumentReadystate(int ready_state);

  IEEventSink* event_sink() { return event_sink_; }

 private:
  // Override IE's OnDocumentComplete to call our OnLoad, iff it is IE actually
  // rendering the page.
  virtual void OnDocumentComplete(IDispatch* dispatch, VARIANT* url);

  // Override CF's OnLoad to call our OnLoad.
  virtual void OnLoad(const wchar_t* url) {
    OnLoad(IN_CF, url);
  }

  // Helper method for expecting navigations. |before_cardinality| specifies
  // the cardinality for the BeforeNavigate expectation and
  // |complete_cardinality| specifies the cardinality for the NavigateComplete
  // expectation. Returns the set of expectations added.
  // Note: Prefer making a new Expect... method before making this public.
  testing::ExpectationSet ExpectNavigationCardinality(const std::wstring& url,
      testing::Cardinality before_cardinality,
      testing::Cardinality complete_cardinality);

  // It may be necessary to create this on the heap. Otherwise, if the
  // reference count is greater than zero, a debug assert will be triggered
  // in the destructor. This happens at least when IE crashes. In that case,
  // DispEventUnadvise and CoDisconnectObject are not sufficient to decrement
  // the reference count.
  // TODO(kkania): Investigate if the above is true.
  CComObject<IEEventSink>* event_sink_;
};

// This mocks a PropertyNotifySinkListener, providing methods for
// expecting certain sequences of events.
class MockPropertyNotifySinkListener : public PropertyNotifySinkListener {
 public:
  MockPropertyNotifySinkListener() : cookie_(0), sink_(NULL) {
    CComObject<PropertyNotifySinkImpl>::CreateInstance(&sink_);
    sink_->AddRef();
    sink_->set_listener(this);
  }

  ~MockPropertyNotifySinkListener() {
    Detach();
    sink_->set_listener(NULL);
    LOG_IF(ERROR, sink_->m_dwRef != 1)
        << "Event sink is still referenced externally: ref count = "
        << sink_->m_dwRef;
    sink_->Release();
  }

  // Override PropertyNotifySinkListener methods.
  MOCK_METHOD1(OnChanged, void (DISPID dispid));  // NOLINT

  bool Attach(IUnknown* obj) {
    DCHECK_EQ(cookie_, 0UL);
    DCHECK(obj);
    HRESULT hr = AtlAdvise(obj, sink_->GetUnknown(), IID_IPropertyNotifySink,
                           &cookie_);
    if (SUCCEEDED(hr)) {
      event_source_ = obj;
    } else {
      LOG(ERROR) << base::StringPrintf("AtlAdvise: 0x%08X", hr);
      cookie_ = 0;
    }

    return SUCCEEDED(hr);
  }

  void Detach() {
    if (event_source_) {
      DCHECK_NE(cookie_, 0UL);
      AtlUnadvise(event_source_, IID_IPropertyNotifySink, cookie_);
      event_source_.Release();
      cookie_ = 0;
    }
  }

 private:
  CComObject<PropertyNotifySinkImpl>* sink_;
  DWORD cookie_;
  base::win::ScopedComPtr<IUnknown> event_source_;
};

// Allows tests to observe when processes exit.
class MockObjectWatcherDelegate : public base::win::ObjectWatcher::Delegate {
 public:
  // base::win::ObjectWatcher::Delegate implementation
  MOCK_METHOD1(OnObjectSignaled, void (HANDLE process_handle));  // NOLINT

  virtual ~MockObjectWatcherDelegate() {
    // Would be nice to free them when OnObjectSignaled is called, too, but
    // it doesn't seem worth it.
    for (std::vector<HANDLE>::iterator it = process_handles_.begin();
         it != process_handles_.end(); ++it) {
      ::CloseHandle(*it);
    }
  }

  // Registers this instance to watch |process_handle| for termination.
  void WatchProcess(HANDLE process_handle) {
    process_handles_.push_back(process_handle);
    object_watcher_.StartWatching(process_handle, this);
  }

  // Registers this instance to watch |hwnd|'s owning process for termination.
  void WatchProcessForHwnd(HWND hwnd) {
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    EXPECT_TRUE(pid);
    if (pid != 0) {
      HANDLE process_handle = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
      EXPECT_TRUE(process_handle);
      if (process_handle != NULL) {
        WatchProcess(process_handle);
      }
    }
  }

 private:
  std::vector<HANDLE> process_handles_;
  base::win::ObjectWatcher object_watcher_;
};

// Mocks a window observer so that tests can detect new windows.
class MockWindowObserver : public WindowObserver {
 public:
  // WindowObserver implementation
  MOCK_METHOD1(OnWindowOpen, void (HWND hwnd));  // NOLINT
  MOCK_METHOD1(OnWindowClose, void (HWND hwnd));  // NOLINT

  void WatchWindow(std::string caption_pattern, std::string class_pattern) {
    window_watcher_.AddObserver(this, caption_pattern, class_pattern);
  }

 private:
  WindowWatchdog window_watcher_;
};

class MockAccEventObserver : public AccEventObserver {
 public:
  MOCK_METHOD1(OnAccDocLoad, void (HWND));  // NOLINT
  MOCK_METHOD3(OnAccValueChange, void (HWND, AccObject*,  // NOLINT
                                       const std::wstring&));
  MOCK_METHOD1(OnMenuPopup, void (HWND));  // NOLINT
  MOCK_METHOD2(OnTextCaretMoved, void (HWND, AccObject*));  // NOLINT
};

// This test fixture provides common methods needed for testing CF
// integration with IE. gMock is used to verify that IE is reporting correct
// navigational events and MockWebServer is used to verify that the correct
// requests are going out.
class MockIEEventSinkTest {
 public:
  MockIEEventSinkTest();
  MockIEEventSinkTest(int port, const std::wstring& address,
                      const base::FilePath& root_dir);

  ~MockIEEventSinkTest() {
    // Detach manually here so that it occurs before |last_resort_close_ie_|
    // is destroyed.
    ie_mock_.Detach();
  }

  // Launches IE as a COM server and sets |ie_mock_| as the event sink, then
  // navigates to the given url. Then the timed message loop is run until
  // |ie_mock_| receives OnQuit or the timeout is exceeded.
  void LaunchIEAndNavigate(const std::wstring& url);

  // Same as above but allows the timeout to be specified.
  void LaunchIENavigateAndLoop(const std::wstring& url,
                               base::TimeDelta timeout);

  // Returns the url for the test file given. |relative_path| should be
  // relative to the test data directory.
  std::wstring GetTestUrl(const std::wstring& relative_path);

  // Returns the absolute FilePath for the test file given. |relative_path|
  // should be relative to the test data directory.
  base::FilePath GetTestFilePath(const std::wstring& relative_path);

  // Returns the url for an html page just containing some text. Iff |use_cf|
  // is true, the chrome_frame meta tag will be included too.
  std::wstring GetSimplePageUrl() {
    return GetTestUrl(L"simple.html");
  }

  // Returns the title of the html page at |GetSimplePageUrl()|.
  std::wstring GetSimplePageTitle() {
    return L"simple web page";
  }

  // Returns the url for an html page just containing one link to the simple
  // page mentioned above.
  std::wstring GetLinkPageUrl() {
    return GetTestUrl(L"link.html");
  }

  // Returns the title of the html page at |GetLinkPageUrl()|.
  std::wstring GetLinkPageTitle() {
    return L"link";
  }

  // Returns the url for an html page containing several anchors pointing
  // to different parts of the page. |index| specifies what fragment to
  // append to the url. If zero, no fragment is appended. The highest fragment
  // is #a4.
  std::wstring GetAnchorPageUrl(int index) {
    DCHECK_LT(index, 5);
    std::wstring base_name = L"anchor.html";
    if (index > 0)
      base_name += std::wstring(L"#a") + base::IntToString16(index);
    return GetTestUrl(base_name);
  }

  // Returns the title of the html page at |GetAnchorPageUrl()|.
  std::wstring GetAnchorPageTitle() {
    return L"Chrome Frame Test";
  }

  // Returns the url for an html page that will, when clicked, open a new window
  // to |target|.
  std::wstring GetWindowOpenUrl(const wchar_t* target) {
    return GetTestUrl(std::wstring(L"window_open.html?").append(target));
  }

  // Returns the title of the html page at |GetWindowOpenUrl()|.
  std::wstring GetWindowOpenTitle() {
    return L"window open";
  }

 protected:
  CloseIeAtEndOfScope last_resort_close_ie_;
  chrome_frame_test::TimedMsgLoop loop_;
  testing::StrictMock<MockIEEventSink> ie_mock_;
  testing::StrictMock<MockWebServer> server_mock_;
  scoped_refptr<HungCOMCallDetector> hung_call_detector_;
 private:
  DISALLOW_COPY_AND_ASSIGN(MockIEEventSinkTest);
};

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_TEST_H_
