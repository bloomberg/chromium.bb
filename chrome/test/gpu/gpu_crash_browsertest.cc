// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"

namespace {

void SimulateGPUCrash(Browser* browser) {
  LOG(ERROR) << "SimulateGPUCrash, before NavigateToURLWithDisposition";
  ui_test_utils::NavigateToURLWithDisposition(browser,
      GURL(chrome::kChromeUIGpuCrashURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  browser->SelectPreviousTab();
  LOG(ERROR) << "SimulateGPUCrash, after CloseTab";
}

} // namespace

class GPUCrashTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    EnableDOMAutomation();
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // GPU tests require gpu acceleration.
    // We do not care which GL backend is used.
    command_line->AppendSwitchASCII(switches::kUseGL, "any");
  }
  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }
  FilePath gpu_test_dir_;
};

// Currently Kill times out on GPU bots: http://crbug.com/101513
IN_PROC_BROWSER_TEST_F(GPUCrashTest, DISABLED_Kill) {
  ui_test_utils::DOMMessageQueue message_queue;

  ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetFileUrlWithQuery(
          gpu_test_dir_.AppendASCII("webgl.html"), "query=kill"));
  SimulateGPUCrash(browser());

  std::string m;
  ASSERT_TRUE(message_queue.WaitForMessage(&m));
  EXPECT_EQ("\"SUCCESS\"", m);
}


IN_PROC_BROWSER_TEST_F(GPUCrashTest, WebkitLoseContext) {
  ui_test_utils::DOMMessageQueue message_queue;

  ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetFileUrlWithQuery(
          gpu_test_dir_.AppendASCII("webgl.html"),
          "query=WEBGL_lose_context"));

  std::string m;
  ASSERT_TRUE(message_queue.WaitForMessage(&m));
  EXPECT_EQ("\"SUCCESS\"", m);
}
