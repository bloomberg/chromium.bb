// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/test/net/url_request_prepackaged_interceptor.h"
#include "net/url_request/url_fetcher.h"

class AdViewTest : public extensions::PlatformAppBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableAdview);
    command_line->AppendSwitch(switches::kEnableAdviewSrcAttribute);
  }
};

// This test checks the "loadcommit" event is called when the page inside an
// <adview> is loaded.
#if defined(OS_MACOSX)
// Very flaky on MacOS 10.8.
#define MAYBE_LoadCommitEventIsCalled DISABLED_LoadCommitEventIsCalled
#else
#define MAYBE_LoadCommitEventIsCalled LoadCommitEventIsCalled
#endif
IN_PROC_BROWSER_TEST_F(AdViewTest, MAYBE_LoadCommitEventIsCalled) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/loadcommit_event")) << message_;
}

// This test checks the "loadabort" event is called when the "src" attribute
// of an <adview> is an invalid URL.
IN_PROC_BROWSER_TEST_F(AdViewTest, LoadAbortEventIsCalled) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/loadabort_event")) << message_;
}

// This test checks the page loaded inside an <adview> has the ability to
// 1) receive "message" events from the application, and 2) use
// "window.postMessage" to post back a message to the application.
#if defined(OS_WIN)
// Flaky, or takes too long time on Win7. (http://crbug.com/230271)
#define MAYBE_CommitMessageFromAdNetwork DISABLED_CommitMessageFromAdNetwork
#else
#define MAYBE_CommitMessageFromAdNetwork CommitMessageFromAdNetwork
#endif
IN_PROC_BROWSER_TEST_F(AdViewTest, MAYBE_CommitMessageFromAdNetwork) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/onloadcommit_ack")) << message_;
}

// This test checks the page running inside an <adview> has the ability to load
// and display an image inside an <iframe>.
// Note: Disabled for initial checkin because the test depends on a binary
//       file (image035.png) which the trybots don't process correctly when
//       first checked-in.
IN_PROC_BROWSER_TEST_F(AdViewTest, DISABLED_DisplayFirstAd) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/display_first_ad")) << message_;
}

// This test checks that <adview> attributes are also exposed as properties
// (with the same name and value).
IN_PROC_BROWSER_TEST_F(AdViewTest, PropertiesAreInSyncWithAttributes) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/properties_exposed")) << message_;
}

// This test checks an <adview> element has no behavior when the "adview"
// permission is missing from the application manifest.
IN_PROC_BROWSER_TEST_F(AdViewTest, AdViewPermissionIsRequired) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/permission_required")) << message_;
}

// This test checks that 1) it is possible change the value of the "ad-network"
// attribute of an <adview> element and 2) changing the value will reset the
// "src" attribute.
IN_PROC_BROWSER_TEST_F(AdViewTest, ChangeAdNetworkValue) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/change_ad_network")) << message_;
}

class AdViewNoSrcTest : public extensions::PlatformAppBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableAdview);
    //Note: The "kEnableAdviewSrcAttribute" flag is not here!
  }
};

// This test checks an invalid "ad-network" value (i.e. not whitelisted)
// is ignored.
IN_PROC_BROWSER_TEST_F(AdViewNoSrcTest, InvalidAdNetworkIsIgnored) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/invalid_ad_network")) << message_;
}

// This test checks the "src" attribute is ignored when the
// "kEnableAdviewSrcAttribute" is missing.
IN_PROC_BROWSER_TEST_F(AdViewNoSrcTest, EnableAdviewSrcAttributeFlagRequired) {
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/src_flag_required")) << message_;
}

// This test checks 1) an <adview> works end-to-end (i.e. page is loaded) when
// using a whitelisted ad-network, and 2) the "src" attribute is never exposed
// to the application.
IN_PROC_BROWSER_TEST_F(AdViewNoSrcTest, SrcNotExposed) {
  base::FilePath file_path = test_data_dir_
    .AppendASCII("platform_apps")
    .AppendASCII("ad_view/src_not_exposed")
    .AppendASCII("ad_network_fake_website.html");

  // Note: The following URL is identical to the whitelisted url
  //       for "admob" (see ad_view.js).
  GURL url = GURL("https://admob-sdk.doubleclick.net/chromeapps");
  std::string scheme = url.scheme();
  std::string hostname = url.host();

  content::URLRequestPrepackagedInterceptor interceptor(scheme, hostname);
  interceptor.SetResponse(url, file_path);

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/src_not_exposed")) << message_;
  ASSERT_EQ(1, interceptor.GetHitCount());
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
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/ad_view/flag_required")) << message_;
}
