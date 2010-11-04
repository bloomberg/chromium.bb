// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
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

  FilePath gpu_test_dir_;
};

// TODO(apatrick): OSMesa is flaky on Mac and Windows. http://crbug/61037
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_BrowserTestLaunchedWithOSMesa \
    DISABLED_BrowserTestLaunchedWithOSMesa
#else
#define MAYBE_BrowserTestLaunchedWithOSMesa BrowserTestLaunchedWithOSMesa
#endif

IN_PROC_BROWSER_TEST_F(GPUBrowserTest, MAYBE_BrowserTestLaunchedWithOSMesa) {
  // Check the webgl test reports success and that the renderer was OSMesa.
  // We use OSMesa for tests in order to get consistent results across a
  // variety of boxes.
  ui_test_utils::NavigateToURL(
      browser(),
      net::FilePathToFileURL(gpu_test_dir_.AppendASCII("webgl.html")));

  EXPECT_EQ(ASCIIToUTF16("SUCCESS: Mesa OffScreen"),
            browser()->GetSelectedTabContents()->GetTitle());
}
