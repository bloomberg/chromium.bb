// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INPUT_METHOD_INPUT_METHOD_ENGINE_BASE_H_
#define CHROME_BROWSER_INPUT_METHOD_INPUT_METHOD_ENGINE_BASE_H_

#include <map>
#include <string>
#include <vector>
#include "base/time/time.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/ime_engine_observer.h"
#include "url/gurl.h"

class Profile;

namespace ui {
struct CompositionText;
class IMEEngineHandlerInterface;
class IMEEngineObserver;
class KeyEvent;
}  // namespace ui

namespace input_method {

class InputMethodEngineBase : virtual public ui::IMEEngineHandlerInterface {
 public:
  InputMethodEngineBase();

  ~InputMethodEngineBase() override;

  void Initialize(scoped_ptr<ui::IMEEngineObserver> observer,
                  const char* extension_id,
                  Profile* profile);

  // IMEEngineHandlerInterface overrides.
  void FocusIn(const ui::IMEEngineHandlerInterface::InputContext& input_context)
      override;
  void FocusOut() override;
  void Enable(const std::string& component_id) override;
  void Disable() override;
  void Reset() override;
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback& callback) override;
  void SetSurroundingText(const std::string& text,
                          uint32_t cursor_pos,
                          uint32_t anchor_pos,
                          uint32_t offset_pos) override;
  void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetComposition(int context_id,
                      const char* text,
                      int selection_start,
                      int selection_end,
                      int cursor,
                      const std::vector<SegmentInfo>& segments,
                      std::string* error) override;
  bool ClearComposition(int context_id, std::string* error) override;
  bool CommitText(int context_id,
                  const char* text,
                  std::string* error) override;
  const std::string& GetActiveComponentId() const override;
  bool DeleteSurroundingText(int context_id,
                             int offset,
                             size_t number_of_chars,
                             std::string* error) override;
  int GetCotextIdForTesting() { return context_id_; }
  bool IsInterestedInKeyEvent() const override;

 protected:
  ui::TextInputType current_input_type_;

  // ID that is used for the current input context.  False if there is no focus.
  int context_id_;

  // Next id that will be assigned to a context.
  int next_context_id_;

  // The input_component ID in IME extension's manifest.
  std::string active_component_id_;

  // The IME extension ID.
  std::string extension_id_;

  // The observer object recieving events for this IME.
  scoped_ptr<ui::IMEEngineObserver> observer_;

  // The current preedit text, and it's cursor position.
  scoped_ptr<ui::CompositionText> composition_text_;
  int composition_cursor_;

  // Used with SendKeyEvents and ProcessKeyEvent to check if the key event
  // sent to ProcessKeyEvent is sent by SendKeyEvents.
  const ui::KeyEvent* sent_key_event_;

  Profile* profile_;
};

}  // namespace input_method

#endif  // CHROME_BROWSER_INPUT_METHOD_INPUT_METHOD_ENGINE_BASE_H_
