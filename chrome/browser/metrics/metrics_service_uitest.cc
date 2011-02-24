// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests the MetricsService stat recording to make sure that the numbers are
// what we expect.

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

class MetricsServiceTest : public UITest {
 public:
  MetricsServiceTest() : UITest() {
    // We need to show the window so web content type tabs load.
    show_window_ = true;
  }

  // Open a few tabs of random content
  void OpenTabs() {
    scoped_refptr<BrowserProxy> window = automation()->GetBrowserWindow(0);
    ASSERT_TRUE(window.get());

    FilePath page1_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &page1_path));
    page1_path = page1_path.AppendASCII("title2.html");
    ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(page1_path)));

    FilePath page2_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &page2_path));
    page2_path = page2_path.AppendASCII("iframe.html");
    ASSERT_TRUE(window->AppendTab(net::FilePathToFileURL(page2_path)));
  }

  // Get a PrefService whose contents correspond to the Local State file
  // that was saved by the app as it closed.  The caller takes ownership of the
  // returned PrefService object.
  PrefService* GetLocalState() {
    FilePath path = user_data_dir().Append(chrome::kLocalStateFilename);
    return PrefServiceMockBuilder().WithUserFilePrefs(path).Create();
  }
};

TEST_F(MetricsServiceTest, CloseRenderersNormally) {
  OpenTabs();
  QuitBrowser();

  scoped_ptr<PrefService> local_state(GetLocalState());
  local_state->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);
  local_state->RegisterIntegerPref(prefs::kStabilityLaunchCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kStabilityExitedCleanly));
  EXPECT_EQ(1, local_state->GetInteger(prefs::kStabilityLaunchCount));
  EXPECT_EQ(3, local_state->GetInteger(prefs::kStabilityPageLoadCount));
  EXPECT_EQ(0, local_state->GetInteger(prefs::kStabilityRendererCrashCount));
}

TEST_F(MetricsServiceTest, DISABLED_CrashRenderers) {
  // This doesn't make sense to test in single process mode.
  if (ProxyLauncher::in_process_renderer())
    return;

  OpenTabs();

  {
    // Limit the lifetime of various automation proxies used here. We must
    // destroy them before calling QuitBrowser.

    scoped_refptr<BrowserProxy> window = automation()->GetBrowserWindow(0);
    ASSERT_TRUE(window.get());

    // Kill the process for one of the tabs.
    scoped_refptr<TabProxy> tab(window->GetTab(1));
    ASSERT_TRUE(tab.get());

    // We can get crash dumps on Windows always, Linux when breakpad is
    // enabled, and all platforms for official Google Chrome builds.
#if defined(OS_WIN) || defined(USE_LINUX_BREAKPAD) || \
    defined(GOOGLE_CHROME_BUILD)
    expected_crashes_ = 1;
#endif
    ASSERT_TRUE(tab->NavigateToURLAsync(GURL(chrome::kAboutCrashURL)));
  }

  // Give the browser a chance to notice the crashed tab.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());

  QuitBrowser();

  scoped_ptr<PrefService> local_state(GetLocalState());
  local_state->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);
  local_state->RegisterIntegerPref(prefs::kStabilityLaunchCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kStabilityExitedCleanly));
  EXPECT_EQ(1, local_state->GetInteger(prefs::kStabilityLaunchCount));
  EXPECT_EQ(4, local_state->GetInteger(prefs::kStabilityPageLoadCount));
  EXPECT_EQ(1, local_state->GetInteger(prefs::kStabilityRendererCrashCount));
}
