// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mshtmcid.h>
#include <string>

#include "base/test/test_file_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "chrome/common/url_constants.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/chrome_frame_ui_test_utils.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"

#include "testing/gmock_mutant.h"

using testing::_;
using testing::InSequence;
using testing::StrCaseEq;
using testing::StrEq;

namespace chrome_frame_test {

// This parameterized test fixture uses the MockIEEventSink and is used by
// UI-related tests.
class FullTabUITest : public MockIEEventSinkTest,
                      public testing::TestWithParam<CFInvocation> {
 public:
  FullTabUITest() {}

  virtual void SetUp() {
    // These are UI-related tests, so we do not care about the exact requests
    // and navigations that occur.
    server_mock_.ExpectAndServeAnyRequests(GetParam());
    ie_mock_.ExpectAnyNavigations();
  }
};

// Instantiate each test case for the IE case and for CF meta tag case.
// It does not seem too useful to also run the CF http header case since these
// are UI, not navigation tests.
INSTANTIATE_TEST_CASE_P(IE, FullTabUITest,
                        testing::Values(CFInvocation::None()));
INSTANTIATE_TEST_CASE_P(CF, FullTabUITest,
                        testing::Values(CFInvocation::MetaTag()));

// Tests keyboard input.
TEST_P(FullTabUITest, KeyboardInput) {
  if (!GetParam().invokes_cf()) {
    LOG(ERROR) << "Test not implemented for this configuration.";
    return;
  }
  std::wstring key_event_url = GetTestUrl(L"keyevent.html");

  const char* input = "Chrome";
  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(), StrEq(key_event_url)))
      .WillOnce(PostCharMessagesToRenderer(&ie_mock_, input));

  EXPECT_CALL(ie_mock_, OnMessage(StrCaseEq(UTF8ToWide(input)), _, _))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(key_event_url);
}

// Tests keyboard shortcuts for back and forward.
TEST_P(FullTabUITest, KeyboardBackForward) {
  if (IsWorkstationLocked()) {
    LOG(ERROR) << "This test cannot be run in a locked workstation.";
    return;
  }

  std::wstring page1 = GetSimplePageUrl();
  std::wstring page2 = GetLinkPageUrl();
  bool in_cf = GetParam().invokes_cf();
  InSequence expect_in_sequence_for_scope;

  // This test performs the following steps.
  // 1. Launches IE and navigates to page1
  // 2. It then navigates to page2
  // 3. Sends the VK_BACK keystroke to IE, which should navigate back to
  //    page 1
  // 4. Sends the Shift + VK_BACK keystroke to IE which should navigate
  //    forward to page2
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page1)))
      .WillOnce(Navigate(&ie_mock_, page2));

  short bkspace = VkKeyScanA(VK_BACK);  // NOLINT
  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page2)))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendScanCode(&loop_, 1000, bkspace, simulate_input::NONE)));

  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page1)))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendScanCode(&loop_, 1000, bkspace, simulate_input::SHIFT)));

  EXPECT_CALL(ie_mock_, OnLoad(in_cf, StrEq(page2)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIENavigateAndLoop(page1,
                          kChromeFrameLongNavigationTimeoutInSeconds * 2);
}

// Tests new window behavior with ctrl+N.
TEST_P(FullTabUITest, CtrlN) {
  if (IsWorkstationLocked()) {
    LOG(ERROR) << "This test cannot be run in a locked workstation.";
    return;
  }

  bool is_cf = GetParam().invokes_cf();
  if (!is_cf) {
    LOG(ERROR) << "Test not implemented for this configuration.";
    return;
  }
  // Ideally we want to use a ie_mock_ to watch for finer grained
  // events for New Window, but for Crl+N we don't get any
  // OnNewWindowX notifications. :(
  MockWindowObserver win_observer_mock;

  const char* kNewWindowTitlePattern = "*Internet Explorer*";
  EXPECT_CALL(ie_mock_, OnLoad(is_cf, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          WatchWindow(&win_observer_mock, kNewWindowTitlePattern, ""),
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 1000, 'n', simulate_input::CONTROL)));

  // Watch for new window. It appears that the window close message cannot be
  // reliably delivered immediately upon receipt of the window open event.
  EXPECT_CALL(win_observer_mock, OnWindowOpen(_))
      .Times(testing::AtMost(2))
      .WillOnce(DelayDoCloseWindow(500))
      .WillOnce(testing::Return());

  EXPECT_CALL(win_observer_mock, OnWindowClose(_))
      .Times(testing::AtMost(2))
      .WillOnce(CloseBrowserMock(&ie_mock_))
      .WillOnce(testing::Return());

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test that Ctrl+F opens the Find dialog.
TEST_P(FullTabUITest, CtrlF) {
  if (IsWorkstationLocked()) {
    LOG(ERROR) << "This test cannot be run in a locked workstation.";
    return;
  }

  bool is_cf = GetParam().invokes_cf();
  if (!is_cf) {
    LOG(ERROR) << "Test not implemented for this configuration.";
    return;
  }
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockWindowObserver win_observer_mock;
  InSequence expect_in_sequence_for_scope;

  const char* kFindDialogCaption = "Find";
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          WatchWindow(&win_observer_mock, kFindDialogCaption, ""),
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 1500, 'f', simulate_input::CONTROL)));

  // Watch for find dialog. It appears that the window close message cannot be
  // reliably delivered immediately upon receipt of the window open event.
  EXPECT_CALL(win_observer_mock, OnWindowOpen(_))
      .WillOnce(DelayDoCloseWindow(500));

  EXPECT_CALL(win_observer_mock, OnWindowClose(_))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test that ctrl+r does cause a refresh.
TEST_P(FullTabUITest, CtrlR) {
  if (IsWorkstationLocked()) {
    LOG(ERROR) << "This test cannot be run in a locked workstation.";
    return;
  }

  EXPECT_CALL(server_mock_, Get(_, UrlPathEq(GetSimplePageUrl()), _))
      .Times(testing::AtMost(2))
      .WillRepeatedly(SendResponse(&server_mock_, GetParam()));

  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(),
                               StrEq(GetSimplePageUrl())))
      .Times(testing::AtMost(2))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 1000, 'r', simulate_input::CONTROL),
          DelayCloseBrowserMock(&loop_, 4000, &ie_mock_)))
      .WillRepeatedly(testing::Return());

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test window close with ctrl+w.
TEST_P(FullTabUITest, CtrlW) {
  if (IsWorkstationLocked()) {
    LOG(ERROR) << "This test cannot be run in a locked workstation.";
    return;
  }

  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(),
                               StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 1000, 'w', simulate_input::CONTROL)));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test address bar navigation with Alt+d and URL.
TEST_P(FullTabUITest, AltD) {
  if (IsWorkstationLocked()) {
    LOG(ERROR) << "This test cannot be run in a locked workstation.";
    return;
  }

  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(),
                               StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          TypeUrlInAddressBar(&loop_, GetLinkPageUrl(), 1500)));

  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(),
                               StrEq(GetLinkPageUrl())))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Tests that the renderer has focus after navigation.
TEST_P(FullTabUITest, RendererHasFocus) {
  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(),
                               StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          ExpectRendererHasFocus(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Tests that view source works.
TEST_P(FullTabUITest, ViewSource) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }

  bool in_cf = GetParam().invokes_cf();
  if (!in_cf) {
    LOG(ERROR) << "Test not implemented for this configuration.";
    return;
  }
  MockIEEventSink view_source_mock;
  view_source_mock.ExpectAnyNavigations();
  InSequence expect_in_sequence_for_scope;

  // After navigation invoke view soruce action using IWebBrowser2::ExecWB
  VARIANT empty = base::win::ScopedVariant::kEmptyVariant;
  EXPECT_CALL(ie_mock_, OnLoad(in_cf,
                               StrEq(GetSimplePageUrl())))
      .WillOnce(DelayExecCommand(&ie_mock_, &loop_, 0, &CGID_MSHTML,
                                 static_cast<OLECMDID>(IDM_VIEWSOURCE),
                                 OLECMDEXECOPT_DONTPROMPTUSER, &empty, &empty));

  // Expect notification for view-source window, handle new window event
  // and attach a new ie_mock_ to the received web browser
  std::wstring view_source_url;
  view_source_url += UTF8ToWide(chrome::kViewSourceScheme);
  view_source_url += L":";
  view_source_url += GetSimplePageUrl();
  std::wstring url_in_new_window = kChromeProtocolPrefix;
  url_in_new_window += view_source_url;

  ie_mock_.ExpectNewWindow(&view_source_mock);
  // For some reason this happens occasionally at least on XP IE7.
  EXPECT_CALL(view_source_mock, OnLoad(IN_IE, StrEq(url_in_new_window)))
      .Times(testing::AtMost(1));
  EXPECT_CALL(view_source_mock, OnLoad(in_cf, StrEq(view_source_url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrlWithGcf(&view_source_mock),
          CloseBrowserMock(&view_source_mock)));

  EXPECT_CALL(view_source_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

void NavigateToCurrentUrl(MockIEEventSink* mock) {
  IWebBrowser2* browser = mock->event_sink()->web_browser2();
  DCHECK(browser);
  base::win::ScopedBstr bstr;
  HRESULT hr = browser->get_LocationURL(bstr.Receive());
  EXPECT_HRESULT_SUCCEEDED(hr);
  if (SUCCEEDED(hr)) {
    DCHECK(bstr.Length());
    VARIANT empty = base::win::ScopedVariant::kEmptyVariant;
    hr = browser->Navigate(bstr, &empty, &empty, &empty, &empty);
    EXPECT_HRESULT_SUCCEEDED(hr);
  }
}

// Tests that Chrome gets re-instantiated after crash if we reload via
// the address bar or via a new navigation.
TEST_P(FullTabUITest, TabCrashReload) {
  using testing::DoAll;

  if (!GetParam().invokes_cf()) {
    LOG(ERROR) << "Test needs CF.";
    return;
  }

  MockPropertyNotifySinkListener prop_listener;
  InSequence expect_in_sequence_for_scope;

  EXPECT_CALL(ie_mock_, OnLoad(_, StrEq(GetSimplePageUrl())))
      .WillOnce(DoAll(
          ExpectRendererHasFocus(&ie_mock_),
          ExpectDocumentReadystate(&ie_mock_, READYSTATE_COMPLETE),
          ConnectDocPropNotifySink(&ie_mock_, &prop_listener),
          KillChromeFrameProcesses()));

  EXPECT_CALL(prop_listener, OnChanged(DISPID_READYSTATE))
      .WillOnce(DoAll(
          ExpectDocumentReadystate(&ie_mock_, READYSTATE_UNINITIALIZED),
          DelayNavigateToCurrentUrl(&ie_mock_, &loop_, 10)));

  EXPECT_CALL(ie_mock_, OnLoad(_, StrEq(GetSimplePageUrl())))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Tests if Chrome gets restarted after a crash by just refreshing the document.
TEST_P(FullTabUITest, TabCrashRefresh) {
  using testing::DoAll;

  if (!GetParam().invokes_cf()) {
    LOG(ERROR) << "Test needs CF.";
    return;
  }

  MockPropertyNotifySinkListener prop_listener;
  InSequence expect_in_sequence_for_scope;

  EXPECT_CALL(ie_mock_, OnLoad(_, StrEq(GetSimplePageUrl())))
      .WillOnce(DoAll(
          ExpectRendererHasFocus(&ie_mock_),
          ExpectDocumentReadystate(&ie_mock_, READYSTATE_COMPLETE),
          ConnectDocPropNotifySink(&ie_mock_, &prop_listener),
          KillChromeFrameProcesses()));

  VARIANT empty = base::win::ScopedVariant::kEmptyVariant;
  EXPECT_CALL(prop_listener, OnChanged(/*DISPID_READYSTATE*/_))
      .WillOnce(DoAll(
          DisconnectDocPropNotifySink(&prop_listener),
          ExpectDocumentReadystate(&ie_mock_, READYSTATE_UNINITIALIZED),
          DelayExecCommand(&ie_mock_, &loop_, 10, static_cast<GUID*>(NULL),
              OLECMDID_REFRESH, 0, &empty, &empty)));

  EXPECT_CALL(ie_mock_, OnLoad(_, StrEq(GetSimplePageUrl())))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test fixture for tests related to the context menu UI. Since the context
// menus for CF and IE are different, these tests are not parameterized.
class ContextMenuTest : public MockIEEventSinkTest, public testing::Test {
 public:
  ContextMenuTest(): kTextFieldInitValue(L"SomeInitializedTextValue") {}

  virtual void SetUp() {
    context_menu_page_url = GetTestUrl(L"context_menu.html");
    // Clear clipboard to make sure there is no effect from previous tests.
    SetClipboardText(L"");
    // These are UI-related tests, so we do not care about the exact
    // navigations that occur.
    ie_mock_.ExpectAnyNavigations();
    EXPECT_CALL(ie_mock_, OnLoad(_, _)).Times(testing::AnyNumber());
    EXPECT_CALL(acc_observer_, OnAccDocLoad(_)).Times(testing::AnyNumber());
  }

  // Common helper function for "Save xxx As" tests.
  void DoSaveAsTest(const wchar_t* role, const wchar_t* menu_item_name,
                    const wchar_t* file_ext) {
    server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
    MockWindowObserver win_observer_mock;
    InSequence expect_in_sequence_for_scope;

    // Open 'Save As' dialog.
    const char* kSaveDlgCaption = "Save As";
    EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
        .WillOnce(testing::DoAll(
            WatchWindow(&win_observer_mock, kSaveDlgCaption, ""),
            AccRightClick(AccObjectMatcher(L"", role))));
    EXPECT_CALL(acc_observer_, OnMenuPopup(_))
        .WillOnce(AccLeftClick(AccObjectMatcher(menu_item_name)));

    // Get safe download name using temporary file.
    FilePath temp_file_path;
    ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
    ASSERT_TRUE(file_util::DieFileDie(temp_file_path, false));
    temp_file_path = temp_file_path.ReplaceExtension(file_ext);

    AccObjectMatcher file_name_box(L"File name:", L"editable text");
    EXPECT_CALL(win_observer_mock, OnWindowOpen(_))
        .WillOnce(testing::DoAll(
            AccSendCharMessage(file_name_box, L'a'),
            AccSetValue(file_name_box, temp_file_path.value()),
            AccDoDefaultAction(AccObjectMatcher(L"Save", L"push button"))));

    EXPECT_CALL(win_observer_mock, OnWindowClose(_))
        .WillOnce(CloseWhenFileSaved(&ie_mock_, temp_file_path, 5000));

    LaunchIEAndNavigate(GetTestUrl(L"save_as_context_menu.html"));
    ASSERT_TRUE(file_util::DieFileDie(temp_file_path, false));
  }

 protected:
  // Html page that holds a text field for context menu testing.
  std::wstring context_menu_page_url;
  // This is the text value used to test cut/copy/paste etc.
  const std::wstring kTextFieldInitValue;

  testing::NiceMock<MockAccEventObserver> acc_observer_;
};

// Test reloading from the context menu.
TEST_F(ContextMenuTest, CFReload) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  InSequence expect_in_sequence_for_scope;

  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(OpenContextMenuAsync());
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Reload")));

  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test view source from the context menu.
TEST_F(ContextMenuTest, CFViewSource) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockIEEventSink view_source_mock;
  view_source_mock.ExpectAnyNavigations();
  InSequence expect_in_sequence_for_scope;

  // View the page source.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(OpenContextMenuAsync());
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"View page source")));

  // Expect notification for view-source window, handle new window event
  // and attach a new ie_mock_ to the received web browser
  std::wstring view_source_url;
  view_source_url += UTF8ToWide(chrome::kViewSourceScheme);
  view_source_url += L":";
  view_source_url += GetSimplePageUrl();
  std::wstring url_in_new_window = kChromeProtocolPrefix;
  url_in_new_window += view_source_url;

  ie_mock_.ExpectNewWindow(&view_source_mock);
  // For some reason this happens occasionally at least on XP IE7 and Win7 IE8.
  EXPECT_CALL(view_source_mock, OnLoad(IN_IE, StrEq(url_in_new_window)))
      .Times(testing::AtMost(1));
  EXPECT_CALL(view_source_mock, OnLoad(IN_CF, StrEq(view_source_url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrlWithGcf(&view_source_mock),
          CloseBrowserMock(&view_source_mock)));
  EXPECT_CALL(view_source_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

TEST_F(ContextMenuTest, CFPageInfo) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockWindowObserver win_observer_mock;
  InSequence expect_in_sequence_for_scope;

  // View page information.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          WatchWindow(&win_observer_mock, "", "Chrome_WidgetWin_*"),
          OpenContextMenuAsync()));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"View page info")));

  EXPECT_CALL(win_observer_mock, OnWindowOpen(_)).Times(1);
  // Expect page info dialog to pop up. Dismiss the dialog with 'Esc' key
  EXPECT_CALL(win_observer_mock, OnWindowOpen(_))
      .WillOnce(DoCloseWindow());

  EXPECT_CALL(win_observer_mock, OnWindowClose(_)).Times(1);
  EXPECT_CALL(win_observer_mock, OnWindowClose(_))
    .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

TEST_F(ContextMenuTest, CFInspector) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockWindowObserver win_observer_mock;
  InSequence expect_in_sequence_for_scope;

  // Open developer tools.
  // Devtools begins life with "Untitled" caption and it changes
  // later to the 'Developer Tools - <url> form.
  const char* kPageInfoCaptionPattern = "Untitled*";
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          WatchWindow(&win_observer_mock, kPageInfoCaptionPattern, ""),
          OpenContextMenuAsync()));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Inspect element")));

  EXPECT_CALL(win_observer_mock, OnWindowOpen(_))
      .WillOnce(DelayDoCloseWindow(5000));  // wait to catch possible crash
  EXPECT_CALL(win_observer_mock, OnWindowClose(_))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIENavigateAndLoop(GetSimplePageUrl(),
                          kChromeFrameLongNavigationTimeoutInSeconds * 2);
}

TEST_F(ContextMenuTest, CFSavePageAs) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(DoSaveAsTest(L"", L"Save as...", L".html"));
}

TEST_F(ContextMenuTest, CFSaveLinkAs) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(DoSaveAsTest(L"link", L"Save link as...", L".zip"));
}

// This tests that the about:version page can be opened via the CF context menu.
TEST_F(ContextMenuTest, CFAboutVersionLoads) {
  // Please see http://code.google.com/p/chromium/issues/detail?id=60987
  // for more information on why this test is disabled for Vista with IE7.
  if (base::win::GetVersion() == base::win::VERSION_VISTA &&
      GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test on Vista with IE7";
    return;
  }
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  const wchar_t* kAboutVersionUrl = L"gcf:about:version";
  const wchar_t* kAboutVersionWithoutProtoUrl = L"about:version";
  MockIEEventSink new_window_mock;
  new_window_mock.ExpectAnyNavigations();
  InSequence expect_in_sequence_for_scope;

  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(OpenContextMenuAsync());
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"About*")));

  ie_mock_.ExpectNewWindow(&new_window_mock);
  // For some reason this happens occasionally at least on Win7 IE8.
  EXPECT_CALL(new_window_mock, OnLoad(IN_IE, StrEq(kAboutVersionUrl)))
      .Times(testing::AtMost(1));
  EXPECT_CALL(new_window_mock,
              OnLoad(IN_CF, StrEq(kAboutVersionWithoutProtoUrl)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrlWithGcf(&new_window_mock),
          CloseBrowserMock(&new_window_mock)));

  EXPECT_CALL(new_window_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

TEST_F(ContextMenuTest, IEOpen) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::None());
  InSequence expect_in_sequence_for_scope;

  // Open the link throught the context menu.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(AccRightClick(AccObjectMatcher(L"", L"link")));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Open")));

  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetLinkPageUrl());
}

TEST_F(ContextMenuTest, IEOpenInNewWindow) {
  // See crbug.com/64794.
  if (GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test with IE7";
    return;
  }
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::None());
  MockIEEventSink new_window_mock;
  new_window_mock.ExpectAnyNavigations();
  InSequence expect_in_sequence_for_scope;

  // Open the link in a new window.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(AccRightClick(AccObjectMatcher(L"", L"link")));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Open in New Window")));

  ie_mock_.ExpectNewWindow(&new_window_mock);
  EXPECT_CALL(new_window_mock, OnLoad(IN_IE, StrEq(GetSimplePageUrl())))
      // TODO(kkania): Verifying the address bar is flaky with this, at least
      // on XP ie6. Fix.
      .WillOnce(CloseBrowserMock(&new_window_mock));

  EXPECT_CALL(new_window_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetLinkPageUrl());
}

// Test Back/Forward from context menu.
TEST_F(ContextMenuTest, IEBackForward) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::None());
  std::wstring page1 = GetLinkPageUrl();
  std::wstring page2 = GetSimplePageUrl();
  InSequence expect_in_sequence_for_scope;

  // Navigate to second page.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(Navigate(&ie_mock_, page2));

  // Go back.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          VerifyPageLoad(&ie_mock_, IN_IE, page2),
          OpenContextMenuAsync()));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Back")));

  // Go forward.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          VerifyPageLoad(&ie_mock_, IN_IE, page1),
          OpenContextMenuAsync()));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Forward")));

  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(page2)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(page1);
}

// Test CF link context menu - Open link in new window.
TEST_F(ContextMenuTest, CFOpenLinkInNewWindow) {
  // See crbug.com/64794.
  if (GetInstalledIEVersion() == IE_7) {
    LOG(INFO) << "Not running test with IE7";
    return;
  }
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockIEEventSink new_window_mock;
  new_window_mock.ExpectAnyNavigations();

  // Invoke 'Open link in new window' context menu item.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(AccRightClick(AccObjectMatcher(L"", L"link")))
      .WillOnce(testing::Return());
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Open link in new window*")));

  ie_mock_.ExpectNewWindow(&new_window_mock);
  EXPECT_CALL(new_window_mock, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(CloseBrowserMock(&new_window_mock));
  EXPECT_CALL(new_window_mock, OnQuit())
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetLinkPageUrl());
}

// Test CF link context menu - Copy link address.
TEST_F(ContextMenuTest, CFCopyLinkAddress) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());

  // Invoke 'Copy link address' context menu item.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(AccRightClick(AccObjectMatcher(L"", L"link")));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(testing::DoAll(
          AccLeftClick(AccObjectMatcher(L"Copy link address*")),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetLinkPageUrl());

  EXPECT_STREQ(GetSimplePageUrl().c_str(), GetClipboardText().c_str());
}

// Test CF text field context menu - cut.
TEST_F(ContextMenuTest, CFTxtFieldCut) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  AccObjectMatcher txtfield_matcher(L"", L"editable text");

  // Invoke "Cut" context menu item of text field.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          AccRightClick(txtfield_matcher),
          AccWatchForOneValueChange(&acc_observer_, txtfield_matcher)));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
    .WillOnce(AccLeftClick(AccObjectMatcher(L"Cut*")));

  // Verify that text field is empty after cut operation.
  EXPECT_CALL(acc_observer_, OnAccValueChange(_, _, StrEq(L"")))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(context_menu_page_url);
  // Verify that the text value has been cut to clipboard.
  EXPECT_STREQ(kTextFieldInitValue.c_str(), GetClipboardText().c_str());
}

// Test CF text field context menu - copy.
TEST_F(ContextMenuTest, CFTxtFieldCopy) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  AccObjectMatcher txtfield_matcher(L"", L"editable text");

  // Invoke "Copy" context menu item of text field.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          AccRightClick(txtfield_matcher),
          AccWatchForOneValueChange(&acc_observer_, txtfield_matcher)));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
    .WillOnce(testing::DoAll(
        AccLeftClick(AccObjectMatcher(L"Copy*")),
        CloseBrowserMock(&ie_mock_)));

  // Verify that there is no change on text field value after copy operation.
  EXPECT_CALL(acc_observer_, OnAccValueChange(_, _, _))
      .Times(testing::AtMost(0));

  LaunchIEAndNavigate(context_menu_page_url);
  // Verify that the text value has been copied to clipboard.
  EXPECT_STREQ(kTextFieldInitValue.c_str(), GetClipboardText().c_str());
}

// Test CF text field context menu - paste.
TEST_F(ContextMenuTest, CFTxtFieldPaste) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  AccObjectMatcher txtfield_matcher(L"", L"editable text");

  // Invoke "Paste" context menu item of text field.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          AccRightClick(txtfield_matcher),
          AccWatchForOneValueChange(&acc_observer_, txtfield_matcher)));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Paste*")));
  // Verify that value has been pasted to text field.
  EXPECT_CALL(acc_observer_, OnAccValueChange(_, _, StrEq(kTextFieldInitValue)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  // Set some text value to clipboard, this is to emulate the 'copy' action.
  SetClipboardText(kTextFieldInitValue);

  LaunchIEAndNavigate(context_menu_page_url);
}

// Test CF text field context menu - delete.
TEST_F(ContextMenuTest, CFTxtFieldDelete) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  AccObjectMatcher txtfield_matcher(L"", L"editable text");

  // Invoke 'Delete' context menu item of text field.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          AccRightClick(txtfield_matcher),
          AccWatchForOneValueChange(&acc_observer_, txtfield_matcher)));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Delete*")));
  // Verify that value has been deleted from text field.
  EXPECT_CALL(acc_observer_, OnAccValueChange(_, _, StrEq(L"")))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(context_menu_page_url);
}

// Test CF text field context menu - select all.
TEST_F(ContextMenuTest, CFTxtFieldSelectAll) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());

  // Invoke 'Select all' context menu item of text field.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(AccRightClick(AccObjectMatcher(L"", L"editable text")));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(testing::DoAll(
          AccLeftClick(AccObjectMatcher(L"Select all*")),
          PostMessageToCF(&ie_mock_, L"selectall")));
  // Client side script verifies that the text field value has been selected,
  // then send 'OK' message.
  EXPECT_CALL(ie_mock_, OnMessage(testing::StrCaseEq(L"OK"), _, _))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(context_menu_page_url + L"?action=selectall");
}

// Test CF text field context menu - undo.
TEST_F(ContextMenuTest, CFTxtFieldUndo) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  AccObjectMatcher txtfield_matcher(L"", L"editable text");

  // Change the value of text field to 'A', then invoke 'Undo' context menu item
  // of text field.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          AccSendCharMessage(txtfield_matcher, L'A'),
          AccRightClick(txtfield_matcher)));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(testing::DoAll(
          AccWatchForOneValueChange(&acc_observer_, txtfield_matcher),
          AccLeftClick(AccObjectMatcher(L"Undo*"))));

  // Verify that value has been reset to initial value after undo operation.
  EXPECT_CALL(acc_observer_, OnAccValueChange(_, _, StrEq(kTextFieldInitValue)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(context_menu_page_url);
}

// Test CF text field context menu - redo.
TEST_F(ContextMenuTest, CFTxtFieldRedo) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  AccObjectMatcher txtfield_matcher(L"", L"editable text");
  InSequence expect_in_sequence_for_scope;

  // Change text field value to 'A', then undo it.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          AccSendCharMessage(txtfield_matcher, L'A'),
          AccRightClick(txtfield_matcher)));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(testing::DoAll(
          AccWatchForOneValueChange(&acc_observer_, txtfield_matcher),
          AccLeftClick(AccObjectMatcher(L"Undo*"))));

  // After undo operation is done, invoke 'Redo' context menu item.
  EXPECT_CALL(acc_observer_, OnAccValueChange(_, _, StrEq(kTextFieldInitValue)))
      .WillOnce(testing::DoAll(
          AccRightClick(txtfield_matcher),
          AccWatchForOneValueChange(&acc_observer_, txtfield_matcher)));
  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Redo*")));

  // After redo operation is done, verify that text field value is reset to its
  // changed value 'A'.
  EXPECT_CALL(acc_observer_, OnAccValueChange(_, _, StrEq(L"A")))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(context_menu_page_url);
}

TEST_F(ContextMenuTest, CFBackForward) {
  std::wstring page1 = GetLinkPageUrl();
  std::wstring page2 = GetSimplePageUrl();
  std::wstring page3 = GetTestUrl(L"anchor.html");

  server_mock_.ExpectAndServeRequestWithCardinality(
      CFInvocation::MetaTag(), page1, testing::Exactly(2));

  server_mock_.ExpectAndServeRequestWithCardinality(
      CFInvocation::None(), page2, testing::Exactly(3));

  server_mock_.ExpectAndServeRequestWithCardinality(
      CFInvocation::MetaTag(), page3, testing::Exactly(2));

  InSequence expect_in_sequence_for_scope;

  // Navigate to second page.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          VerifyPageLoad(&ie_mock_, IN_CF, page1),
          Navigate(&ie_mock_, page2)));

  // Navigate to third page.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          VerifyPageLoad(&ie_mock_, IN_IE, page2),
          Navigate(&ie_mock_, page3)));

  // Go back.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          VerifyPageLoad(&ie_mock_, IN_CF, page3),
          OpenContextMenuAsync()));

  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Back")));

  // Go back
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          VerifyPageLoad(&ie_mock_, IN_IE, page2),
          OpenContextMenuAsync()));

  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Back")));

  // Go forward.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          VerifyPageLoad(&ie_mock_, IN_CF, page1),
          OpenContextMenuAsync()));

  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Forward")));

  // Go forward.
  EXPECT_CALL(acc_observer_, OnAccDocLoad(_))
      .WillOnce(testing::DoAll(
          VerifyPageLoad(&ie_mock_, IN_IE, page2),
          OpenContextMenuAsync()));

  EXPECT_CALL(acc_observer_, OnMenuPopup(_))
      .WillOnce(AccLeftClick(AccObjectMatcher(L"Forward")));

  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(page3)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIENavigateAndLoop(page1,
                          kChromeFrameVeryLongNavigationTimeoutInSeconds);
}

}  // namespace chrome_frame_test
