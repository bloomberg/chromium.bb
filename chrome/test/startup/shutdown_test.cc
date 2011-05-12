// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/env_vars.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

using base::TimeDelta;

namespace {

class ShutdownTest : public UIPerfTest {
 public:
  ShutdownTest() {
    show_window_ = true;
  }
  void SetUp() {}
  void TearDown() {}

  enum TestSize {
    SIMPLE,  // Runs with no command line arguments (loads about:blank).
    TWENTY_TABS,  // Opens 5 copies of 4 different test pages.
  };

  void SetUpTwentyTabs() {
    int window_count;
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(1, window_count);
    scoped_refptr<BrowserProxy> browser_proxy(
        automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser_proxy.get());

    const FilePath kFastShutdownDir(FILE_PATH_LITERAL("fast_shutdown"));
    const FilePath kCurrentDir(FilePath::kCurrentDirectory);
    const FilePath test_cases[] = {
      ui_test_utils::GetTestFilePath(kFastShutdownDir,
          FilePath(FILE_PATH_LITERAL("on_before_unloader.html"))),
      ui_test_utils::GetTestFilePath(kCurrentDir,
          FilePath(FILE_PATH_LITERAL("animated-gifs.html"))),
      ui_test_utils::GetTestFilePath(kCurrentDir,
          FilePath(FILE_PATH_LITERAL("french_page.html"))),
      ui_test_utils::GetTestFilePath(kCurrentDir,
          FilePath(FILE_PATH_LITERAL("onunload_cookie.html"))),
    };

    for (size_t i = 0; i < arraysize(test_cases); i++) {
      ASSERT_TRUE(file_util::PathExists(test_cases[i]));
      for (size_t j = 0; j < 5; j++) {
        ASSERT_TRUE(browser_proxy->AppendTab(
            net::FilePathToFileURL(test_cases[i])));
      }
    }
  }

  void RunShutdownTest(const char* graph, const char* trace,
                       bool important, TestSize test_size,
                       ProxyLauncher::ShutdownType shutdown_type) {
    const int kNumCyclesMax = 20;
    int numCycles = kNumCyclesMax;
    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string numCyclesEnv;
    if (env->GetVar(env_vars::kStartupTestsNumCycles, &numCyclesEnv) &&
        base::StringToInt(numCyclesEnv, &numCycles)) {
      if (numCycles <= kNumCyclesMax) {
        VLOG(1) << env_vars::kStartupTestsNumCycles
                << " set in environment, so setting numCycles to " << numCycles;
      } else {
        VLOG(1) << env_vars::kStartupTestsNumCycles
                << " is higher than the max, setting numCycles to "
                << kNumCyclesMax;
        numCycles = kNumCyclesMax;
      }
    }

    TimeDelta timings[kNumCyclesMax];
    for (int i = 0; i < numCycles; ++i) {
      UITest::SetUp();
      if (test_size == TWENTY_TABS) {
        SetUpTwentyTabs();
      }
      set_shutdown_type(shutdown_type);
      UITest::TearDown();
      timings[i] = browser_quit_time();

      if (i == 0) {
        // Re-use the profile data after first run so that the noise from
        // creating databases doesn't impact all the runs.
        clear_profile_ = false;
        // Clear template_user_data_ so we don't try to copy it over each time
        // through.
        set_template_user_data(FilePath());
      }
    }

    std::string times;
    for (int i = 0; i < numCycles; ++i)
      base::StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
    PrintResultList(graph, "", trace, times, "ms", important);
  }
};

TEST_F(ShutdownTest, SimpleWindowClose) {
  RunShutdownTest("shutdown", "simple-window-close",
                  true, /* important */ SIMPLE, ProxyLauncher::WINDOW_CLOSE);
}

TEST_F(ShutdownTest, SimpleUserQuit) {
  RunShutdownTest("shutdown", "simple-user-quit",
                  true, /* important */ SIMPLE, ProxyLauncher::USER_QUIT);
}

TEST_F(ShutdownTest, SimpleSessionEnding) {
  RunShutdownTest("shutdown", "simple-session-ending",
                  true, /* important */ SIMPLE, ProxyLauncher::SESSION_ENDING);
}

TEST_F(ShutdownTest, TwentyTabsWindowClose) {
  RunShutdownTest("shutdown", "twentytabs-window-close",
                  true, /* important */ TWENTY_TABS,
                  ProxyLauncher::WINDOW_CLOSE);
}

TEST_F(ShutdownTest, TwentyTabsUserQuit) {
  RunShutdownTest("shutdown", "twentytabs-user-quit",
                  true, /* important */ TWENTY_TABS, ProxyLauncher::USER_QUIT);
}

// http://crbug.com/40671
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TwentyTabsSessionEnding DISABLED_TwentyTabsSessionEnding
#else
#define MAYBE_TwentyTabsSessionEnding TwentyTabsSessionEnding
#endif

TEST_F(ShutdownTest, MAYBE_TwentyTabsSessionEnding) {
  RunShutdownTest("shutdown", "twentytabs-session-ending",
                  true, /* important */ TWENTY_TABS,
                  ProxyLauncher::SESSION_ENDING);
}

}  // namespace
