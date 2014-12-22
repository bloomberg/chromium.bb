// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace extensions {

namespace {

const char kExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

class TabCaptureApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Specify smallish window size to make testing of tab capture less CPU
    // intensive.
    command_line->AppendSwitchASCII(::switches::kWindowSize, "300,300");
  }

  void AddExtensionToCommandLineWhitelist() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, kExtensionId);
  }

 protected:
  void SimulateMouseClickInCurrentTab() {
    content::SimulateMouseClick(
        browser()->tab_strip_model()->GetActiveWebContents(),
        0,
        blink::WebMouseEvent::ButtonLeft);
  }
};

class TabCaptureApiPixelTest : public TabCaptureApiTest {
 public:
  void SetUp() override {
    if (!IsTooIntensiveForThisPlatform())
      EnablePixelOutput();
    TabCaptureApiTest::SetUp();
  }

 protected:
  bool IsTooIntensiveForThisPlatform() const {
#if defined(OS_WIN)
    if (base::win::GetVersion() < base::win::VERSION_VISTA)
      return true;
#endif

    // The tests are too slow to succeed with OSMesa on the bots.
    if (UsingOSMesa())
      return true;

#if defined(NDEBUG)
    return false;
#else
    // TODO(miu): Look into enabling these tests for the Debug build bots once
    // they prove to be stable again on the Release bots.
    // http://crbug.com/396413
    return !base::CommandLine::ForCurrentProcess()->HasSwitch(
        "run-tab-capture-api-pixel-tests");
#endif
  }
};

// Tests API behaviors, including info queries, and constraints violations.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, ApiTests) {
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "api_tests.html")) << message_;
}

// Tests that tab capture video frames can be received in a VIDEO element.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPixelTest, EndToEndWithoutRemoting) {
  if (IsTooIntensiveForThisPlatform()) {
    LOG(WARNING) << "Skipping this CPU-intensive test on this platform/build.";
    return;
  }
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest(
      "tab_capture", "end_to_end.html?method=local&colorDeviation=10"))
      << message_;
}

// Tests that video frames are captured, transported via WebRTC, and finally
// received in a VIDEO element.  More allowance is provided for color deviation
// because of the additional layers of video processing performed within
// WebRTC.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPixelTest, EndToEndThroughWebRTC) {
  if (IsTooIntensiveForThisPlatform()) {
    LOG(WARNING) << "Skipping this CPU-intensive test on this platform/build.";
    return;
  }
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest(
      "tab_capture", "end_to_end.html?method=webrtc&colorDeviation=50"))
      << message_;
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_GetUserMediaTest DISABLED_GetUserMediaTest
#else
#define MAYBE_GetUserMediaTest GetUserMediaTest
#endif
// Tests that getUserMedia() is NOT a way to start tab capture.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_GetUserMediaTest) {
  ExtensionTestMessageListener listener("ready", true);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "get_user_media_test.html"))
      << message_;

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  content::OpenURLParams params(GURL("about:blank"), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);

  content::RenderFrameHost* const main_frame = web_contents->GetMainFrame();
  ASSERT_TRUE(main_frame);
  listener.Reply(base::StringPrintf("web-contents-media-stream://%i:%i",
                                    main_frame->GetProcess()->GetID(),
                                    main_frame->GetRoutingID()));

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_ActiveTabPermission DISABLED_ActiveTabPermission
#else
#define MAYBE_ActiveTabPermission ActiveTabPermission
#endif
// Make sure tabCapture.capture only works if the tab has been granted
// permission via an extension icon click or the extension is whitelisted.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_ActiveTabPermission) {
  ExtensionTestMessageListener before_open_tab("ready1", true);
  ExtensionTestMessageListener before_grant_permission("ready2", true);
  ExtensionTestMessageListener before_open_new_tab("ready3", true);
  ExtensionTestMessageListener before_whitelist_extension("ready4", true);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture",
                                  "active_tab_permission_test.html"))
      << message_;

  // Open a new tab and make sure capture is denied.
  EXPECT_TRUE(before_open_tab.WaitUntilSatisfied());
  content::OpenURLParams params(GURL("http://google.com"), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  before_open_tab.Reply("");

  // Grant permission and make sure capture succeeds.
  EXPECT_TRUE(before_grant_permission.WaitUntilSatisfied());
  const Extension* extension = ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          kExtensionId);
  TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter()->GrantIfRequested(extension);
  before_grant_permission.Reply("");

  // Open a new tab and make sure capture is denied.
  EXPECT_TRUE(before_open_new_tab.WaitUntilSatisfied());
  browser()->OpenURL(params);
  before_open_new_tab.Reply("");

  // Add extension to whitelist and make sure capture succeeds.
  EXPECT_TRUE(before_whitelist_extension.WaitUntilSatisfied());
  AddExtensionToCommandLineWhitelist();
  before_whitelist_extension.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_FullscreenEvents DISABLED_FullscreenEvents
#else
#define MAYBE_FullscreenEvents FullscreenEvents
#endif
// Tests that fullscreen transitions during a tab capture session dispatch
// events to the onStatusChange listener.  The test loads a page that toggles
// fullscreen mode, using the Fullscreen Javascript API, in response to mouse
// clicks.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_FullscreenEvents) {
  AddExtensionToCommandLineWhitelist();

  ExtensionTestMessageListener capture_started("tab_capture_started", false);
  ExtensionTestMessageListener entered_fullscreen("entered_fullscreen", false);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "fullscreen_test.html"))
      << message_;
  EXPECT_TRUE(capture_started.WaitUntilSatisfied());

  // Click on the page to trigger the Javascript that will toggle the tab into
  // fullscreen mode.
  SimulateMouseClickInCurrentTab();
  EXPECT_TRUE(entered_fullscreen.WaitUntilSatisfied());

  // Click again to exit fullscreen mode.
  SimulateMouseClickInCurrentTab();

  // Wait until the page examines its results and calls chrome.test.succeed().
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Times out on Win dbg bots: http://crbug.com/177163
// Flaky on MSan bots: http://crbug.com/294431
#if (defined(OS_WIN) && !defined(NDEBUG)) || defined(MEMORY_SANITIZER)
#define MAYBE_GrantForChromePages DISABLED_GrantForChromePages
#else
#define MAYBE_GrantForChromePages GrantForChromePages
#endif
// Make sure tabCapture API can be granted for Chrome:// pages.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_GrantForChromePages) {
  ExtensionTestMessageListener before_open_tab("ready1", true);
  ASSERT_TRUE(RunExtensionSubtest("tab_capture",
                                  "active_tab_chrome_pages.html"))
      << message_;
  EXPECT_TRUE(before_open_tab.WaitUntilSatisfied());

  // Open a tab on a chrome:// page and make sure we can capture.
  content::OpenURLParams params(GURL("chrome://version"), content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  const Extension* extension = ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          kExtensionId);
  TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter()->GrantIfRequested(extension);
  before_open_tab.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_CaptureInSplitIncognitoMode DISABLED_CaptureInSplitIncognitoMode
#else
#define MAYBE_CaptureInSplitIncognitoMode CaptureInSplitIncognitoMode
#endif
// Tests that a tab in incognito mode can be captured.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_CaptureInSplitIncognitoMode) {
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture",
                                  "start_tab_capture.html",
                                  kFlagEnableIncognito | kFlagUseIncognito))
      << message_;
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_Constraints DISABLED_Constraints
#else
#define MAYBE_Constraints Constraints
#endif
// Tests that valid constraints allow tab capture to start, while invalid ones
// do not.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_Constraints) {
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "constraints.html"))
      << message_;
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TabIndicator DISABLED_TabIndicator
#else
#define MAYBE_TabIndicator TabIndicator
#endif
// Tests that the tab indicator (in the tab strip) is shown during tab capture.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_TabIndicator) {
  ASSERT_EQ(TAB_MEDIA_STATE_NONE,
            chrome::GetTabMediaStateForContents(
                browser()->tab_strip_model()->GetActiveWebContents()));

  // Run an extension test that just turns on tab capture, which should cause
  // the indicator to turn on.
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "start_tab_capture.html"))
      << message_;

  // A TabStripModelObserver that quits the MessageLoop whenever the UI's model
  // is sent an event that changes the indicator status.
  class IndicatorChangeObserver : public TabStripModelObserver {
   public:
    explicit IndicatorChangeObserver(Browser* browser)
        : last_media_state_(chrome::GetTabMediaStateForContents(
              browser->tab_strip_model()->GetActiveWebContents())) {}

    TabMediaState last_media_state() const { return last_media_state_; }

    void TabChangedAt(content::WebContents* contents,
                      int index,
                      TabChangeType change_type) override {
      const TabMediaState media_state =
          chrome::GetTabMediaStateForContents(contents);
      const bool has_changed = media_state != last_media_state_;
      last_media_state_ = media_state;
      if (has_changed) {
        base::MessageLoop::current()->PostTask(
            FROM_HERE, base::MessageLoop::QuitClosure());
      }
    }

   private:
    TabMediaState last_media_state_;
  };

  // Run the browser until the indicator turns on.
  IndicatorChangeObserver observer(browser());
  browser()->tab_strip_model()->AddObserver(&observer);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (observer.last_media_state() != TAB_MEDIA_STATE_CAPTURING) {
    if (base::TimeTicks::Now() - start_time >
            base::TimeDelta::FromSeconds(10)) {
      EXPECT_EQ(TAB_MEDIA_STATE_CAPTURING, observer.last_media_state());
      browser()->tab_strip_model()->RemoveObserver(&observer);
      return;
    }
    content::RunMessageLoop();
  }
  browser()->tab_strip_model()->RemoveObserver(&observer);
}

}  // namespace

}  // namespace extensions
