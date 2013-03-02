// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the MetricsService stat recording to make sure that the numbers are
// what we expect.

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/metrics_service.h"
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
#include "ui/base/window_open_disposition.h"

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

    base::FilePath test_directory;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_directory));

    base::FilePath page1_path = test_directory.AppendASCII("title2.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        net::FilePathToFileURL(page1_path),
        NEW_FOREGROUND_TAB,
        kBrowserTestFlags);

    base::FilePath page2_path = test_directory.AppendASCII("iframe.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(),
        net::FilePathToFileURL(page2_path),
        NEW_FOREGROUND_TAB,
        kBrowserTestFlags);
  }
};

class MetricsServiceReportingTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Enable the metrics service for testing (in the full mode).
    command_line->AppendSwitch(switches::kEnableMetricsReportingForTesting);
  }
};

IN_PROC_BROWSER_TEST_F(MetricsServiceTest, CloseRenderersNormally) {
  OpenTabs();

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(prefs::kStabilityLaunchCount));
  EXPECT_EQ(3, prefs->GetInteger(prefs::kStabilityPageLoadCount));
  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilityRendererCrashCount));
  // TODO(isherman): We should also verify that prefs::kStabilityExitedCleanly
  // is set to true, but this preference isn't set until the browser
  // exits... it's not clear to me how to test that.
}

// Flaky on Linux. See http://crbug.com/131094
#if defined(OS_LINUX)
#define MAYBE_CrashRenderers DISABLED_CrashRenderers
#else
#define MAYBE_CrashRenderers CrashRenderers
#endif
IN_PROC_BROWSER_TEST_F(MetricsServiceTest, MAYBE_CrashRenderers) {
  OpenTabs();

  // Kill the process for one of the tabs.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), GURL(content::kChromeUICrashURL));
  observer.Wait();

  // The MetricsService listens for the same notification, so the |observer|
  // might finish waiting before the MetricsService has a chance to process the
  // notification.  To avoid racing here, we repeatedly run the message loop
  // until the MetricsService catches up.  This should happen "real soon now",
  // since the notification is posted to all observers essentially
  // simultaneously... so busy waiting here shouldn't be too bad.
  const PrefService* prefs = g_browser_process->local_state();
  while (!prefs->GetInteger(prefs::kStabilityRendererCrashCount)) {
    content::RunAllPendingInMessageLoop();
  }

  // Verify that the expected stability metrics were recorded.
  EXPECT_EQ(1, prefs->GetInteger(prefs::kStabilityLaunchCount));
  EXPECT_EQ(4, prefs->GetInteger(prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, prefs->GetInteger(prefs::kStabilityRendererCrashCount));
  // TODO(isherman): We should also verify that prefs::kStabilityExitedCleanly
  // is set to true, but this preference isn't set until the browser
  // exits... it's not clear to me how to test that.
}

IN_PROC_BROWSER_TEST_F(MetricsServiceTest, CheckLowEntropySourceUsed) {
  // Since MetricsService is only in recording mode, and is not reporting,
  // check that the low entropy source is returned at some point.
  ASSERT_TRUE(g_browser_process->metrics_service());
  EXPECT_EQ(MetricsService::LAST_ENTROPY_LOW,
            g_browser_process->metrics_service()->entropy_source_returned());
}

IN_PROC_BROWSER_TEST_F(MetricsServiceReportingTest,
                       CheckHighEntropySourceUsed) {
  // Since the full metrics service runs in this test, we expect that
  // MetricsService returns the full entropy source at some point during
  // BrowserMain startup.
  ASSERT_TRUE(g_browser_process->metrics_service());
  EXPECT_EQ(MetricsService::LAST_ENTROPY_HIGH,
            g_browser_process->metrics_service()->entropy_source_returned());
}
