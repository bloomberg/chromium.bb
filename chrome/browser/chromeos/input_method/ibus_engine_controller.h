// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_ENGINE_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_ENGINE_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"

#include <string>
#include <vector>

namespace chromeos {
namespace input_method {

struct KeyEventHandle;

// IBusEngineController is used to encapsulate an ibus engine.
class IBusEngineController {
 public:
  class Observer {
   public:
    // Called when a key is pressed or released.
    virtual void OnKeyEvent(bool key_press, unsigned int keyval,
                            unsigned int keycode, bool alt_key,
                            bool ctrl_key, bool shift_key,
                            KeyEventHandle* key_data) = 0;

    // Called when the engine should reset its internal state.
    virtual void OnReset() = 0;

    // Called when the engine become the current engine.
    virtual void OnEnable() = 0;

    // Called when the engine is being disabled.
    virtual void OnDisable() = 0;

    // Called when a text field is focused.
    virtual void OnFocusIn() = 0;

    // Called when focus leaves a text field.
    virtual void OnFocusOut() = 0;

    // Called when one of the engine's menus is interacted with.
    virtual void OnPropertyActivate(const char *prop_name,
                                    unsigned int prop_state) = 0;

    // Called when a candidate in the candidate window is clicked on.
    virtual void OnCandidateClicked(unsigned int index, unsigned int button,
                                    unsigned int state) = 0;
  };

  struct Candidate {
    std::string value;
    std::string label;
    std::string annotation;
  };

  // Constants for the button parameter of OnCandidateClicked
  enum {
    MOUSE_BUTTON_1_MASK = 0x01,
    MOUSE_BUTTON_2_MASK = 0x02,
    MOUSE_BUTTON_3_MASK = 0x04,
  };

  // Constants for the type argument of SetPreeditUnderline
  enum {
    UNDERLINE_NONE,
    UNDERLINE_SINGLE,
    UNDERLINE_DOUBLE,
    UNDERLINE_LOW,
    UNDERLINE_ERROR
  };

  static IBusEngineController* Create(Observer* observer,
                                      const char* engine_id,
                                      const char* engine_name,
                                      const char* description,
                                      const char* language,
                                      const char* layout);

  virtual ~IBusEngineController();

  // Set the preedit text.
  virtual void SetPreeditText(const char* text, int cursor) = 0;

  // Adds an underline to the preedit text.  Can be called multiple times.
  virtual void SetPreeditUnderline(int start, int end, int type) = 0;

  // Commit the provided text to the current input box.
  virtual void CommitText(const char* text) = 0;

  // Show or hide the candidate window.
  virtual void SetTableVisible(bool visible) = 0;

  // Show or hide the cursor in the candidate window.
  virtual void SetCursorVisible(bool visible) = 0;

  // Change the orientation of the candidate window.
  virtual void SetOrientationVertical(bool vertical) = 0;

  // Set the number of candidates displayed in the candidate window.
  virtual void SetPageSize(unsigned int size) = 0;

  // Remove all candidates.
  virtual void ClearCandidates() = 0;

  // Set the list of candidates in the candidate window.
  virtual void SetCandidates(std::vector<Candidate> candidates) = 0;

  // Set the text displayed at the bottom of the candidate window.
  virtual void SetCandidateAuxText(const char* text) = 0;

  // Show or hide the text at the bottom of the candidate window.
  virtual void SetCandidateAuxTextVisible(bool visible) = 0;

  // Set the posistion of the cursor in the candidate window.
  virtual void SetCursorPosition(unsigned int position) = 0;

  // Inform the engine that a key event has been processed.
  virtual void KeyEventDone(KeyEventHandle* key_data, bool handled) = 0;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_ENGINE_CONTROLLER_H_
