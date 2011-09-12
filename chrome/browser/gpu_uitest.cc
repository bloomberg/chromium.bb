// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "ui/gfx/gl/gl_implementation.h"

class GPUUITest : public UITest {
 protected:
  GPUUITest() {
  }

  virtual void SetUp() {
    UITest::SetUp();

    gpu_test_dir_ = test_data_directory_.AppendASCII("gpu");
  }

  FilePath gpu_test_dir_;
};

#if !defined(OS_WIN) && defined(TOOLKIT_VIEWS)
#define UITestCanLaunchWithOSMesa DISABLED_UITestCanLaunchWithOSMesa
// http://crbug.com/76217
#endif
TEST_F(GPUUITest, UITestCanLaunchWithOSMesa) {
  // Check the webgl test reports success and that the renderer was OSMesa.
  NavigateToURL(
      net::FilePathToFileURL(gpu_test_dir_.AppendASCII("webgl.html")));

  EXPECT_EQ(std::wstring(L"SUCCESS: Mesa OffScreen"), GetActiveTabTitle());
}
