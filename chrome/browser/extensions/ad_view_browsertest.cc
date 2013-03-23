// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/common/chrome_switches.h"

class AdViewTest : public extensions::PlatformAppBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableAdview);
    command_line->AppendSwitch(switches::kEnableAdviewSrcAttribute);
  }
};

IN_PROC_BROWSER_TEST_F(AdViewTest, LoadEventIsCalled) {
  ASSERT_TRUE(StartTestServer());

  ExtensionTestMessageListener listener("guest-loaded", false);
  LoadAndLaunchPlatformApp("ad_view/load_event");
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(AdViewTest, AdNetworkIsLoaded) {
  ASSERT_TRUE(StartTestServer());

  ExtensionTestMessageListener listener("ad-network-loaded", false);
  LoadAndLaunchPlatformApp("ad_view/ad_network_loaded");
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

// This test currently fails on trybots because it requires new binary file
// (image315.png). The test will be enabled once the binary files are committed.
IN_PROC_BROWSER_TEST_F(AdViewTest, DISABLED_DisplayFirstAd) {
  ASSERT_TRUE(StartTestServer());

  ExtensionTestMessageListener listener("ad-displayed", false);
  LoadAndLaunchPlatformApp("ad_view/display_first_ad");
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}
