// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/dictation_chromeos.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/mock_ime_input_context_handler.h"

namespace chromeos {

namespace {

const char kFirstSpeechResult[] = "help";
const char kSecondSpeechResult[] = "help oh";
const char kFinalSpeechResult[] = "hello world";

}  // namespace

class DictationTest : public InProcessBrowserTest {
 protected:
  DictationTest() {
    input_context_handler_.reset(new ui::MockIMEInputContextHandler());
    empty_composition_text_ =
        ui::MockIMEInputContextHandler::UpdateCompositionTextArg()
            .composition_text;
  }
  ~DictationTest() override = default;

  void SetUpOnMainThread() override {
    ui::IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
  }

  AccessibilityManager* GetManager() { return AccessibilityManager::Get(); }

  void EnableChromeVox() { GetManager()->EnableSpokenFeedback(true); }

  void SendSpeechResult(const char* result, bool is_final) {
    GetManager()->dictation_->OnSpeechResult(base::ASCIIToUTF16(result),
                                             is_final);
  }

  ui::CompositionText GetLastCompositionText() {
    return input_context_handler_->last_update_composition_arg()
        .composition_text;
  }

  std::unique_ptr<ui::MockIMEInputContextHandler> input_context_handler_;
  ui::CompositionText empty_composition_text_;
};

IN_PROC_BROWSER_TEST_F(DictationTest, RecognitionEnds) {
  AccessibilityManager* manager = GetManager();

  manager->ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFirstSpeechResult, false /* is_final */);
  EXPECT_EQ(base::ASCIIToUTF16(kFirstSpeechResult),
            GetLastCompositionText().text);

  SendSpeechResult(kSecondSpeechResult, false /* is_final */);
  EXPECT_EQ(base::ASCIIToUTF16(kSecondSpeechResult),
            GetLastCompositionText().text);

  SendSpeechResult(kFinalSpeechResult, true /* is_final */);
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult, input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_F(DictationTest, RecognitionEndsWithChromeVoxEnabled) {
  AccessibilityManager* manager = GetManager();

  EnableChromeVox();
  EXPECT_TRUE(manager->IsSpokenFeedbackEnabled());

  manager->ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFirstSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kSecondSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFinalSpeechResult, true /* is_final */);
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult, input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_F(DictationTest, UserEndsDictation) {
  AccessibilityManager* manager = GetManager();

  manager->ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFinalSpeechResult, false /* is_final */);
  EXPECT_EQ(base::ASCIIToUTF16(kFinalSpeechResult),
            GetLastCompositionText().text);

  manager->ToggleDictation();
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult, input_context_handler_->last_commit_text());
}

IN_PROC_BROWSER_TEST_F(DictationTest, UserEndsDictationWhenChromeVoxEnabled) {
  AccessibilityManager* manager = GetManager();

  EnableChromeVox();
  EXPECT_TRUE(manager->IsSpokenFeedbackEnabled());

  manager->ToggleDictation();
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  SendSpeechResult(kFinalSpeechResult, false /* is_final */);
  EXPECT_EQ(GetLastCompositionText().text, empty_composition_text_.text);

  manager->ToggleDictation();
  EXPECT_EQ(1, input_context_handler_->commit_text_call_count());
  EXPECT_EQ(kFinalSpeechResult, input_context_handler_->last_commit_text());
}

}  // namespace chromeos
