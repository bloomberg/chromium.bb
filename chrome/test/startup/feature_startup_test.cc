// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/gfx/rect.h"
#include "base/path_service.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

using base::TimeDelta;

namespace {

class NewTabUIStartupTest : public UITest {
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
      StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
    PrintResultList("new_tab", "", label, times, "ms", important);
  }

  void InitProfile(UITest::ProfileType profile_type) {
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
                      UITest::ProfileType profile_type) {
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
      window->RunCommand(IDC_NEW_TAB);
      ASSERT_TRUE(window->GetTabCount(&tab_count));
      ASSERT_EQ(2, tab_count);
      int load_time;
      ASSERT_TRUE(automation()->WaitForInitialNewTabUILoad(&load_time));

      if (want_warm) {
        // Bring up a second tab, now that we've already shown one tab.
        window->RunCommand(IDC_NEW_TAB);
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
    InitProfile(UITest::DEFAULT_THEME);

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
      window->ApplyAccelerator(IDC_NEW_TAB);
      ASSERT_TRUE(window->WaitForTabCountToBecome(2, 5000));
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

// TODO(pamg): run these tests with a reference build?
TEST_F(NewTabUIStartupTest, PerfCold) {
  RunStartupTest("tab_cold", false /* cold */, true /* important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(NewTabUIStartupTest, PerfWarm) {
  RunStartupTest("tab_warm", true /* warm */, false /* not important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(NewTabUIStartupTest, ComplexThemeCold) {
  RunStartupTest("tab_complex_theme_cold", false /* cold */,
                 false /* not important */,
                 UITest::COMPLEX_THEME);
}

TEST_F(NewTabUIStartupTest, NewTabTimingTestsCold) {
  RunNewTabTimingTest();
}

#if defined(OS_LINUX)
TEST_F(NewTabUIStartupTest, GtkThemeCold) {
  RunStartupTest("tab_gtk_theme_cold", false /* cold */,
                 false /* not important */,
                 UITest::NATIVE_THEME);
}

TEST_F(NewTabUIStartupTest, NativeFrameCold) {
  RunStartupTest("tab_custom_frame_cold", false /* cold */,
                 false /* not important */,
                 UITest::CUSTOM_FRAME);
}

TEST_F(NewTabUIStartupTest, NativeFrameGtkThemeCold) {
  RunStartupTest("tab_custom_frame_gtk_theme_cold", false /* cold */,
                 false /* not important */,
                 UITest::CUSTOM_FRAME_NATIVE_THEME);
}
#endif

}  // namespace
