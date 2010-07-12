// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mshtmcid.h>
#include <string>

#include "chrome/common/url_constants.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"

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
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_P(FullTabUITest, FLAKY_KeyboardInput) {
  if (!GetParam().invokes_cf()) {
    LOG(ERROR) << "Test not implemented for this configuration.";
    return;
  }
  std::wstring key_event_url = GetTestUrl(L"keyevent.html");

  const wchar_t* input = L"Chrome";
  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(), StrEq(key_event_url)))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendString(&loop_, 500, input)));

  EXPECT_CALL(ie_mock_, OnMessage(StrCaseEq(input), _, _))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(key_event_url);
}

// Tests keyboard shortcuts for back and forward.
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_P(FullTabUITest, FLAKY_KeyboardBackForward) {
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
          DelaySendScanCode(&loop_, 500, bkspace, simulate_input::NONE)));

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
TEST_P(FullTabUITest, FLAKY_CtrlN) {
  bool is_cf = GetParam().invokes_cf();
  if (!is_cf) {
    LOG(ERROR) << "Test not implemented for this configuration.";
    return;
  }
  // Ideally we want to use a ie_mock_ to watch for finer grained
  // events for New Window, but for Crl+N we don't get any
  // OnNewWindowX notifications. :(
  MockWindowObserver win_observer_mock;
  const wchar_t* kIEFrameClass = L"IEFrame";
  EXPECT_CALL(ie_mock_, OnLoad(is_cf, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          WatchWindow(&win_observer_mock, kIEFrameClass),
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 1000, 'n', simulate_input::CONTROL)));

  // Watch for new window
  const char* kNewWindowTitle = "Internet Explorer";
  EXPECT_CALL(win_observer_mock,
              OnWindowDetected(_, testing::HasSubstr(kNewWindowTitle)))
      .WillOnce(testing::DoAll(
          DoCloseWindow(),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetSimplePageUrl());
  // TODO(kkania): The new window does not close properly sometimes.
}

// Test that ctrl+r does cause a refresh.
TEST_P(FullTabUITest, FLAKY_CtrlR) {
  InSequence expect_in_sequence_for_scope;

  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(),
                               StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 1000, 'r', simulate_input::CONTROL)));

  EXPECT_CALL(server_mock_, Get(_, UrlPathEq(GetSimplePageUrl()), _))
      .WillOnce(testing::DoAll(
          SendResponse(&server_mock_, GetParam()),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test window close with ctrl+w.
TEST_P(FullTabUITest, FLAKY_CtrlW) {
  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(),
                               StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelaySendChar(&loop_, 1000, 'w', simulate_input::CONTROL)));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test address bar navigation with Alt+d and URL.
TEST_P(FullTabUITest, FLAKY_AltD) {
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
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
TEST_P(FullTabUITest, FLAKY_RendererHasFocus) {
  EXPECT_CALL(ie_mock_, OnLoad(GetParam().invokes_cf(),
                               StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          ExpectRendererHasFocus(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Tests that view source works.
// This test has been marked FLAKY
// http://code.google.com/p/chromium/issues/detail?id=35370
TEST_P(FullTabUITest, FLAKY_ViewSource) {
  bool in_cf = GetParam().invokes_cf();
  if (!in_cf) {
    LOG(ERROR) << "Test not implemented for this configuration.";
    return;
  }
  MockIEEventSink view_source_mock;
  view_source_mock.ExpectAnyNavigations();
  InSequence expect_in_sequence_for_scope;

  // After navigation invoke view soruce action using IWebBrowser2::ExecWB
  VARIANT empty = ScopedVariant::kEmptyVariant;
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
  EXPECT_CALL(view_source_mock, OnLoad(false, StrEq(url_in_new_window)))
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

// Test fixture for tests related to the context menu UI. Since the context
// menus for CF and IE are different, these tests are not parameterized.
class ContextMenuTest : public MockIEEventSinkTest, public testing::Test {
 public:
  ContextMenuTest() {}

  virtual void SetUp() {
    // These are UI-related tests, so we do not care about the exact
    // navigations that occur.
    ie_mock_.ExpectAnyNavigations();
  }
};

// Test Reload from context menu.
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ContextMenuTest, FLAKY_CFReload) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  InSequence expect_in_sequence_for_scope;

  // Reload using Rt-Click + DOWN + DOWN + DOWN + ENTER
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_DOWN, 3,
                                simulate_input::NONE)));

  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

// Test view source using context menu
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ContextMenuTest, FLAKY_CFViewSource) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockIEEventSink view_source_mock;
  view_source_mock.ExpectAnyNavigations();
  InSequence expect_in_sequence_for_scope;

  // View source using Rt-Click + UP + UP + UP + UP + ENTER
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_UP, 4, simulate_input::NONE)));

  // Expect notification for view-source window, handle new window event
  // and attach a new ie_mock_ to the received web browser
  std::wstring view_source_url;
  view_source_url += UTF8ToWide(chrome::kViewSourceScheme);
  view_source_url += L":";
  view_source_url += GetSimplePageUrl();
  std::wstring url_in_new_window = kChromeProtocolPrefix;
  url_in_new_window += view_source_url;

  ie_mock_.ExpectNewWindow(&view_source_mock);
  EXPECT_CALL(view_source_mock, OnLoad(IN_CF, StrEq(view_source_url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrlWithGcf(&view_source_mock),
          CloseBrowserMock(&view_source_mock)));
  EXPECT_CALL(view_source_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

TEST_F(ContextMenuTest, FLAKY_CFPageInfo) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockWindowObserver win_observer_mock;
  InSequence expect_in_sequence_for_scope;

  // View page information using Rt-Click + UP + UP + UP + ENTER
  const wchar_t* kPageInfoWindowClass = L"Chrome_WidgetWin_0";
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          WatchWindow(&win_observer_mock, kPageInfoWindowClass),
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_UP, 3, simulate_input::NONE)));

  // Expect page info dialog to pop up. Dismiss the dialog with 'Esc' key
  const char* kPageInfoCaption = "Security Information";
  EXPECT_CALL(win_observer_mock, OnWindowDetected(_, StrEq(kPageInfoCaption)))
      .WillOnce(testing::DoAll(
          DelaySendChar(&loop_, 100, VK_ESCAPE, simulate_input::NONE),
          DelayCloseBrowserMock(&loop_, 2000, &ie_mock_)));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

TEST_F(ContextMenuTest, FLAKY_CFInspector) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockWindowObserver win_observer_mock;
  InSequence expect_in_sequence_for_scope;

  // Open developer tools using Rt-Click + UP + UP + ENTER
  const wchar_t* kPageInfoWindowClass = L"Chrome_WidgetWin_0";
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          WatchWindow(&win_observer_mock, kPageInfoWindowClass),
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_UP, 2, simulate_input::NONE)));

  // Devtools begins life with "Untitled" caption and it changes
  // later to the 'Developer Tools - <url> form.
  const char* kPageInfoCaption = "Untitled";
  EXPECT_CALL(win_observer_mock,
              OnWindowDetected(_, testing::StartsWith(kPageInfoCaption)))
      .WillOnce(testing::DoAll(
          SetFocusToRenderer(&ie_mock_),
          DelayCloseBrowserMock(&loop_, 2000, &ie_mock_)));

  LaunchIEAndNavigate(GetSimplePageUrl());
}

TEST_F(ContextMenuTest, FLAKY_CFSaveAs) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  MockWindowObserver win_observer_mock;
  InSequence expect_in_sequence_for_scope;

  // Open'Save As' dialog using Rt-Click + DOWN + DOWN + DOWN + DOWN + ENTER
  const wchar_t* kSaveDlgClass = L"#32770";
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          WatchWindow(&win_observer_mock, kSaveDlgClass),
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_DOWN, 4,
                                simulate_input::NONE)));

  FilePath temp_file_path;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  temp_file_path = temp_file_path.ReplaceExtension(L".htm");

  const wchar_t* kSaveFileName = temp_file_path.value().c_str();
  DeleteFile(kSaveFileName);

  const char* kSaveDlgCaption = "Save As";
  EXPECT_CALL(win_observer_mock, OnWindowDetected(_, StrEq(kSaveDlgCaption)))
      .WillOnce(testing::DoAll(
          DelaySendString(&loop_, 100, kSaveFileName),
          DelaySendChar(&loop_, 200, VK_RETURN, simulate_input::NONE),
          DelayCloseBrowserMock(&loop_, 4000, &ie_mock_)));

  LaunchIEAndNavigate(GetSimplePageUrl());
  ASSERT_NE(INVALID_FILE_ATTRIBUTES, GetFileAttributes(kSaveFileName));
  ASSERT_TRUE(DeleteFile(kSaveFileName));
}

// This tests that the about:version page can be opened via the CF context menu.
TEST_F(ContextMenuTest, FLAKY_CFAboutVersionLoads) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::MetaTag());
  const wchar_t* kAboutVersionUrl = L"gcf:about:version";
  const wchar_t* kAboutVersionWithoutProtoUrl = L"about:version";
  MockIEEventSink new_window_mock;
  new_window_mock.ExpectAnyNavigations();

  ie_mock_.ExpectNavigation(IN_CF, GetSimplePageUrl());
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_UP, 1, simulate_input::NONE)));

  ie_mock_.ExpectNewWindow(&new_window_mock);
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

TEST_F(ContextMenuTest, FLAKY_IEOpen) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::None());
  // Focus the renderer window by clicking and then tab once to highlight the
  // link.
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetLinkPageUrl())))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 1, 1, simulate_input::LEFT),
          DelaySendScanCode(&loop_, 1000, VK_TAB, simulate_input::NONE),
          OpenContextMenu(&loop_, 2000),
          SelectItem(&loop_, 3000, 0)));

  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetSimplePageUrl())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIEAndNavigate(GetLinkPageUrl());
}

TEST_F(ContextMenuTest, FLAKY_IEOpenInNewWindow) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::None());
  MockIEEventSink new_window_mock;
  new_window_mock.ExpectAnyNavigations();

  int open_new_window_index = 2;
  if (chrome_frame_test::GetInstalledIEVersion() == IE_6)
    open_new_window_index = 1;

  // Focus the renderer window by clicking and then tab once to highlight the
  // link.
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(GetLinkPageUrl())))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 1, 1, simulate_input::LEFT),
          DelaySendScanCode(&loop_, 500, VK_TAB, simulate_input::NONE),
          OpenContextMenu(&loop_, 1000),
          SelectItem(&loop_, 1500, open_new_window_index)));

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
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ContextMenuTest, FLAKY_IEBackForward) {
  server_mock_.ExpectAndServeAnyRequests(CFInvocation::None());
  std::wstring page1 = GetLinkPageUrl();
  std::wstring page2 = GetSimplePageUrl();
  InSequence expect_in_sequence_for_scope;

  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(page1)))
      .WillOnce(Navigate(&ie_mock_, page2));

  // Go back using Rt-Click + DOWN + ENTER
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(page2)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_DOWN, 1,
                                simulate_input::NONE)));

  // Go forward using Rt-Click + DOWN + DOWN + ENTER
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(page1)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_DOWN, 2,
                                simulate_input::NONE)));

  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(page2)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(page1);
}

// Test Back/Forward from context menu. Loads page 1 in chrome and page 2
// in IE. Then it tests back and forward using context menu
// Disabling this test as it won't work as per the current chrome external tab
// design.
// http://code.google.com/p/chromium/issues/detail?id=46615
TEST_F(ContextMenuTest, DISABLED_BackForwardWithSwitch) {
  std::wstring page1 = GetLinkPageUrl();
  std::wstring page2 = GetSimplePageUrl();
  InSequence expect_in_sequence_for_scope;

  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(page1)))
      .WillOnce(Navigate(&ie_mock_, page2));

  server_mock_.ExpectAndServeRequest(CFInvocation::None(), page2);
  // Go back using Rt-Click + DOWN + ENTER
  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(page2)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_DOWN, 1,
                                simulate_input::NONE)));

  // Go forward using Rt-Click + DOWN + DOWN + ENTER
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(page1)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&ie_mock_, &loop_, 0, 10, 10,
                              simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop_, 500, VK_DOWN, 2,
                                simulate_input::NONE)));

  EXPECT_CALL(ie_mock_, OnLoad(IN_IE, StrEq(page2)))
      .WillOnce(CloseBrowserMock(&ie_mock_));

  LaunchIEAndNavigate(page1);
}

}  // namespace chrome_frame_test