// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the MetricsService stat recording to make sure that the numbers are
// what we expect.

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "webkit/glue/window_open_disposition.h"

class MetricsServiceTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Enable the metrics service for testing (in recording-only mode).
    command_line->AppendSwitch(switches::kMetricsRecordingOnly);
  }

  // Open a couple of tabs of random content.
  void OpenTabs() {
    const int kBrowserTestFlags =
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION;

    FilePath test_directory;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_directory));

    FilePath page1_path = test_directory.AppendASCII("title2.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        net::FilePathToFileURL(page1_path),
        NEW_FOREGROUND_TAB,
        kBrowserTestFlags);

    FilePath page2_path = test_directory.AppendASCII("iframe.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        net::FilePathToFileURL(page2_path),
        NEW_FOREGROUND_TAB,
        kBrowserTestFlags);
  }
};

IN_PROC_BROWSER_TEST_F(MetricsServiceTest, CloseRenderersNormally) {
  OpenTabs();

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(prefs::kStabilityLaunchCount));
#if defined(USE_VIRTUAL_KEYBOARD)
  // The keyboard page loads.
  EXPECT_EQ(4, prefs->GetInteger(prefs::kStabilityPageLoadCount));
#else
  EXPECT_EQ(3, prefs->GetInteger(prefs::kStabilityPageLoadCount));
#endif
  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilityRendererCrashCount));
  // TODO(isherman): We should also verify that prefs::kStabilityExitedCleanly
  // is set to true, but this preference isn't set until the browser
  // exits... it's not clear to me how to test that.
}

IN_PROC_BROWSER_TEST_F(MetricsServiceTest, CrashRenderers) {
  OpenTabs();

  // Kill the process for one of the tabs.
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUICrashURL));
  observer.Wait();

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(prefs::kStabilityLaunchCount));
#if defined(USE_VIRTUAL_KEYBOARD)
  // The keyboard page loads.
  EXPECT_EQ(5, prefs->GetInteger(prefs::kStabilityPageLoadCount));
#else
  EXPECT_EQ(4, prefs->GetInteger(prefs::kStabilityPageLoadCount));
#endif
  EXPECT_EQ(1, prefs->GetInteger(prefs::kStabilityRendererCrashCount));
  // TODO(isherman): We should also verify that prefs::kStabilityExitedCleanly
  // is set to true, but this preference isn't set until the browser
  // exits... it's not clear to me how to test that.
}
