// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_

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
class CandidateWindow;
struct CompositionText;
class IMEEngineHandlerInterface;
class IMEEngineObserver;
class KeyEvent;

namespace ime {
struct InputMethodMenuItem;
}  // namespace ime
}  // namespace ui

namespace chromeos {

class InputMethodEngine : public ui::IMEEngineHandlerInterface {
 public:
  InputMethodEngine();

  ~InputMethodEngine() override;

  void Initialize(scoped_ptr<ui::IMEEngineObserver> observer,
                  const char* extension_id,
                  Profile* profile);

  // IMEEngineHandlerInterface overrides.
  const std::string& GetActiveComponentId() const override;
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
  bool SendKeyEvents(int context_id,
                     const std::vector<KeyboardEvent>& events) override;
  const CandidateWindowProperty& GetCandidateWindowProperty() const override;
  void SetCandidateWindowProperty(
      const CandidateWindowProperty& property) override;
  bool SetCandidateWindowVisible(bool visible, std::string* error) override;
  bool SetCandidates(int context_id,
                     const std::vector<Candidate>& candidates,
                     std::string* error) override;
  bool SetCursorPosition(int context_id,
                         int candidate_id,
                         std::string* error) override;
  bool SetMenuItems(const std::vector<MenuItem>& items) override;
  bool UpdateMenuItems(const std::vector<MenuItem>& items) override;
  bool IsActive() const override;
  bool DeleteSurroundingText(int context_id,
                             int offset,
                             size_t number_of_chars,
                             std::string* error) override;

  // IMEEngineHandlerInterface overrides.
  void FocusIn(const ui::IMEEngineHandlerInterface::InputContext& input_context)
      override;
  void FocusOut() override;
  void Enable(const std::string& component_id) override;
  void Disable() override;
  void PropertyActivate(const std::string& property_name) override;
  void Reset() override;
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback& callback) override;
  void CandidateClicked(uint32 index) override;
  void SetSurroundingText(const std::string& text,
                          uint32 cursor_pos,
                          uint32 anchor_pos,
                          uint32 offset_pos) override;
  void HideInputView() override;
  void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) override;

  int GetCotextIdForTesting() { return context_id_; }

  bool IsInterestedInKeyEvent() const override;

 private:
  bool CheckProfile() const;
  // Converts MenuItem to InputMethodMenuItem.
  void MenuItemToProperty(const MenuItem& item,
                          ui::ime::InputMethodMenuItem* property);

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
  scoped_ptr<ui::IMEEngineObserver> observer_;

  // The current preedit text, and it's cursor position.
  scoped_ptr<ui::CompositionText> composition_text_;
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

  Profile* profile_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
