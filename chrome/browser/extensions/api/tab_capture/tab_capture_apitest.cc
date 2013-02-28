// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/features/feature.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"

namespace chrome {

namespace {

class TabCaptureApiTest : public ExtensionApiTest {
 public:
  TabCaptureApiTest() : current_channel_(VersionInfo::CHANNEL_UNKNOWN) {}

 private:
  extensions::Feature::ScopedCurrentChannel current_channel_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, ApiTests) {
  extensions::FeatureSwitch::ScopedOverride tab_capture(
      extensions::FeatureSwitch::tab_capture(), true);
  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "api_tests.html")) << message_;
}

// TODO(miu): Remove this disabling of Win dbg runs once we confirm the test
// does not time-out on all other platforms.  See http://crbug.com/174640
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_EndToEnd DISABLED_EndToEnd
#else
#define MAYBE_EndToEnd EndToEnd
#endif

IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_EndToEnd) {
  extensions::FeatureSwitch::ScopedOverride tab_capture(
      extensions::FeatureSwitch::tab_capture(), true);
  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "end_to_end.html")) << message_;
}

// Test that we can't get tabCapture streams using GetUserMedia directly.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, GetUserMediaTest) {
  ExtensionTestMessageListener listener("ready", true);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture/experimental",
                                  "get_user_media_test.html")) << message_;

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  content::OpenURLParams params(GURL("about:blank"), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);

  content::RenderViewHost* const rvh = web_contents->GetRenderViewHost();
  int render_process_id = rvh->GetProcess()->GetID();
  int routing_id = rvh->GetRoutingID();

  listener.Reply(StringPrintf("%i:%i", render_process_id, routing_id));

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace chrome
