// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_

#include <string>

#include "chrome/browser/chromeos/input_method/emoji_suggester.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/personal_info_suggester.h"
#include "chrome/browser/chromeos/input_method/suggester.h"
#include "chrome/browser/chromeos/input_method/suggestion_enums.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"

namespace chromeos {

// Check if any assistive feature is enabled.
bool IsAssistiveFeatureEnabled();

// An agent to suggest assistive information when the user types, and adopt or
// dismiss the suggestion according to the user action.
class AssistiveSuggester {
 public:
  AssistiveSuggester(InputMethodEngine* engine, Profile* profile);

  // Called when a text field gains focus, and suggester starts working.
  void OnFocus(int context_id);

  // Called when a text field loses focus, and suggester stops working.
  void OnBlur();

  // Checks the text before cursor, emits metric if any assistive prefix is
  // matched.
  void RecordAssistiveCoverageMetrics(const base::string16& text,
                                      int cursor_pos,
                                      int anchor_pos);

  // Called when a surrounding text is changed.
  // Returns true if it changes the surrounding text, e.g. a suggestion is
  // generated or dismissed.
  bool OnSurroundingTextChanged(const base::string16& text,
                                int cursor_pos,
                                int anchor_pos);

  // Called when the user pressed a key.
  // Returns true if suggester handles the event and it should stop propagate.
  bool OnKeyEvent(
      const ::input_method::InputMethodEngineBase::KeyboardEvent& event);

 private:
  // Returns if any suggestion text should be displayed according to the
  // surrounding text information.
  bool Suggest(const base::string16& text, int cursor_pos, int anchor_pos);

  void DismissSuggestion();

  // Check if suggestion is being shown.
  bool IsSuggestionShown();

  PersonalInfoSuggester personal_info_suggester_;
  EmojiSuggester emoji_suggester_;

  // ID of the focused text field, 0 if none is focused.
  int context_id_ = -1;

  // The current suggester in use, nullptr means no suggestion is shown.
  Suggester* current_suggester_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_ASSISTIVE_SUGGESTER_H_
