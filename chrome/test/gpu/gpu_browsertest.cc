// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_implementation.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/test_launcher_utils.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

class GPUBrowserTest : public InProcessBrowserTest {
 protected:
  GPUBrowserTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    gpu_test_dir_ = test_dir.AppendASCII("gpu");
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    EXPECT_TRUE(test_launcher_utils::OverrideGLImplementation(
        command_line,
        gfx::kGLImplementationOSMesaName));

#if defined(OS_MACOSX)
    // Accelerated compositing does not work with OSMesa. AcceleratedSurface
    // assumes GL contexts are native.
    command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
#endif
  }

  FilePath gpu_test_dir_;
};

#if defined(OS_WIN) || defined(OS_CHROMEOS)
// Flaky on Windows (dbg): http://crbug.com/72608
// For ChromeOS: http://crbug.com/76217
#define MAYBE_BrowserTestCanLaunchWithOSMesa DISABLED_BrowserTestCanLaunchWithOSMesa
#else
#define MAYBE_BrowserTestCanLaunchWithOSMesa BrowserTestCanLaunchWithOSMesa
#endif

IN_PROC_BROWSER_TEST_F(GPUBrowserTest, MAYBE_BrowserTestCanLaunchWithOSMesa) {
  // Check the webgl test reports success and that the renderer was OSMesa.
  ui_test_utils::NavigateToURL(
      browser(),
      net::FilePathToFileURL(gpu_test_dir_.AppendASCII("webgl.html")));

  EXPECT_EQ(ASCIIToUTF16("SUCCESS: Mesa OffScreen"),
            browser()->GetSelectedTabContents()->GetTitle());
}
