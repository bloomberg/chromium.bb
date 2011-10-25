// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "ui/gfx/gl/gl_switches.h"

class GPUBrowserTest : public InProcessBrowserTest {
 protected:
  GPUBrowserTest() {
    EnableDOMAutomation();
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // GPU tests require gpu acceleration.
    // We do not care which GL backend is used.
    command_line->AppendSwitchASCII(switches::kUseGL, "any");

    command_line->AppendSwitch(switches::kDisablePopupBlocking);
  }

  FilePath gpu_test_dir_;
};

// Test is flaky and timing out.  See crbug.com/99883
IN_PROC_BROWSER_TEST_F(GPUBrowserTest,
                       DISABLED_CanOpenPopupAndRenderWithWebGLCanvas) {
  ui_test_utils::DOMMessageQueue message_queue;

  ui_test_utils::NavigateToURL(
      browser(),
      net::FilePathToFileURL(gpu_test_dir_.AppendASCII("webgl_popup.html")));

  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ("\"SUCCESS\"", result);
}

IN_PROC_BROWSER_TEST_F(GPUBrowserTest, CanOpenPopupAndRenderWith2DCanvas) {
  ui_test_utils::DOMMessageQueue message_queue;

  ui_test_utils::NavigateToURL(
      browser(),
      net::FilePathToFileURL(gpu_test_dir_.AppendASCII("canvas_popup.html")));

  std::string result;
  ASSERT_TRUE(message_queue.WaitForMessage(&result));
  EXPECT_EQ("\"SUCCESS\"", result);
}
