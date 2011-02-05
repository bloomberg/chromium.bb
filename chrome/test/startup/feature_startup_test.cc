// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "net/base/net_util.h"
#include "ui/gfx/rect.h"

using base::TimeDelta;

namespace {

class NewTabUIStartupTest : public UIPerfTest {
 public:
  NewTabUIStartupTest() {
    show_window_ = true;
  }

  void SetUp() {}
  void TearDown() {}

  static const int kNumCycles = 5;

  void PrintTimings(const char* label, TimeDelta timings[kNumCycles],
                    bool important) {
    std::string times;
    for (int i = 0; i < kNumCycles; ++i)
      base::StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
    PrintResultList("new_tab", "", label, times, "ms", important);
  }

  void InitProfile(ProxyLauncher::ProfileType profile_type) {
    profile_type_ = profile_type;

    // Install the location of the test profile file.
    set_template_user_data(UITest::ComputeTypicalUserDataSource(
        profile_type));

    // Disable the first run notification because it has an animation which
    // masks any real performance regressions.
    launch_arguments_.AppendSwitch(switches::kDisableNewTabFirstRun);
  }

  // Run the test, by bringing up a browser and timing the new tab startup.
  // |want_warm| is true if we should output warm-disk timings, false if
  // we should report cold timings.
  void RunStartupTest(const char* label, bool want_warm, bool important,
                      ProxyLauncher::ProfileType profile_type) {
    InitProfile(profile_type);

    TimeDelta timings[kNumCycles];
    for (int i = 0; i < kNumCycles; ++i) {
      UITest::SetUp();

      // Switch to the "new tab" tab, which should be any new tab after the
      // first (the first is about:blank).
      scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
      ASSERT_TRUE(window.get());

      // We resize the window so that we hit the normal layout of the NTP and
      // not the small layout mode.
#if defined(OS_WIN)
      // TODO(port): SetBounds returns false when not implemented.
      // It is OK to comment out the resize since it will still be useful to
      // test the default size of the window.
      ASSERT_TRUE(window->GetWindow().get()->SetBounds(gfx::Rect(1000, 1000)));
#endif
      int tab_count = -1;
      ASSERT_TRUE(window->GetTabCount(&tab_count));
      ASSERT_EQ(1, tab_count);

      // Hit ctl-t and wait for the tab to load.
      ASSERT_TRUE(window->RunCommand(IDC_NEW_TAB));
      ASSERT_TRUE(window->GetTabCount(&tab_count));
      ASSERT_EQ(2, tab_count);
      int load_time;
      ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));

      if (want_warm) {
        // Bring up a second tab, now that we've already shown one tab.
        ASSERT_TRUE(window->RunCommand(IDC_NEW_TAB));
        ASSERT_TRUE(window->GetTabCount(&tab_count));
        ASSERT_EQ(3, tab_count);
        ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));
      }
      timings[i] = TimeDelta::FromMilliseconds(load_time);

      window = NULL;
      UITest::TearDown();
    }

    PrintTimings(label, timings, important);
  }

  void RunNewTabTimingTest() {
    InitProfile(ProxyLauncher::DEFAULT_THEME);

    TimeDelta scriptstart_times[kNumCycles];
    TimeDelta domcontentloaded_times[kNumCycles];
    TimeDelta onload_times[kNumCycles];

    for (int i = 0; i < kNumCycles; ++i) {
      UITest::SetUp();

      // Switch to the "new tab" tab, which should be any new tab after the
      // first (the first is about:blank).
      scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
      ASSERT_TRUE(window.get());

      // We resize the window so that we hit the normal layout of the NTP and
      // not the small layout mode.
#if defined(OS_WIN)
      // TODO(port): SetBounds returns false when not implemented.
      // It is OK to comment out the resize since it will still be useful to
      // test the default size of the window.
      ASSERT_TRUE(window->GetWindow().get()->SetBounds(gfx::Rect(1000, 1000)));
#endif
      int tab_count = -1;
      ASSERT_TRUE(window->GetTabCount(&tab_count));
      ASSERT_EQ(1, tab_count);

      // Hit ctl-t and wait for the tab to load.
      ASSERT_TRUE(window->RunCommand(IDC_NEW_TAB));
      ASSERT_TRUE(window->GetTabCount(&tab_count));
      ASSERT_EQ(2, tab_count);
      int duration;
      ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&duration));

      // Collect the timing information.
      ASSERT_TRUE(automation()->GetMetricEventDuration("Tab.NewTabScriptStart",
          &duration));
      scriptstart_times[i] = TimeDelta::FromMilliseconds(duration);

      ASSERT_TRUE(automation()->GetMetricEventDuration(
          "Tab.NewTabDOMContentLoaded", &duration));
      domcontentloaded_times[i] = TimeDelta::FromMilliseconds(duration);

      ASSERT_TRUE(automation()->GetMetricEventDuration("Tab.NewTabOnload",
          &duration));
      onload_times[i] = TimeDelta::FromMilliseconds(duration);

      window = NULL;
      UITest::TearDown();
    }

    PrintTimings("script_start", scriptstart_times, false /* important */);
    PrintTimings("domcontent_loaded", domcontentloaded_times,
                 false /* important */);
    PrintTimings("onload", onload_times, false /* important */);
  }
};

// FLAKY: http://crbug.com/69940
TEST_F(NewTabUIStartupTest, FLAKY_PerfRefCold) {
  UseReferenceBuild();
  RunStartupTest("tab_cold_ref", false /* cold */, true /* important */,
                 ProxyLauncher::DEFAULT_THEME);
}

// FLAKY: http://crbug.com/69940
TEST_F(NewTabUIStartupTest, FLAKY_PerfCold) {
  RunStartupTest("tab_cold", false /* cold */, true /* important */,
                 ProxyLauncher::DEFAULT_THEME);
}

// FLAKY: http://crbug.com/69940
TEST_F(NewTabUIStartupTest, FLAKY_PerfRefWarm) {
  UseReferenceBuild();
  RunStartupTest("tab_warm_ref", true /* warm */, true /* not important */,
                 ProxyLauncher::DEFAULT_THEME);
}

// FLAKY: http://crbug.com/69940
TEST_F(NewTabUIStartupTest, FLAKY_PerfWarm) {
  RunStartupTest("tab_warm", true /* warm */, true /* not important */,
                 ProxyLauncher::DEFAULT_THEME);
}

// FLAKY: http://crbug.com/69940
TEST_F(NewTabUIStartupTest, FLAKY_ComplexThemeCold) {
  RunStartupTest("tab_complex_theme_cold", false /* cold */,
                 false /* not important */,
                 ProxyLauncher::COMPLEX_THEME);
}

// FLAKY: http://crbug.com/69940
TEST_F(NewTabUIStartupTest, FLAKY_NewTabTimingTestsCold) {
  RunNewTabTimingTest();
}

#if defined(OS_LINUX)
TEST_F(NewTabUIStartupTest, GtkThemeCold) {
  RunStartupTest("tab_gtk_theme_cold", false /* cold */,
                 false /* not important */,
                 ProxyLauncher::NATIVE_THEME);
}

TEST_F(NewTabUIStartupTest, NativeFrameCold) {
  RunStartupTest("tab_custom_frame_cold", false /* cold */,
                 false /* not important */,
                 ProxyLauncher::CUSTOM_FRAME);
}

TEST_F(NewTabUIStartupTest, NativeFrameGtkThemeCold) {
  RunStartupTest("tab_custom_frame_gtk_theme_cold", false /* cold */,
                 false /* not important */,
                 ProxyLauncher::CUSTOM_FRAME_NATIVE_THEME);
}
#endif

}  // namespace
