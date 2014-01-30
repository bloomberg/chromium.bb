// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_switches.h"
#include "content/test/net/url_request_prepackaged_interceptor.h"
#include "net/url_request/url_fetcher.h"

class AdViewTest : public extensions::PlatformAppBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableAdview);
  }
};

// This test checks that <adview> attributes are also exposed as properties
// (with the same name and value).
#if defined(OS_WIN)
// Flaky on Win XP. (http://crbug.com/264362)
#define MAYBE_PropertiesAreInSyncWithAttributes \
    DISABLED_PropertiesAreInSyncWithAttributes
#else
#define MAYBE_PropertiesAreInSyncWithAttributes \
    PropertiesAreInSyncWithAttributes
#endif
IN_PROC_BROWSER_TEST_F(AdViewTest, MAYBE_PropertiesAreInSyncWithAttributes) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/properties_exposed")) << message_;
}

// This test checks an invalid "ad-network" value (i.e. not whitelisted)
// is ignored.
IN_PROC_BROWSER_TEST_F(AdViewTest, InvalidAdNetworkIsIgnored) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/invalid_ad_network")) << message_;
}

class AdViewNotEnabledTest : public extensions::PlatformAppBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    //Note: The "kEnableAdview" flag is not here!
  }
};

// This test checks an <adview> element has no behavior when the "kEnableAdview"
// flag is missing.
IN_PROC_BROWSER_TEST_F(AdViewNotEnabledTest, EnableAdviewFlagRequired) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/flag_required")) << message_;
}
