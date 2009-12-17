// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

class StartupTest : public UITest {
 public:
  StartupTest() {
    show_window_ = true;
    pages_ = "about:blank";
  }
  void SetUp() {}
  void TearDown() {}

  // Load a file on startup rather than about:blank.  This tests a longer
  // startup path, including resource loading and the loading of gears.dll.
  void SetUpWithFileURL() {
    FilePath file_url;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &file_url));
    file_url = file_url.AppendASCII("simple.html");
    ASSERT_TRUE(file_util::PathExists(file_url));
    launch_arguments_.AppendLooseValue(file_url.ToWStringHack());

    pages_ = WideToUTF8(file_url.ToWStringHack());
  }

  // Use the given profile in the test data extensions/profiles dir.  This tests
  // startup with extensions installed.
  void SetUpWithExtensionsProfile(const char* profile) {
    FilePath data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &data_dir);
    data_dir = data_dir.AppendASCII("extensions").AppendASCII("profiles").
        AppendASCII(profile);
    set_template_user_data(data_dir);

    // For now, these tests still depend on toolstrips.
    launch_arguments_.AppendSwitch(switches::kEnableExtensionToolstrips);
  }

  void RunStartupTest(const char* graph, const char* trace,
      bool test_cold, bool important, UITest::ProfileType profile_type) {
    profile_type_ = profile_type;

    // Sets the profile data for the run.  For now, this is only used for
    // the non-default themes test.
    if (profile_type != UITest::DEFAULT_THEME) {
      set_template_user_data(UITest::ComputeTypicalUserDataSource(
          profile_type));
    }

    const int kNumCyclesMax = 20;
    int numCycles = kNumCyclesMax;
// It's ok for unit test code to use getenv(), isn't it?
#if defined(OS_WIN)
#pragma warning( disable : 4996 )
#endif
    const char* numCyclesEnv = getenv("STARTUP_TESTS_NUMCYCLES");
    if (numCyclesEnv && StringToInt(numCyclesEnv, &numCycles))
      LOG(INFO) << "STARTUP_TESTS_NUMCYCLES set in environment, "
                << "so setting numCycles to " << numCycles;

    TimeDelta timings[kNumCyclesMax];
    for (int i = 0; i < numCycles; ++i) {
      if (test_cold) {
        FilePath dir_app;
        ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir_app));

        FilePath chrome_exe(dir_app.Append(
            FilePath::FromWStringHack(chrome::kBrowserProcessExecutablePath)));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_exe));
#if defined(OS_WIN)
        // chrome.dll is windows specific.
        FilePath chrome_dll(dir_app.Append(FILE_PATH_LITERAL("chrome.dll")));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_dll));
#endif

#if defined(OS_WIN)
        // TODO(port): Re-enable once gears is working on mac/linux.
        FilePath gears_dll;
        ASSERT_TRUE(PathService::Get(chrome::FILE_GEARS_PLUGIN, &gears_dll));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(gears_dll));
#else
        NOTIMPLEMENTED() << "gears not enabled yet";
#endif
      }
      UITest::SetUp();
      TimeTicks end_time = TimeTicks::Now();
      timings[i] = end_time - browser_launch_time_;
      // TODO(beng): Can't shut down so quickly. Figure out why, and fix. If we
      // do, we crash.
      PlatformThread::Sleep(50);
      UITest::TearDown();

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
      StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
    PrintResultList(graph, "", trace, times, "ms", important);
  }

 protected:
  std::string pages_;
};

class StartupReferenceTest : public StartupTest {
 public:
  // override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
    FilePath dir;
    PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
    dir = dir.AppendASCII("reference_build");
#if defined(OS_WIN)
    dir = dir.AppendASCII("chrome");
#elif defined(OS_LINUX)
    dir = dir.AppendASCII("chrome_linux");
#elif defined(OS_MACOSX)
    dir = dir.AppendASCII("chrome_mac");
#endif
    browser_directory_ = dir;
  }
};

TEST_F(StartupTest, Perf) {
  RunStartupTest("warm", "t", false /* not cold */, true /* important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(StartupReferenceTest, Perf) {
  RunStartupTest("warm", "t_ref", false /* not cold */,
                 true /* important */, UITest::DEFAULT_THEME);
}

// TODO(mpcomplete): Should we have reference timings for all these?

TEST_F(StartupTest, PerfCold) {
  RunStartupTest("cold", "t", true /* cold */, false /* not important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(StartupTest, PerfExtensionEmpty) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("empty");
  RunStartupTest("warm", "t", false /* cold */, false /* not important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(StartupTest, PerfExtensionToolstrips1) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("toolstrips1");
  RunStartupTest("warm", "extension_toolstrip1",
                 false /* cold */, false /* not important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(StartupTest, PerfExtensionToolstrips50) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("toolstrips50");
  RunStartupTest("warm", "extension_toolstrip50",
                 false /* cold */, false /* not important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(StartupTest, PerfExtensionContentScript1) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("content_scripts1");
  RunStartupTest("warm", "extension_content_scripts1",
                 false /* cold */, false /* not important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(StartupTest, PerfExtensionContentScript50) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("content_scripts50");
  RunStartupTest("warm", "extension_content_scripts50",
                 false /* cold */, false /* not important */,
                 UITest::DEFAULT_THEME);
}

#if defined(OS_WIN)
// TODO(port): Enable gears tests on linux/mac once gears is working.
TEST_F(StartupTest, PerfGears) {
  SetUpWithFileURL();
  RunStartupTest("warm", "gears", false /* not cold */,
                 false /* not important */, UITest::DEFAULT_THEME);
}

TEST_F(StartupTest, PerfColdGears) {
  SetUpWithFileURL();
  RunStartupTest("cold", "gears", true /* cold */,
                 false /* not important */, UITest::DEFAULT_THEME);
}
#endif

TEST_F(StartupTest, PerfColdComplexTheme) {
  RunStartupTest("warm", "t-theme", false /* warm */,
                 false /* not important */, UITest::COMPLEX_THEME);
}

#if defined(OS_LINUX)
TEST_F(StartupTest, PerfColdGtkTheme) {
  RunStartupTest("warm", "gtk-theme", false /* warm */,
                 false /* not important */, UITest::NATIVE_THEME);
}

TEST_F(StartupTest, PrefColdNativeFrame) {
  RunStartupTest("warm", "custom-frame", false /* warm */,
                 false /* not important */, UITest::CUSTOM_FRAME);
}

TEST_F(StartupTest, PerfColdNativeFrameGtkTheme) {
  RunStartupTest("warm", "custom-frame-gtk-theme", false /* warm */,
                 false /* not important */, UITest::CUSTOM_FRAME_NATIVE_THEME);
}
#endif

}  // namespace
