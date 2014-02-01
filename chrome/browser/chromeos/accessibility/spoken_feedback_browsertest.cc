// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/widget/widget.h"

using extensions::api::braille_display_private::StubBrailleController;

namespace chromeos {

namespace {
const char kChromeVoxEnabledMessage[] = "chrome vox spoken feedback is ready";
}  // anonymous namespace

// Installs itself as the platform speech synthesis engine, allowing it to
// intercept all speech calls, and then provides a method to block until the
// next utterance is spoken.
class SpeechMonitor : public TtsPlatformImpl {
 public:
  SpeechMonitor();
  virtual ~SpeechMonitor();

  // Blocks until the next utterance is spoken, and returns its text.
  std::string GetNextUtterance();

  // TtsPlatformImpl implementation.
  virtual bool PlatformImplAvailable() OVERRIDE { return true; }
  virtual bool Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params) OVERRIDE {
    TtsController::GetInstance()->OnTtsEvent(
        utterance_id,
        TTS_EVENT_END,
        static_cast<int>(utterance.size()),
        std::string());
    return true;
  }
  virtual bool StopSpeaking() OVERRIDE { return true; }
  virtual bool IsSpeaking() OVERRIDE { return false; }
  virtual void GetVoices(std::vector<VoiceData>* out_voices) OVERRIDE {
    out_voices->push_back(VoiceData());
    VoiceData& voice = out_voices->back();
    voice.native = true;
    voice.name = "SpeechMonitor";
    voice.events.insert(TTS_EVENT_END);
  }
  virtual void Pause() OVERRIDE {}
  virtual void Resume() OVERRIDE {}
  virtual std::string error() OVERRIDE { return ""; }
  virtual void clear_error() OVERRIDE {}
  virtual void set_error(const std::string& error) OVERRIDE {}
  virtual void WillSpeakUtteranceWithVoice(
      const Utterance* utterance, const VoiceData& voice_data) OVERRIDE;

 private:
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  std::deque<std::string> utterance_queue_;

  DISALLOW_COPY_AND_ASSIGN(SpeechMonitor);
};

SpeechMonitor::SpeechMonitor() {
  TtsController::GetInstance()->SetPlatformImpl(this);
}

SpeechMonitor::~SpeechMonitor() {
  TtsController::GetInstance()->SetPlatformImpl(TtsPlatformImpl::GetInstance());
}

void SpeechMonitor::WillSpeakUtteranceWithVoice(const Utterance* utterance,
                                                const VoiceData& voice_data) {
  utterance_queue_.push_back(utterance->text());
  if (loop_runner_.get())
    loop_runner_->Quit();
}

std::string SpeechMonitor::GetNextUtterance() {
  if (utterance_queue_.empty()) {
    loop_runner_ = new content::MessageLoopRunner();
    loop_runner_->Run();
    loop_runner_ = NULL;
  }
  std::string result = utterance_queue_.front();
  utterance_queue_.pop_front();
  return result;
}

//
// Spoken feedback tests in a normal browser window.
//

class SpokenFeedbackTest : public InProcessBrowserTest {
 protected:
  SpokenFeedbackTest() {}
  virtual ~SpokenFeedbackTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    AccessibilityManager::SetBrailleControllerForTest(&braille_controller_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    AccessibilityManager::SetBrailleControllerForTest(NULL);
  }

 private:
  StubBrailleController braille_controller_;
  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackTest);
};

IN_PROC_BROWSER_TEST_F(SpokenFeedbackTest, EnableSpokenFeedback) {
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  SpeechMonitor monitor;
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_EQ(kChromeVoxEnabledMessage, monitor.GetNextUtterance());
}

IN_PROC_BROWSER_TEST_F(SpokenFeedbackTest, FocusToolbar) {
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  SpeechMonitor monitor;
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_EQ(kChromeVoxEnabledMessage, monitor.GetNextUtterance());

  chrome::ExecuteCommand(browser(), IDC_FOCUS_TOOLBAR);
  // Might be "Google Chrome Toolbar" or "Chromium Toolbar".
  EXPECT_TRUE(MatchPattern(monitor.GetNextUtterance(), "*oolbar*"));
  EXPECT_EQ("Reload,", monitor.GetNextUtterance());
  EXPECT_EQ("button", monitor.GetNextUtterance());
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

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeSpokenFeedbackTest);
};

IN_PROC_BROWSER_TEST_F(OobeSpokenFeedbackTest, SpokenFeedbackInOobe) {
  ui_controls::EnableUIControls();
  EXPECT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  LoginDisplayHost* login_display_host = LoginDisplayHostImpl::default_host();
  WebUILoginView* web_ui_login_view = login_display_host->GetWebUILoginView();
  views::Widget* widget = web_ui_login_view->GetWidget();
  gfx::NativeWindow window = widget->GetNativeWindow();

  SpeechMonitor monitor;
  AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_EQ(kChromeVoxEnabledMessage, monitor.GetNextUtterance());

  EXPECT_EQ("Select your language:", monitor.GetNextUtterance());
  EXPECT_EQ("English ( United States)", monitor.GetNextUtterance());
  EXPECT_TRUE(MatchPattern(monitor.GetNextUtterance(), "Combo box * of *"));
  ui_controls::SendKeyPress(window, ui::VKEY_TAB, false, false, false, false);
  EXPECT_EQ("Select your keyboard:", monitor.GetNextUtterance());
}

}  // namespace chromeos
