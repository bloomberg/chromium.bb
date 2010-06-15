// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/env_var.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/test/test_file_util.h"
#include "base/time.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

class StartupTest : public UITest {
 public:
  StartupTest() {
    show_window_ = true;
  }
  void SetUp() {}
  void TearDown() {}

  // Load a file on startup rather than about:blank.  This tests a longer
  // startup path, including resource loading and the loading of gears.dll.
  void SetUpWithFileURL() {
    const FilePath file_url = ui_test_utils::GetTestFilePath(
        FilePath(FilePath::kCurrentDirectory),
        FilePath(FILE_PATH_LITERAL("simple.html")));
    ASSERT_TRUE(file_util::PathExists(file_url));
    launch_arguments_.AppendLooseValue(file_url.ToWStringHack());
  }

  // Load a complex html file on startup represented by |which_tab|.
  void SetUpWithComplexFileURL(unsigned int which_tab) {
    const char* const kTestPageCyclerDomains[] = {
        "www.google.com", "www.nytimes.com",
        "www.yahoo.com", "espn.go.com", "www.amazon.com"
    };
    unsigned int which_el = which_tab % arraysize(kTestPageCyclerDomains);
    const char* this_domain = kTestPageCyclerDomains[which_el];

    FilePath page_cycler_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &page_cycler_path);
    page_cycler_path = page_cycler_path.AppendASCII("data")
        .AppendASCII("page_cycler").AppendASCII("moz")
        .AppendASCII(this_domain).AppendASCII("index.html");
    GURL file_url = net::FilePathToFileURL(page_cycler_path).Resolve("?skip");
    launch_arguments_.AppendLooseValue(ASCIIToWide(file_url.spec()));
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

  void RunPerfTestWithManyTabs(const char *test_name,
      int tab_count, bool restore_session);

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
    scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());
    std::string numCyclesEnv;
    if (env->GetEnv(env_vars::kStartupTestsNumCycles, &numCyclesEnv) &&
        StringToInt(numCyclesEnv, &numCycles)) {
      if (numCycles <= kNumCyclesMax) {
        LOG(INFO) << env_vars::kStartupTestsNumCycles
                  << " set in environment, so setting numCycles to "
                  << numCycles;
      } else {
        LOG(INFO) << env_vars::kStartupTestsNumCycles
                  << " is higher than the max, setting numCycles to "
                  << kNumCyclesMax;
        numCycles = kNumCyclesMax;
      }
    }

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
};

TEST_F(StartupTest, PerfWarm) {
  RunStartupTest("warm", "t", false /* not cold */, true /* important */,
                 UITest::DEFAULT_THEME);
}

TEST_F(StartupTest, PerfReferenceWarm) {
  UseReferenceBuild();
  RunStartupTest("warm", "t_ref", false /* not cold */,
                 true /* important */, UITest::DEFAULT_THEME);
}

// TODO(mpcomplete): Should we have reference timings for all these?

TEST_F(StartupTest, PerfCold) {
  RunStartupTest("cold", "t", true /* cold */, false /* not important */,
                 UITest::DEFAULT_THEME);
}

void StartupTest::RunPerfTestWithManyTabs(const char *test_name,
    int tab_count, bool restore_session) {
  // Initialize session with |tab_count| tabs.
  for (int i = 0; i < tab_count; ++i)
    SetUpWithComplexFileURL(i);

  if (restore_session) {
    // Start the browser with these urls so we can save the session and exit.
    UITest::SetUp();
    // Set flags to ensure profile is saved and can be restored.
#if defined(OS_MACOSX)
    shutdown_type_ = UITestBase::USER_QUIT;
#endif
    clear_profile_ = false;
    // Quit and set flags to restore session.
    UITest::TearDown();
    // Clear all arguments for session restore, or the number of open tabs
    // will grow with each restore.
    launch_arguments_ = CommandLine(CommandLine::ARGUMENTS_ONLY);
    // The session will be restored once per cycle for numCycles test cycles,
    // and each time, UITest::SetUp will wait for |tab_count| tabs to
    // finish loading.
    launch_arguments_.AppendSwitchWithValue(switches::kRestoreLastSession,
                                            IntToWString(tab_count));
  }
  RunStartupTest("warm", test_name,
                 false, false,
                 UITest::DEFAULT_THEME);
}

TEST_F(StartupTest, PerfFewTabs) {
  RunPerfTestWithManyTabs("few_tabs", 5, false);
}

TEST_F(StartupTest, PerfSeveralTabs) {
  RunPerfTestWithManyTabs("several_tabs", 20, false);
}

TEST_F(StartupTest, PerfRestoreFewTabs) {
  RunPerfTestWithManyTabs("restore_few_tabs", 5, true);
}

TEST_F(StartupTest, PerfRestoreSeveralTabs) {
  RunPerfTestWithManyTabs("restore_several_tabs", 20, true);
}

TEST_F(StartupTest, PerfExtensionEmpty) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("empty");
  RunStartupTest("warm", "extension_empty",
                 false /* cold */, false /* not important */,
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
