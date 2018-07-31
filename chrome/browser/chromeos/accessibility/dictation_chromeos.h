// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_DICTATION_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_DICTATION_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "ui/base/ime/ime_bridge_observer.h"

namespace ui {
struct CompositionText;
class IMEInputContextHandlerInterface;
}  // namespace ui

class Profile;
class SpeechRecognizer;

namespace chromeos {

// Provides global dictation (type what you speak) on Chrome OS.
class DictationChromeos : public SpeechRecognizerDelegate,
                          ui::IMEBridgeObserver {
 public:
  explicit DictationChromeos(Profile* profile);
  ~DictationChromeos() override;

  // User-initiated dictation.
  bool OnToggleDictation();

 private:
  // SpeechRecognizerDelegate:
  void OnSpeechResult(const base::string16& query, bool is_final) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;
  void GetSpeechAuthParameters(std::string* auth_scope,
                               std::string* auth_token) override;

  // IMEBridgeObserver
  void OnRequestSwitchEngine() override;

  // Saves current dictation result and stops listening.
  void DictationOff();

  std::unique_ptr<SpeechRecognizer> speech_recognizer_;
  std::unique_ptr<ui::CompositionText> composition_;
  ui::IMEInputContextHandlerInterface* input_context_;

  Profile* profile_;

  base::WeakPtrFactory<DictationChromeos> weak_ptr_factory_;

  friend class DictationTest;
  DISALLOW_COPY_AND_ASSIGN(DictationChromeos);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_DICTATION_CHROMEOS_H_
