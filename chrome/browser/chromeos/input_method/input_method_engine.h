// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include "base/time/time.h"
#include "chrome/browser/input_method/input_method_engine_base.h"
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

namespace input_method {
class InputMethodEngineBase;
}

namespace chromeos {

class InputMethodEngine : public ::input_method::InputMethodEngineBase {
 public:
  InputMethodEngine();

  ~InputMethodEngine() override;

  // IMEEngineHandlerInterface overrides.
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
  void Enable(const std::string& component_id) override;
  void PropertyActivate(const std::string& property_name) override;
  void CandidateClicked(uint32_t index) override;
  void HideInputView() override;

 private:
  // Converts MenuItem to InputMethodMenuItem.
  void MenuItemToProperty(const MenuItem& item,
                          ui::ime::InputMethodMenuItem* property);

  // Enables overriding input view page to Virtual Keyboard window.
  void EnableInputView();

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
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
