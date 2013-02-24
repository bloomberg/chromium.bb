// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "net/base/net_util.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

class StartupTest : public UIPerfTest {
 public:
  StartupTest() {
    show_window_ = true;
  }
  virtual void SetUp() {
    collect_profiling_stats_ = false;
  }
  virtual void TearDown() {}

  enum TestColdness {
    WARM,
    COLD
  };

  enum TestImportance {
    NOT_IMPORTANT,
    IMPORTANT
  };

  // Load a file on startup rather than about:blank.  This tests a longer
  // startup path, including resource loading.
  void SetUpWithFileURL() {
    const base::FilePath file_url = ui_test_utils::GetTestFilePath(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("simple.html")));
    ASSERT_TRUE(file_util::PathExists(file_url));
    launch_arguments_.AppendArgPath(file_url);
  }

  // Setup the command line arguments to capture profiling data for tasks.
  void SetUpWithProfiling() {
    profiling_file_ = ui_test_utils::GetTestFilePath(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("task_profile.json")));
    launch_arguments_.AppendSwitchPath(switches::kProfilingOutputFile,
                                       profiling_file_);
    collect_profiling_stats_ = true;
  }

  // Load a complex html file on startup represented by |which_tab|.
  void SetUpWithComplexFileURL(unsigned int which_tab) {
    const char* const kTestPageCyclerDomains[] = {
        "www.google.com", "www.nytimes.com",
        "www.yahoo.com", "espn.go.com", "www.amazon.com"
    };
    unsigned int which_el = which_tab % arraysize(kTestPageCyclerDomains);
    const char* this_domain = kTestPageCyclerDomains[which_el];

    base::FilePath page_cycler_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &page_cycler_path);
    page_cycler_path = page_cycler_path.AppendASCII("data")
        .AppendASCII("page_cycler").AppendASCII("moz")
        .AppendASCII(this_domain).AppendASCII("index.html");
    GURL file_url = net::FilePathToFileURL(page_cycler_path).Resolve("?skip");
    launch_arguments_.AppendArg(file_url.spec());
  }

  // Use the given profile in the test data extensions/profiles dir.  This tests
  // startup with extensions installed.
  void SetUpWithExtensionsProfile(const char* profile) {
    base::FilePath data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &data_dir);
    data_dir = data_dir.AppendASCII("extensions").AppendASCII("profiles").
        AppendASCII(profile);
    set_template_user_data(data_dir);
  }

  // Rewrite the preferences file to point to the proper image directory.
  virtual void SetUpProfile() OVERRIDE {
    UIPerfTest::SetUpProfile();
    if (profile_type_ != UITestBase::COMPLEX_THEME)
      return;

    const base::FilePath pref_template_path(user_data_dir().
        AppendASCII("Default").
        AppendASCII("PreferencesTemplate"));
    const base::FilePath pref_path(user_data_dir().
        AppendASCII(TestingProfile::kTestUserProfileDir).
        AppendASCII("Preferences"));

    // Read in preferences template.
    std::string pref_string;
    EXPECT_TRUE(file_util::ReadFileToString(pref_template_path, &pref_string));
    string16 format_string = ASCIIToUTF16(pref_string);

    // Make sure temp directory has the proper format for writing to prefs file.
#if defined(OS_POSIX)
    std::wstring user_data_dir_w(base::ASCIIToWide(user_data_dir().value()));
#elif defined(OS_WIN)
    std::wstring user_data_dir_w(user_data_dir().value());
    // In Windows, the FilePath will write '\' for the path separators; change
    // these to a separator that won't trigger escapes.
    std::replace(user_data_dir_w.begin(),
                 user_data_dir_w.end(), '\\', '/');
#endif

    // Rewrite prefs file.
    std::vector<string16> subst;
    subst.push_back(base::WideToUTF16(user_data_dir_w));
    const std::string prefs_string =
        UTF16ToASCII(ReplaceStringPlaceholders(format_string, subst, NULL));
    EXPECT_TRUE(file_util::WriteFile(pref_path, prefs_string.c_str(),
                                     prefs_string.size()));
    file_util::EvictFileFromSystemCache(pref_path);
  }

  // Runs a test which loads |tab_count| tabs on startup, either as command line
  // arguments or, if |restore_session| is true, by using session restore.
  // |nth_timed_tab|, if non-zero, will measure time to load the first n+1 tabs.
  void RunPerfTestWithManyTabs(const char* graph, const char* trace,
                               int tab_count, int nth_timed_tab,
                               bool restore_session);

  void RunStartupTest(const char* graph, const char* trace,
                      TestColdness test_cold, TestImportance test_importance,
                      UITestBase::ProfileType profile_type,
                      int num_tabs, int nth_timed_tab) {
    bool important = (test_importance == IMPORTANT);
    profile_type_ = profile_type;

    // Sets the profile data for the run.  For now, this is only used for
    // the non-default themes test.
    if (profile_type != UITestBase::DEFAULT_THEME) {
      set_template_user_data(UITest::ComputeTypicalUserDataSource(
          profile_type));
    }

#if defined(NDEBUG)
    const int kNumCyclesMax = 20;
#else
    // Debug builds are too slow and we can't run that many cycles in a
    // reasonable amount of time.
    const int kNumCyclesMax = 10;
#endif
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

    struct TimingInfo {
      TimeDelta end_to_end;
      float first_start_ms;
      float last_stop_ms;
      float first_stop_ms;
      float nth_tab_stop_ms;
    };
    TimingInfo timings[kNumCyclesMax];

    for (int i = 0; i < numCycles; ++i) {
      if (test_cold == COLD) {
        base::FilePath dir_app;
        ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir_app));

        base::FilePath chrome_exe(dir_app.Append(GetExecutablePath()));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_exe));
#if defined(OS_WIN)
        // chrome.dll is windows specific.
        base::FilePath chrome_dll(
            dir_app.Append(FILE_PATH_LITERAL("chrome.dll")));
        ASSERT_TRUE(EvictFileFromSystemCacheWrapper(chrome_dll));
#endif
      }
      UITest::SetUp();
      TimeTicks end_time = TimeTicks::Now();

      if (num_tabs > 0) {
        float min_start;
        float max_stop;
        std::vector<float> times;
        scoped_refptr<BrowserProxy> browser_proxy(
            automation()->GetBrowserWindow(0));
        ASSERT_TRUE(browser_proxy.get());

        if (browser_proxy->GetInitialLoadTimes(
              TestTimeouts::action_max_timeout(),
              &min_start,
              &max_stop,
              &times) &&
            !times.empty()) {
          ASSERT_LT(nth_timed_tab, num_tabs);
          ASSERT_EQ(times.size(), static_cast<size_t>(num_tabs));
          timings[i].first_start_ms = min_start;
          timings[i].last_stop_ms = max_stop;
          timings[i].first_stop_ms = times[0];
          timings[i].nth_tab_stop_ms = times[nth_timed_tab];
        } else {
          // Browser might not support initial load times.
          // Only use end-to-end time for this test.
          num_tabs = 0;
        }
      }
      timings[i].end_to_end = end_time - browser_launch_time();
      UITest::TearDown();

      if (i == 0) {
        // Re-use the profile data after first run so that the noise from
        // creating databases doesn't impact all the runs.
        clear_profile_ = false;
        // Clear template_user_data_ so we don't try to copy it over each time
        // through.
        set_template_user_data(base::FilePath());
      }
    }

    std::string times;
    for (int i = 0; i < numCycles; ++i) {
      base::StringAppendF(&times,
                          "%.2f,",
                          timings[i].end_to_end.InMillisecondsF());
    }
    perf_test::PrintResultList(graph, "", trace, times, "ms", important);

    if (num_tabs > 0) {
      std::string name_base = trace;
      std::string name;

      times.clear();
      name = name_base + "-start";
      for (int i = 0; i < numCycles; ++i)
        base::StringAppendF(&times, "%.2f,", timings[i].first_start_ms);
      perf_test::PrintResultList(graph, "", name.c_str(), times, "ms",
                                 important);

      times.clear();
      name = name_base + "-first";
      for (int i = 0; i < numCycles; ++i)
        base::StringAppendF(&times, "%.2f,", timings[i].first_stop_ms);
      perf_test::PrintResultList(graph, "", name.c_str(), times, "ms",
                                 important);

      if (nth_timed_tab > 0) {
        // Display only the time necessary to load the first n tabs.
        times.clear();
        name = name_base + "-" + base::IntToString(nth_timed_tab);
        for (int i = 0; i < numCycles; ++i)
          base::StringAppendF(&times, "%.2f,", timings[i].nth_tab_stop_ms);
        perf_test::PrintResultList(graph, "", name.c_str(), times, "ms",
                                   important);
      }

      if (num_tabs > 1) {
        // Display the time necessary to load all of the tabs.
        times.clear();
        name = name_base + "-all";
        for (int i = 0; i < numCycles; ++i)
          base::StringAppendF(&times, "%.2f,", timings[i].last_stop_ms);
        perf_test::PrintResultList(graph, "", name.c_str(), times, "ms",
                                   important);
      }
    }
  }

  base::FilePath profiling_file_;
  bool collect_profiling_stats_;
};

TEST_F(StartupTest, PerfWarm) {
  RunStartupTest("warm", "t", WARM, IMPORTANT,
                 UITestBase::DEFAULT_THEME, 0, 0);
}

TEST_F(StartupTest, PerfReferenceWarm) {
  UseReferenceBuild();
  RunStartupTest("warm", "t_ref", WARM, IMPORTANT,
                 UITestBase::DEFAULT_THEME, 0, 0);
}

// TODO(mpcomplete): Should we have reference timings for all these?

// dominich: Disabling as per http://crbug.com/100900.
#if defined(OS_WIN)
#define MAYBE_PerfCold DISABLED_PerfCold
#else
#define MAYBE_PerfCold PerfCold
#endif

TEST_F(StartupTest, MAYBE_PerfCold) {
  RunStartupTest("cold", "t", COLD, NOT_IMPORTANT,
                 UITestBase::DEFAULT_THEME, 0, 0);
}

void StartupTest::RunPerfTestWithManyTabs(const char* graph, const char* trace,
                                          int tab_count, int nth_timed_tab,
                                          bool restore_session) {
  // Initialize session with |tab_count| tabs.
  for (int i = 0; i < tab_count; ++i)
    SetUpWithComplexFileURL(i);

  if (restore_session) {
    // Start the browser with these urls so we can save the session and exit.
    UITest::SetUp();
    // Set flags to ensure profile is saved and can be restored.
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
    set_shutdown_type(ProxyLauncher::USER_QUIT);
#endif
    clear_profile_ = false;
    // Quit and set flags to restore session.
    UITest::TearDown();

    // Clear all arguments for session restore, or the number of open tabs
    // will grow with each restore.
    CommandLine new_launch_arguments = CommandLine(CommandLine::NO_PROGRAM);
    // Keep the branding switch if using a reference build.
    if (launch_arguments_.HasSwitch(switches::kEnableChromiumBranding)) {
      new_launch_arguments.AppendSwitch(switches::kEnableChromiumBranding);
    }
    // The session will be restored once per cycle for numCycles test cycles,
    // and each time, UITest::SetUp will wait for |tab_count| tabs to
    // finish loading.
    new_launch_arguments.AppendSwitchASCII(switches::kRestoreLastSession,
                                        base::IntToString(tab_count));
    launch_arguments_ = new_launch_arguments;
  }
  RunStartupTest(graph, trace, WARM, NOT_IMPORTANT,
                 UITestBase::DEFAULT_THEME, tab_count, nth_timed_tab);
}

// http://crbug.com/101591
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_PerfFewTabs DISABLED_PerfFewTabs
#else
#define MAYBE_PerfFewTabs PerfFewTabs
#endif

TEST_F(StartupTest, MAYBE_PerfFewTabs) {
  RunPerfTestWithManyTabs("few_tabs", "cmdline", 5, 2, false);
}

TEST_F(StartupTest, PerfFewTabsReference) {
  UseReferenceBuild();
  RunPerfTestWithManyTabs("few_tabs", "cmdline-ref", 5, 2, false);
}

TEST_F(StartupTest, PerfRestoreFewTabs) {
  RunPerfTestWithManyTabs("few_tabs", "restore", 5, 2, true);
}

TEST_F(StartupTest, PerfRestoreFewTabsReference) {
  UseReferenceBuild();
  RunPerfTestWithManyTabs("few_tabs", "restore-ref", 5, 2, true);
}

#if defined(OS_MACOSX)
// http://crbug.com/46609
#define MAYBE_PerfSeveralTabsReference DISABLED_PerfSeveralTabsReference
#define MAYBE_PerfSeveralTabs DISABLED_PerfSeveralTabs
// http://crbug.com/52858
#define MAYBE_PerfRestoreSeveralTabs DISABLED_PerfRestoreSeveralTabs
#define MAYBE_PerfExtensionContentScript50 DISABLED_PerfExtensionContentScript50
#elif defined(OS_WIN)
// http://crbug.com/46609
#if !defined(NDEBUG)
// This test is disabled on Windows Debug. See bug http://crbug.com/132279
#define MAYBE_PerfRestoreSeveralTabs DISABLED_PerfRestoreSeveralTabs
#else
#define MAYBE_PerfRestoreSeveralTabs PerfRestoreSeveralTabs
#endif  // !defined(NDEBUG)
// http://crbug.com/102584
#define MAYBE_PerfSeveralTabs DISABLED_PerfSeveralTabs
#define MAYBE_PerfSeveralTabsReference PerfSeveralTabsReference
#define MAYBE_PerfExtensionContentScript50 PerfExtensionContentScript50
#else
#define MAYBE_PerfSeveralTabsReference PerfSeveralTabsReference
#define MAYBE_PerfSeveralTabs PerfSeveralTabs
#define MAYBE_PerfRestoreSeveralTabs PerfRestoreSeveralTabs
#define MAYBE_PerfExtensionContentScript50 PerfExtensionContentScript50
#endif

// http://crbug.com/99604
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_PerfComplexTheme DISABLED_PerfComplexTheme
#else
#define MAYBE_PerfComplexTheme PerfComplexTheme
#endif

TEST_F(StartupTest, MAYBE_PerfSeveralTabs) {
  RunPerfTestWithManyTabs("several_tabs", "cmdline", 10, 4, false);
}

TEST_F(StartupTest, MAYBE_PerfSeveralTabsReference) {
  UseReferenceBuild();
  RunPerfTestWithManyTabs("several_tabs", "cmdline-ref", 10, 4, false);
}

TEST_F(StartupTest, MAYBE_PerfRestoreSeveralTabs) {
  RunPerfTestWithManyTabs("several_tabs", "restore", 10, 4, true);
}

TEST_F(StartupTest, PerfRestoreSeveralTabsReference) {
  UseReferenceBuild();
  RunPerfTestWithManyTabs("several_tabs", "restore-ref", 10, 4, true);
}

TEST_F(StartupTest, PerfExtensionEmpty) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("empty");
  RunStartupTest("warm", "extension_empty", WARM, NOT_IMPORTANT,
                 UITestBase::DEFAULT_THEME, 1, 0);
}

TEST_F(StartupTest, PerfExtensionContentScript1) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("content_scripts1");
  RunStartupTest("warm", "extension_content_scripts1", WARM, NOT_IMPORTANT,
                 UITestBase::DEFAULT_THEME, 1, 0);
}

TEST_F(StartupTest, MAYBE_PerfExtensionContentScript50) {
  SetUpWithFileURL();
  SetUpWithExtensionsProfile("content_scripts50");
  RunStartupTest("warm", "extension_content_scripts50", WARM, NOT_IMPORTANT,
                 UITestBase::DEFAULT_THEME, 1, 0);
}

TEST_F(StartupTest, MAYBE_PerfComplexTheme) {
  RunStartupTest("warm", "t-theme", WARM, NOT_IMPORTANT,
                 UITestBase::COMPLEX_THEME, 0, 0);
}

TEST_F(StartupTest, ProfilingScript1) {
  SetUpWithFileURL();
  SetUpWithProfiling();
  RunStartupTest("warm", "profiling_scripts1", WARM, NOT_IMPORTANT,
                 UITestBase::DEFAULT_THEME, 1, 0);
}

}  // namespace
