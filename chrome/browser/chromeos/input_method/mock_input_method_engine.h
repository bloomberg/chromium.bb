// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"
#include "chromeos/ime/input_method_descriptor.h"

namespace ui {
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
class CandidateWindow;
struct KeyEventHandle;
}  // namespace input_method

class MockInputMethodEngine : public InputMethodEngineInterface {
 public:
  MockInputMethodEngine();
  virtual ~MockInputMethodEngine();

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
  virtual void SetCompositionBounds(const gfx::Rect& bounds) override;
  virtual void HideInputView() override;

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
