// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

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

TEST_F(GPUUITest, UITestLaunchedWithOSMesa) {
  // Check the webgl test reports success and that the renderer was OSMesa.
  // We use OSMesa for tests in order to get consistent results across a
  // variety of boxes.
  NavigateToURL(
      net::FilePathToFileURL(gpu_test_dir_.AppendASCII("webgl.html")));

  EXPECT_EQ(std::wstring(L"SUCCESS: Mesa OffScreen"), GetActiveTabTitle());
}
