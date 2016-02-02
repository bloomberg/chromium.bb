// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "build/build_config.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "net/dns/mock_host_resolver.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
// Maximizing/fullscreen popup window doesn't work on aura's managed mode.
// See bug crbug.com/116305.
// Mac: http://crbug.com/103912
#define MAYBE_UpdateWindowShowState DISABLED_UpdateWindowShowState
#else
#define MAYBE_UpdateWindowShowState UpdateWindowShowState
#endif  // defined(USE_AURA) || defined(OS_MACOSX)

// http://crbug.com/145639
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_TabEvents DISABLED_TabEvents
#else
#define MAYBE_TabEvents TabEvents
#endif

class ExtensionApiNewTabTest : public ExtensionApiTest {
 public:
  ExtensionApiNewTabTest() {}
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Override the default which InProcessBrowserTest adds if it doesn't see a
    // homepage.
    command_line->AppendSwitchASCII(
        switches::kHomePage, chrome::kChromeUINewTabURL);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionApiNewTabTest, Tabs) {
  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabAudible) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "audible.html")) << message_;
}

// http://crbug.com/521410
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabMuted) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "muted.html")) << message_;
}

// Flaky on windows: http://crbug.com/238667
#if defined(OS_WIN)
#define MAYBE_Tabs2 DISABLED_Tabs2
#else
#define MAYBE_Tabs2 Tabs2
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Tabs2) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud2.html")) << message_;
}

// crbug.com/149924
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabDuplicate) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "duplicate.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabSize) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "tab_size.html")) << message_;
}

// Flaky on linux: http://crbug.com/396364
#if defined(OS_LINUX)
#define MAYBE_TabUpdate DISABLED_TabUpdate
#else
#define MAYBE_TabUpdate TabUpdate
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabUpdate) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "update.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabPinned) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "pinned.html")) << message_;
}

// Flaky on windows: http://crbug.com/238667
#if defined(OS_WIN)
#define MAYBE_TabMove DISABLED_TabMove
#else
#define MAYBE_TabMove TabMove
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabMove) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "move.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabEvents) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "events.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabRelativeURLs) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "relative_urls.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabQuery) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "query.html")) << message_;
}

// Flaky on windows: http://crbug.com/239022
#if defined(OS_WIN)
#define MAYBE_TabHighlight DISABLED_TabHighlight
#else
#define MAYBE_TabHighlight TabHighlight
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabHighlight) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "highlight.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabCrashBrowser) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crash.html")) << message_;
}

// Flaky on windows: http://crbug.com/238667
#if defined(OS_WIN)
#define MAYBE_TabOpener DISABLED_TabOpener
#else
#define MAYBE_TabOpener TabOpener
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_TabOpener) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "opener.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabGetCurrent) {
  ASSERT_TRUE(RunExtensionTest("tabs/get_current")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabConnect) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}

// Possible race in ChromeURLDataManager. http://crbug.com/59198
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabOnRemoved) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_removed")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_TabReload) {
  ASSERT_TRUE(RunExtensionTest("tabs/reload")) << message_;
}

class ExtensionApiCaptureTest : public ExtensionApiTest {
 public:
  ExtensionApiCaptureTest() {}

  void SetUp() override {
    EnablePixelOutput();
    ExtensionApiTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest,
                       DISABLED_CaptureVisibleTabJpeg) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_jpeg.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest, DISABLED_CaptureVisibleTabPng) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_png.html")) << message_;
}

// Times out on non-Windows.
// See http://crbug.com/80212
IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest,
                       DISABLED_CaptureVisibleTabRace) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_race.html")) << message_;
}


// Disabled for being flaky, see http://crbug/367695.
IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest,
                       DISABLED_CaptureVisibleFile) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_file.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest, CaptureVisibleDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots,
                                               true);
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_disabled.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsOnCreated) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_created")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsOnUpdated) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_updated")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsNoPermissions) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("tabs/no_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, UpdateWindowResize) {
  ASSERT_TRUE(RunExtensionTest("window_update/resize")) << message_;
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FocusWindowDoesNotUnmaximize) {
  HWND window =
      browser()->window()->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  ::SendMessage(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_TRUE(::IsZoomed(window));
}
#endif  // OS_WIN

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_UpdateWindowShowState) {
  ASSERT_TRUE(RunExtensionTest("window_update/show_state")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IncognitoDisabledByPref) {
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);

  // This makes sure that creating an incognito window fails due to pref
  // (policy) being set.
  ASSERT_TRUE(RunExtensionTest("tabs/incognito_disabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_GetViewsOfCreatedPopup) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_popup.html"))
      << message_;
}
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_GetViewsOfCreatedWindow) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_window.html"))
      << message_;
}

// Adding a new test? Awesome. But API tests are the old hotness. The new
// hotness is extension_function_test_utils. See tabs_test.cc for an example.
// We are trying to phase out many uses of API tests as they tend to be flaky.
