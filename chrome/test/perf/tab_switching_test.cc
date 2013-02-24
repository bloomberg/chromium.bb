// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

using base::TimeDelta;

namespace {

// This Automated UI test opens static files in different tabs in a proxy
// browser. After all the tabs have opened, it switches between tabs, and notes
// time taken for each switch. It then prints out the times on the console,
// with the aim that the page cycler parser can interpret these numbers to
// draw graphs for page cycler Tab Switching Performance.
class TabSwitchingUITest : public UIPerfTest {
 public:
  TabSwitchingUITest() {
    PathService::Get(base::DIR_SOURCE_ROOT, &path_prefix_);
    path_prefix_ = path_prefix_.AppendASCII("data");
    path_prefix_ = path_prefix_.AppendASCII("tab_switching");

    show_window_ = true;
  }

  virtual void SetUp() {
    // Set the log_file_name_ path according to the selected browser_directory_.
    log_file_name_ = browser_directory_.AppendASCII("chrome_debug.log");

    // Set the log file path for the browser test.
    scoped_ptr<base::Environment> env(base::Environment::Create());
#if defined(OS_WIN)
    env->SetVar(env_vars::kLogFileName,
                base::WideToUTF8(log_file_name_.value()));
#else
    env->SetVar(env_vars::kLogFileName, log_file_name_.value());
#endif

    // Add the necessary arguments to Chrome's launch command for these tests.
    AddLaunchArguments();

    // Run the rest of the UITest initialization.
    UITest::SetUp();
  }

  static const int kNumCycles = 5;

  void PrintTimings(const char* label, TimeDelta timings[kNumCycles],
    bool important) {
      std::string times;
      for (int i = 0; i < kNumCycles; ++i)
        base::StringAppendF(&times, "%.2f,", timings[i].InMillisecondsF());
      perf_test::PrintResultList("times", "", label, times, "ms", important);
  }

  void RunTabSwitchingUITest(const char* label, bool important) {
    // Shut down from window UITest sets up automatically.
    UITest::TearDown();

    TimeDelta timings[kNumCycles];
    for (int i = 0; i < kNumCycles; ++i) {
      // Prepare for this test run.
      SetUp();

      // Create a browser proxy.
      browser_proxy_ = automation()->GetBrowserWindow(0);

      // Open all the tabs.
      int initial_tab_count = 0;
      ASSERT_TRUE(browser_proxy_->GetTabCount(&initial_tab_count));
      int new_tab_count = OpenTabs();
      int tab_count = -1;
      ASSERT_TRUE(browser_proxy_->GetTabCount(&tab_count));
      ASSERT_EQ(initial_tab_count + new_tab_count, tab_count);

      // Switch linearly between tabs.
      ASSERT_TRUE(browser_proxy_->ActivateTab(0));
      int final_tab_count = 0;
      ASSERT_TRUE(browser_proxy_->GetTabCount(&final_tab_count));
      for (int j = initial_tab_count; j < final_tab_count; ++j) {
        ASSERT_TRUE(browser_proxy_->ActivateTab(j));
        ASSERT_TRUE(browser_proxy_->WaitForTabToBecomeActive(
            j, base::TimeDelta::FromSeconds(10)));
      }

      // Close the browser to force a dump of log.
      bool application_closed = false;
      EXPECT_TRUE(CloseBrowser(browser_proxy_.get(), &application_closed));

      // Open the corresponding log file and collect average from the
      // histogram stats generated for RenderWidgetHost_TabSwitchPaintDuration.
      bool log_has_been_dumped = false;
      std::string contents;
      int max_tries = 20;
      do {
        log_has_been_dumped = file_util::ReadFileToString(log_file_name_,
                                                          &contents);
        if (!log_has_been_dumped)
          base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
      } while (!log_has_been_dumped && max_tries--);
      ASSERT_TRUE(log_has_been_dumped) << "Failed to read the log file";

      // Parse the contents to get average.
      int64 average = 0;
      const std::string average_str("average = ");
      std::string::size_type pos = contents.find(
          "Histogram: MPArch.RWH_TabSwitchPaintDuration", 0);
      std::string::size_type comma_pos;
      std::string::size_type number_length;

      ASSERT_NE(pos, std::string::npos) <<
        "Histogram: MPArch.RWH_TabSwitchPaintDuration wasn't found\n" <<
        contents;

      // Get the average.
      pos = contents.find(average_str, pos);
      comma_pos = contents.find(",", pos);
      pos += average_str.length();
      number_length = comma_pos - pos;
      average = atoi(contents.substr(pos, number_length).c_str());

      // Print the average and standard deviation.
      timings[i] = TimeDelta::FromMilliseconds(average);

      // Clean up from the test run.
      UITest::TearDown();
    }
    PrintTimings(label, timings, important);
  }

 protected:
  // Opens new tabs. Returns the number of tabs opened.
  int OpenTabs() {
    // Add tabs.
    static const char* files[] = { "espn.go.com", "bugzilla.mozilla.org",
                                   "news.cnet.com", "www.amazon.com",
                                   "kannada.chakradeo.net", "allegro.pl",
                                   "ml.wikipedia.org", "www.bbc.co.uk",
                                   "126.com", "www.altavista.com"};
    int number_of_new_tabs_opened = 0;
    base::FilePath file_name;
    for (size_t i = 0; i < arraysize(files); ++i) {
      file_name = path_prefix_;
      file_name = file_name.AppendASCII(files[i]);
      file_name = file_name.AppendASCII("index.html");
      bool success =
          browser_proxy_->AppendTab(net::FilePathToFileURL(file_name));
      EXPECT_TRUE(success);
      if (success)
        number_of_new_tabs_opened++;
    }

    return number_of_new_tabs_opened;
  }

  base::FilePath path_prefix_;
  base::FilePath log_file_name_;
  scoped_refptr<BrowserProxy> browser_proxy_;

 private:
  void AddLaunchArguments() {
    launch_arguments_.AppendSwitch(switches::kEnableLogging);
    launch_arguments_.AppendSwitch(switches::kDumpHistogramsOnExit);
    launch_arguments_.AppendSwitchASCII(switches::kLoggingLevel, "0");
  }

  DISALLOW_COPY_AND_ASSIGN(TabSwitchingUITest);
};

// This is failing, and taking forever to finish when doing so.
// http://crbug.com/102162

TEST_F(TabSwitchingUITest, DISABLED_TabSwitch) {
  RunTabSwitchingUITest("t", true);
}

// Started failing with a webkit roll in r49936. See http://crbug.com/46751
TEST_F(TabSwitchingUITest, DISABLED_TabSwitchRef) {
  UseReferenceBuild();
  RunTabSwitchingUITest("t_ref", true);
}

}  // namespace
