// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_

#include <map>
#include <string>
#include <vector>
#include "base/time/time.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "url/gurl.h"

class Profile;

namespace ui {
class CandidateWindow;
class KeyEvent;
}  // namespace ui

namespace ash {
namespace ime {
struct InputMethodMenuItem;
}  // namespace ime
}  // namespace ash

namespace chromeos {

class CompositionText;

namespace input_method {
struct KeyEventHandle;
}  // namespace input_method

class InputMethodEngine : public InputMethodEngineInterface {
 public:
  InputMethodEngine();

  virtual ~InputMethodEngine();

  void Initialize(scoped_ptr<InputMethodEngineInterface::Observer> observer,
                  const char* extension_id);

  // InputMethodEngineInterface overrides.
  virtual const std::string& GetActiveComponentId() const override;
  virtual bool SetComposition(int context_id,
                              const char* text,
                              int selection_start,
                              int selection_end,
                              int cursor,
                              const std::vector<SegmentInfo>& segments,
                              std::string* error) override;
  virtual bool ClearComposition(int context_id, std::string* error) override;
  virtual bool CommitText(int context_id, const char* text,
                          std::string* error) override;
  virtual bool SendKeyEvents(int context_id,
                             const std::vector<KeyboardEvent>& events) override;
  virtual const CandidateWindowProperty&
    GetCandidateWindowProperty() const override;
  virtual void SetCandidateWindowProperty(
      const CandidateWindowProperty& property) override;
  virtual bool SetCandidateWindowVisible(bool visible,
                                         std::string* error) override;
  virtual bool SetCandidates(int context_id,
                             const std::vector<Candidate>& candidates,
                             std::string* error) override;
  virtual bool SetCursorPosition(int context_id, int candidate_id,
                                 std::string* error) override;
  virtual bool SetMenuItems(const std::vector<MenuItem>& items) override;
  virtual bool UpdateMenuItems(const std::vector<MenuItem>& items) override;
  virtual bool IsActive() const override;
  virtual bool DeleteSurroundingText(int context_id,
                                     int offset,
                                     size_t number_of_chars,
                                     std::string* error) override;

  // IMEEngineHandlerInterface overrides.
  virtual void FocusIn(
      const IMEEngineHandlerInterface::InputContext& input_context) override;
  virtual void FocusOut() override;
  virtual void Enable(const std::string& component_id) override;
  virtual void Disable() override;
  virtual void PropertyActivate(const std::string& property_name) override;
  virtual void Reset() override;
  virtual void ProcessKeyEvent(const ui::KeyEvent& key_event,
                               const KeyEventDoneCallback& callback) override;
  virtual void CandidateClicked(uint32 index) override;
  virtual void SetSurroundingText(const std::string& text, uint32 cursor_pos,
                                  uint32 anchor_pos) override;
  virtual void HideInputView() override;
  virtual void SetCompositionBounds(const gfx::Rect& bounds) override;

  int GetCotextIdForTesting() { return context_id_; }

 private:
  // Converts MenuItem to InputMethodMenuItem.
  void MenuItemToProperty(const MenuItem& item,
                          ash::ime::InputMethodMenuItem* property);

  // Enables overriding input view page to Virtual Keyboard window.
  void EnableInputView();

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
  scoped_ptr<InputMethodEngineInterface::Observer> observer_;

  // The current preedit text, and it's cursor position.
  scoped_ptr<CompositionText> composition_text_;
  int composition_cursor_;

  // The current candidate window.
  scoped_ptr<ui::CandidateWindow> candidate_window_;

  // The current candidate window property.
  CandidateWindowProperty candidate_window_property_;

  // Indicates whether the candidate window is visible.
  bool window_visible_;

  // Mapping of candidate index to candidate id.
  std::vector<int> candidate_ids_;

  // Mapping of candidate id to index.
  std::map<int, int> candidate_indexes_;

  // Used with SendKeyEvents and ProcessKeyEvent to check if the key event
  // sent to ProcessKeyEvent is sent by SendKeyEvents.
  const ui::KeyEvent* sent_key_event_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
