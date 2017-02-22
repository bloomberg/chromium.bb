// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the MetricsService stat recording to make sure that the numbers are
// what we expect.

#include "components/metrics/metrics_service.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if defined(OS_POSIX)
#include <sys/wait.h>
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

#if defined(OS_MACOSX) || defined(OS_LINUX)
namespace {

// Check CrashExitCodes.Renderer histogram for a single bucket entry and then
// verify that the bucket entry contains a signal and the signal is |signal|.
void VerifyRendererExitCodeIsSignal(
    const base::HistogramTester& histogram_tester,
    int signal) {
  const auto buckets =
      histogram_tester.GetAllSamples("CrashExitCodes.Renderer");
  ASSERT_EQ(1UL, buckets.size());
  EXPECT_EQ(1, buckets[0].count);
  int32_t exit_code = buckets[0].min;
  EXPECT_TRUE(WIFSIGNALED(exit_code));
  EXPECT_EQ(signal, WTERMSIG(exit_code));
}

}  // namespace
#endif  // OS_MACOSX || OS_LINUX

// This test class verifies that metrics reporting works correctly for various
// renderer behaviors such as page loads, recording crashed tabs, and browser
// starts. It also verifies that if a renderer process crashes, the correct exit
// code is recorded.
//
// TODO(isherman): We should also verify that
// metrics::prefs::kStabilityExitedCleanly is set correctly after each of these
// tests, but this preference isn't set until the browser exits... it's not
// clear to me how to test that.
class MetricsServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the metrics service for testing (in recording-only mode).
    command_line->AppendSwitch(switches::kMetricsRecordingOnly);
  }

  // Open three tabs then navigate to |crashy_url| and wait for the renderer to
  // crash.
  void OpenTabsAndNavigateToCrashyUrl(const std::string& crashy_url) {
    // Opens three tabs.
    OpenThreeTabs();

    // Kill the process for one of the tabs by navigating to |crashy_url|.
    content::RenderProcessHostWatcher observer(
        browser()->tab_strip_model()->GetActiveWebContents(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    // Opens one tab.
    ui_test_utils::NavigateToURL(browser(), GURL(crashy_url));
    observer.Wait();

    // The MetricsService listens for the same notification, so the |observer|
    // might finish waiting before the MetricsService has a chance to process
    // the notification.  To avoid racing here, we repeatedly run the message
    // loop until the MetricsService catches up.  This should happen "real soon
    // now", since the notification is posted to all observers essentially
    // simultaneously... so busy waiting here shouldn't be too bad.
    const PrefService* prefs = g_browser_process->local_state();
    while (!prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount)) {
      content::RunAllPendingInMessageLoop();
    }
  }

  // Open a couple of tabs of random content.
  //
  // Calling this method causes three page load events:
  // 1. title2.html
  // 2. iframe.html
  // 3. title1.html (iframed by iframe.html)
  void OpenThreeTabs() {
    const int kBrowserTestFlags =
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION;

    base::FilePath test_directory;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_directory));

    base::FilePath page1_path = test_directory.AppendASCII("title2.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), net::FilePathToFileURL(page1_path),
        WindowOpenDisposition::NEW_FOREGROUND_TAB, kBrowserTestFlags);

    base::FilePath page2_path = test_directory.AppendASCII("iframe.html");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), net::FilePathToFileURL(page2_path),
        WindowOpenDisposition::NEW_FOREGROUND_TAB, kBrowserTestFlags);
  }
};

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, CloseRenderersNormally) {
  OpenThreeTabs();

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  EXPECT_EQ(3, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(0, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));
}

// Flaky on Linux. See http://crbug.com/131094
// Child crashes fail the process on ASan (see crbug.com/411251,
// crbug.com/368525).
#if defined(OS_LINUX) || defined(ADDRESS_SANITIZER)
#define MAYBE_CrashRenderers DISABLED_CrashRenderers
#define MAYBE_CheckCrashRenderers DISABLED_CheckCrashRenderers
#else
#define MAYBE_CrashRenderers CrashRenderers
#define MAYBE_CheckCrashRenderers CheckCrashRenderers
#endif

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, MAYBE_CrashRenderers) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(content::kChromeUICrashURL);

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  // The three tabs from OpenTabs() and the one tab to open chrome://crash/.
  EXPECT_EQ(4, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));

#if defined(OS_WIN)
  histogram_tester.ExpectUniqueSample(
      "CrashExitCodes.Renderer",
      std::abs(static_cast<int32_t>(STATUS_ACCESS_VIOLATION)), 1);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  VerifyRendererExitCodeIsSignal(histogram_tester, SIGSEGV);
#endif
  histogram_tester.ExpectUniqueSample("Tabs.SadTab.CrashCreated", 1, 1);
}

IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, MAYBE_CheckCrashRenderers) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(content::kChromeUICheckCrashURL);

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  // The three tabs from OpenTabs() and the one tab to open
  // chrome://checkcrash/.
  EXPECT_EQ(4, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));

#if defined(OS_WIN)
  histogram_tester.ExpectUniqueSample(
      "CrashExitCodes.Renderer",
      std::abs(static_cast<int32_t>(STATUS_BREAKPOINT)), 1);
#elif defined(OS_MACOSX) || defined(OS_LINUX)
  VerifyRendererExitCodeIsSignal(histogram_tester, SIGTRAP);
#endif
  histogram_tester.ExpectUniqueSample("Tabs.SadTab.CrashCreated", 1, 1);
}

// OOM code only works on Windows.
#if defined(OS_WIN) && !defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(MetricsServiceBrowserTest, OOMRenderers) {
  base::HistogramTester histogram_tester;

  OpenTabsAndNavigateToCrashyUrl(content::kChromeUIMemoryExhaustURL);

  // Verify that the expected stability metrics were recorded.
  const PrefService* prefs = g_browser_process->local_state();
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityLaunchCount));
  // The three tabs from OpenTabs() and the one tab to open
  // chrome://memory-exhaust/.
  EXPECT_EQ(4, prefs->GetInteger(metrics::prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, prefs->GetInteger(metrics::prefs::kStabilityRendererCrashCount));

// On 64-bit, the Job object should terminate the renderer on an OOM.
#if defined(ARCH_CPU_64_BITS)
  const int expected_exit_code = sandbox::SBOX_FATAL_MEMORY_EXCEEDED;
#else
  const int expected_exit_code = base::win::kOomExceptionCode;
#endif

  // Exit codes are recorded after being passed through std::abs see
  // MapCrashExitCodeForHistogram.
  histogram_tester.ExpectUniqueSample("CrashExitCodes.Renderer",
                                      std::abs(expected_exit_code), 1);

  histogram_tester.ExpectUniqueSample("Tabs.SadTab.OomCreated", 1, 1);
}
#endif  // OS_WIN && !ADDRESS_SANITIZER
