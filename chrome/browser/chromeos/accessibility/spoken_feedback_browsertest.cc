// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/speech_monitor.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/widget/widget.h"

using extensions::api::braille_display_private::StubBrailleController;

namespace chromeos {

//
// Spoken feedback tests only in a logged in user's window.
//

class LoggedInSpokenFeedbackTest : public InProcessBrowserTest {
 protected:
  LoggedInSpokenFeedbackTest() {}
  virtual ~LoggedInSpokenFeedbackTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    AccessibilityManager::SetBrailleControllerForTest(NULL);
  }

  void SendKeyPress(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(
        ASSERT_TRUE(
            ui_test_utils::SendKeyPressToWindowSync(
                NULL, key, false, false, false, false)));
  }

  void SendKeyPressWithControl(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(
        ASSERT_TRUE(
            ui_test_utils::SendKeyPressToWindowSync(
                NULL, key, true, false, false, false)));
  }

  void SendKeyPressWithSearchAndShift(ui::KeyboardCode key) {
    ASSERT_NO_FATAL_FAILURE(
        ASSERT_TRUE(
            ui_test_utils::SendKeyPressToWindowSync(
                NULL, key, false, true, false, true)));
  }

  void RunJavaScriptInChromeVoxBackgroundPage(const std::string& script) {
    extensions::ExtensionHost* host =
        extensions::ExtensionSystem::Get(browser()->profile())->
        process_manager()->GetBackgroundHostForExtension(
            extension_misc::kChromeVoxExtensionId);
    CHECK(content::ExecuteScript(host->host_contents(), script));
  }

  void SimulateTouchScreenInChromeVox() {
    // ChromeVox looks at whether 'ontouchstart' exists to know whether
    // or not it should respond to hover events. Fake it so that touch
    // exploration events get spoken.
    RunJavaScriptInChromeVoxBackgroundPage(
        "window.ontouchstart = function() {};");
  }

  void DisableEarcons() {
    // Playing earcons from within a test is not only annoying if you're
    // running the test locally, but seems to cause crashes
    // (http://crbug.com/396507). Work around this by just telling
    // ChromeVox to not ever play earcons (prerecorded sound effects).
    RunJavaScriptInChromeVoxBackgroundPage(
        "cvox.ChromeVox.earcons.playEarcon = function() {};");
  }

  void LoadChromeVoxAndThenNavigateToURL(const GURL& url) {
    // The goal of this helper function is to avoid race conditions between
    // the page loading and the ChromeVox extension loading and fully
    // initializing. To do this, we first load a test url that repeatedly
    // asks ChromeVox to speak 'ready', then we load ChromeVox and block
    // until we get that 'ready' speech.

    ui_test_utils::NavigateToURL(
        browser(),
        GURL("data:text/html;charset=utf-8,"
             "<script>"
             "window.setInterval(function() {"
             "  try {"
             "    cvox.Api.speak('ready');"
             "  } catch (e) {}"
             "}, 100);"
             "</script>"));
    EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
    AccessibilityManager::Get()->EnableSpokenFeedback(
        true, ash::A11Y_NOTIFICATION_NONE);

    // Block until we get "ready".
    while (speech_monitor_.GetNextUtterance() != "ready") {
    }

    // Now load the requested url.
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void PressRepeatedlyUntilUtterance(ui::KeyboardCode key,
                                     const std::string& expected_utterance) {
    // This helper function is needed when you want to poll for something
    // that happens asynchronously. Keep pressing |key|, until
    // the speech feedback that follows is |expected_utterance|.
    // Note that this doesn't work if pressing that key doesn't speak anything
    // at all before the asynchronous event occurred.
    while (true) {
      SendKeyPress(key);
      const std::string& utterance = speech_monitor_.GetNextUtterance();
      if (utterance == expected_utterance)
        break;
    }
  }

  SpeechMonitor speech_monitor_;

 private:
  StubBrailleController braille_controller_;
  DISALLOW_COPY_AND_ASSIGN(LoggedInSpokenFeedbackTest);
};

IN_PROC_BROWSER_TEST_F(LoggedInSpokenFeedbackTest, AddBookmark) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
  DisableEarcons();

  chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);

  // Create a bookmark with title "foo".
  chrome::ExecuteCommand(browser(), IDC_BOOKMARK_PAGE);
  EXPECT_EQ("Bookmark added!,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("about blank,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Bookmark name,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("text box", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_F);
  EXPECT_EQ("f", speech_monitor_.GetNextUtterance());
  SendKeyPress(ui::VKEY_O);
  EXPECT_EQ("o", speech_monitor_.GetNextUtterance());
  SendKeyPress(ui::VKEY_O);
  EXPECT_EQ("o", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_TAB);
  EXPECT_EQ("Bookmarks bar,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Bookmark folder,", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(MatchPattern(speech_monitor_.GetNextUtterance(), "combo box*"));

  SendKeyPress(ui::VKEY_RETURN);

  EXPECT_TRUE(MatchPattern(speech_monitor_.GetNextUtterance(), "*oolbar*"));
  // Wait for active window change to be announced to avoid interference from
  // that below.
  while (speech_monitor_.GetNextUtterance() != "window about blank tab") {
    // Do nothing.
  }

  // Focus bookmarks bar and listen for "foo".
  chrome::ExecuteCommand(browser(), IDC_FOCUS_BOOKMARKS);
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    VLOG(0) << "Got utterance: " << utterance;
    if (utterance == "Bookmarks,")
      break;
  }
  EXPECT_EQ("foo,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("button", speech_monitor_.GetNextUtterance());
}

//
// Spoken feedback tests in both a logged in browser window and guest mode.
//

enum SpokenFeedbackTestVariant {
  kTestAsNormalUser,
  kTestAsGuestUser
};

class SpokenFeedbackTest
    : public LoggedInSpokenFeedbackTest,
      public ::testing::WithParamInterface<SpokenFeedbackTestVariant> {
 protected:
  SpokenFeedbackTest() {}
  virtual ~SpokenFeedbackTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    if (GetParam() == kTestAsGuestUser) {
      command_line->AppendSwitch(chromeos::switches::kGuestSession);
      command_line->AppendSwitch(::switches::kIncognito);
      command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                      "user");
      command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                      chromeos::login::kGuestUserName);
    }
  }
};

INSTANTIATE_TEST_CASE_P(
    TestAsNormalAndGuestUser,
    SpokenFeedbackTest,
    ::testing::Values(kTestAsNormalUser,
                      kTestAsGuestUser));

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, EnableSpokenFeedback) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, FocusToolbar) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
  DisableEarcons();

  chrome::ExecuteCommand(browser(), IDC_FOCUS_TOOLBAR);
  // Might be "Google Chrome Toolbar" or "Chromium Toolbar".
  EXPECT_TRUE(MatchPattern(speech_monitor_.GetNextUtterance(), "*oolbar*"));
  EXPECT_EQ("Reload,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("button", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TypeInOmnibox) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
  DisableEarcons();

  // Wait for ChromeVox to finish speaking.
  chrome::ExecuteCommand(browser(), IDC_FOCUS_LOCATION);
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    VLOG(0) << "Got utterance: " << utterance;
    if (utterance == "text box")
      break;
  }

  SendKeyPress(ui::VKEY_X);
  EXPECT_EQ("x", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_Y);
  EXPECT_EQ("y", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_Z);
  EXPECT_EQ("z", speech_monitor_.GetNextUtterance());

  SendKeyPress(ui::VKEY_BACK);
  EXPECT_EQ("z", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxShiftSearch) {
  LoadChromeVoxAndThenNavigateToURL(
      GURL("data:text/html;charset=utf-8,<button autofocus>Click me</button>"));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (utterance == "Click me")
      break;
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());

  // Press Search+Shift+/ to enter ChromeVox's "find in page".
  SendKeyPressWithSearchAndShift(ui::VKEY_OEM_2);
  EXPECT_EQ("Find in page.", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Enter a search query.", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxPrefixKey) {
  LoadChromeVoxAndThenNavigateToURL(
      GURL("data:text/html;charset=utf-8,<button autofocus>Click me</button>"));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (utterance == "Click me")
      break;
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());

  // Press the prefix key Ctrl+';' followed by '/'
  // to enter ChromeVox's "find in page".
  SendKeyPressWithControl(ui::VKEY_OEM_1);
  SendKeyPress(ui::VKEY_OEM_2);
  EXPECT_EQ("Find in page.", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Enter a search query.", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxNavigateAndSelect) {
  LoadChromeVoxAndThenNavigateToURL(
      GURL("data:text/html;charset=utf-8,"
           "<h1>Title</h1>"
           "<button autofocus>Click me</button>"));
  while (true) {
    std::string utterance = speech_monitor_.GetNextUtterance();
    if (utterance == "Click me")
      break;
  }
  EXPECT_EQ("Button", speech_monitor_.GetNextUtterance());

  // Press Search+Shift+Up to navigate to the previous item.
  SendKeyPressWithSearchAndShift(ui::VKEY_UP);
  EXPECT_EQ("Title", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Heading 1", speech_monitor_.GetNextUtterance());

  // Press Search+Shift+S to select the text.
  SendKeyPressWithSearchAndShift(ui::VKEY_S);
  EXPECT_EQ("Start selection", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Title", speech_monitor_.GetNextUtterance());
  EXPECT_EQ(", selected", speech_monitor_.GetNextUtterance());

  // Press again to end the selection.
  SendKeyPressWithSearchAndShift(ui::VKEY_S);
  EXPECT_EQ("End selection", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Title", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, ChromeVoxStickyMode) {
  LoadChromeVoxAndThenNavigateToURL(
      GURL("data:text/html;charset=utf-8,"
           "<label>Enter your name <input autofocus></label>"
           "<p>One</p>"
           "<h2>Two</h2>"));
  while (speech_monitor_.GetNextUtterance() != "Enter your name") {
  }
  EXPECT_EQ("Edit text", speech_monitor_.GetNextUtterance());

  // Press the sticky-key sequence: Search Search.
  SendKeyPress(ui::VKEY_LWIN);
  SendKeyPress(ui::VKEY_LWIN);
  EXPECT_EQ("Sticky mode enabled", speech_monitor_.GetNextUtterance());

  // Even once we hear "sticky mode enabled" from the ChromeVox background
  // page, there's a short window of time when the content script still
  // hasn't switched to sticky mode. That's why we're focused on a text box.
  // Keep pressing the '/' key. If sticky mode is off, it will echo the word
  // "slash". If sticky mode is on, it will open "Find in page". Keep pressing
  // '/' until we get "Find in page.".
  PressRepeatedlyUntilUtterance(ui::VKEY_OEM_2, "Find in page.");
  EXPECT_EQ("Enter a search query.", speech_monitor_.GetNextUtterance());

  // Press Esc to exit Find in Page mode.
  SendKeyPress(ui::VKEY_ESCAPE);
  EXPECT_EQ("Exited", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("Find in page.", speech_monitor_.GetNextUtterance());

  // Press N H to jump to the next heading. Skip over speech in-between
  // but make sure we end up at the heading.
  SendKeyPress(ui::VKEY_N);
  SendKeyPress(ui::VKEY_H);
  while (speech_monitor_.GetNextUtterance() != "Two") {
  }
  EXPECT_EQ("Heading 2", speech_monitor_.GetNextUtterance());

  // Press the up arrow to go to the previous element.
  SendKeyPress(ui::VKEY_UP);
  EXPECT_EQ("One", speech_monitor_.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_P(SpokenFeedbackTest, TouchExploreStatusTray) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
  DisableEarcons();

  SimulateTouchScreenInChromeVox();

  // Send an accessibility hover event on the system tray, which is
  // what we get when you tap it on a touch screen when ChromeVox is on.
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();
  tray->NotifyAccessibilityEvent(ui::AX_EVENT_HOVER, true);

  EXPECT_EQ("Status tray,", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(MatchPattern(speech_monitor_.GetNextUtterance(), "time*,"));
  EXPECT_TRUE(MatchPattern(speech_monitor_.GetNextUtterance(), "Battery*,"));
  EXPECT_EQ("button", speech_monitor_.GetNextUtterance());
}

//
// Spoken feedback tests that run only in guest mode.
//

class GuestSpokenFeedbackTest : public LoggedInSpokenFeedbackTest {
 protected:
  GuestSpokenFeedbackTest() {}
  virtual ~GuestSpokenFeedbackTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    chromeos::login::kGuestUserName);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestSpokenFeedbackTest);
};

IN_PROC_BROWSER_TEST_F(GuestSpokenFeedbackTest, FocusToolbar) {
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());
  DisableEarcons();

  chrome::ExecuteCommand(browser(), IDC_FOCUS_TOOLBAR);
  // Might be "Google Chrome Toolbar" or "Chromium Toolbar".
  EXPECT_TRUE(MatchPattern(speech_monitor_.GetNextUtterance(), "*oolbar*"));
  EXPECT_EQ("Reload,", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("button", speech_monitor_.GetNextUtterance());
}

//
// Spoken feedback tests of the out-of-box experience.
//

class OobeSpokenFeedbackTest : public InProcessBrowserTest {
 protected:
  OobeSpokenFeedbackTest() {}
  virtual ~OobeSpokenFeedbackTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    AccessibilityManager::Get()->
        SetProfileForTest(ProfileHelper::GetSigninProfile());
  }

  SpeechMonitor speech_monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeSpokenFeedbackTest);
};

// Test is flaky: http://crbug.com/346797
IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest, DISABLED_SpokenFeedbackInOobe) {
  ui_controls::EnableUIControls();
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  LoginDisplayHost* login_display_host = LoginDisplayHostImpl::default_host();
  WebUILoginView* web_ui_login_view = login_display_host->GetWebUILoginView();
  views::Widget* widget = web_ui_login_view->GetWidget();
  gfx::NativeWindow window = widget->GetNativeWindow();

  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(speech_monitor_.SkipChromeVoxEnabledMessage());

  EXPECT_EQ("Select your language:", speech_monitor_.GetNextUtterance());
  EXPECT_EQ("English ( United States)", speech_monitor_.GetNextUtterance());
  EXPECT_TRUE(MatchPattern(speech_monitor_.GetNextUtterance(),
                           "Combo box * of *"));
  ASSERT_TRUE(
      ui_test_utils::SendKeyPressToWindowSync(
          window, ui::VKEY_TAB, false, false, false, false));
  EXPECT_EQ("Select your keyboard:", speech_monitor_.GetNextUtterance());
}

}  // namespace chromeos
