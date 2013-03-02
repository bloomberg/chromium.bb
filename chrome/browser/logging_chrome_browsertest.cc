// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/environment.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::RenderProcessHost;

class ChromeLoggingTest : public testing::Test {
 public:
  // Stores the current value of the log file name environment
  // variable and sets the variable to new_value.
  void SaveEnvironmentVariable(std::string new_value) {
    scoped_ptr<base::Environment> env(base::Environment::Create());
    if (!env->GetVar(env_vars::kLogFileName, &environment_filename_))
      environment_filename_ = "";

    env->SetVar(env_vars::kLogFileName, new_value);
  }

  // Restores the value of the log file nave environment variable
  // previously saved by SaveEnvironmentVariable().
  void RestoreEnvironmentVariable() {
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar(env_vars::kLogFileName, environment_filename_);
  }

 private:
  std::string environment_filename_;  // Saves real environment value.
};

// Tests the log file name getter without an environment variable.
TEST_F(ChromeLoggingTest, LogFileName) {
  SaveEnvironmentVariable("");

  base::FilePath filename = logging::GetLogFileName();
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("chrome_debug.log")));

  RestoreEnvironmentVariable();
}

// Tests the log file name getter with an environment variable.
TEST_F(ChromeLoggingTest, EnvironmentLogFileName) {
  SaveEnvironmentVariable("test value");

  base::FilePath filename = logging::GetLogFileName();
  ASSERT_EQ(base::FilePath(FILE_PATH_LITERAL("test value")).value(),
            filename.value());

  RestoreEnvironmentVariable();
}

// Tests whether we correctly fail on browser crashes.
class RendererCrashTest : public InProcessBrowserTest,
                          public content::NotificationObserver{
 protected:
  RendererCrashTest() : saw_crash_(false) {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    content::RenderProcessHost::RendererClosedDetails* process_details =
          content::Details<
              content::RenderProcessHost::RendererClosedDetails>(
                  details).ptr();
    base::TerminationStatus status = process_details->status;
    if (status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
        status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
      saw_crash_ = true;
      MessageLoopForUI::current()->Quit();
    }
  }

  bool saw_crash_;
  content::NotificationRegistrar registrar_;
};

// Flaky, http://crbug.com/107226 .
IN_PROC_BROWSER_TEST_F(RendererCrashTest, DISABLED_Crash) {
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(content::kChromeUICrashURL), CURRENT_TAB, 0);
  content::RunMessageLoop();
  ASSERT_TRUE(saw_crash_);
}
