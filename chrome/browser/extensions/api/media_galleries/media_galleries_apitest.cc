// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/common/chrome_switches.h"

namespace {

class MediaGalleriesApiTest: public PlatformAppBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(MediaGalleriesApiTest, MediaGalleries) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/media_galleries")) << message_;
}
