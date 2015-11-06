// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_

#include <string>
#include <vector>

#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/ime_engine_handler_interface.h"

namespace ui {
class IMEEngineHandlerInterface;
class KeyEvent;

namespace ime {
struct InputMethodMenuItem;
}
}

namespace chromeos {

class CompositionText;

namespace input_method {
class CandidateWindow;
}

class MockInputMethodEngine : public ui::IMEEngineHandlerInterface {
 public:
  MockInputMethodEngine();
  ~MockInputMethodEngine() override;

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
  void FocusIn(
      const IMEEngineHandlerInterface::InputContext& input_context) override;
  void FocusOut() override;
  void Enable(const std::string& component_id) override;
  void Disable() override;
  void PropertyActivate(const std::string& property_name) override;
  void Reset() override;
  bool IsInterestedInKeyEvent() const override;
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback& callback) override;
  void CandidateClicked(uint32 index) override;
  void SetSurroundingText(const std::string& text,
                          uint32 cursor_pos,
                          uint32 anchor_pos,
                          uint32 offset_pos) override;
  void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) override;
  void HideInputView() override;

  std::string last_activated_property() const {
    return last_activated_property_;
  }

 private:
  std::string active_component_id_;

  // The current candidate window property.
  CandidateWindowProperty candidate_window_property_;

  std::string last_activated_property_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
