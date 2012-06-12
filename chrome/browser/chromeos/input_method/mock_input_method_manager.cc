// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"

namespace chromeos {
namespace input_method {

MockInputMethodManager::MockInputMethodManager()
    : add_observer_count_(0),
      remove_observer_count_(0),
      set_state_count_(0),
      last_state_(STATE_TERMINATING),
      util_(whitelist_.GetSupportedInputMethods()) {
}

MockInputMethodManager::~MockInputMethodManager() {
}

void MockInputMethodManager::AddObserver(
    InputMethodManager::Observer* observer) {
  ++add_observer_count_;
}

void MockInputMethodManager::AddCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {
}

void MockInputMethodManager::RemoveObserver(
    InputMethodManager::Observer* observer) {
  ++remove_observer_count_;
}

void MockInputMethodManager::RemoveCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {
}

void MockInputMethodManager::SetState(State new_state) {
  ++set_state_count_;
  last_state_ = new_state;
}

InputMethodDescriptors*
MockInputMethodManager::GetSupportedInputMethods() const {
  InputMethodDescriptors* result = new InputMethodDescriptors;
  result->push_back(
      InputMethodDescriptor::GetFallbackInputMethodDescriptor());
  return result;
}

InputMethodDescriptors* MockInputMethodManager::GetActiveInputMethods() const {
  InputMethodDescriptors* result = new InputMethodDescriptors;
  result->push_back(
      InputMethodDescriptor::GetFallbackInputMethodDescriptor());
  return result;
}

size_t MockInputMethodManager::GetNumActiveInputMethods() const {
  return 1;
}

void MockInputMethodManager::EnableLayouts(const std::string& language_code,
                                           const std::string& initial_layout) {
}

bool MockInputMethodManager::EnableInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  return true;
}

bool MockInputMethodManager::SetInputMethodConfig(
    const std::string& section,
    const std::string& config_name,
    const InputMethodConfigValue& value) {
  return true;
}

void MockInputMethodManager::ChangeInputMethod(
    const std::string& input_method_id) {
}

void MockInputMethodManager::ActivateInputMethodProperty(
    const std::string& key) {
}

void MockInputMethodManager::AddInputMethodExtension(
    const std::string& id,
    const std::string& name,
    const std::vector<std::string>& layouts,
    const std::string& language) {
}

void MockInputMethodManager::RemoveInputMethodExtension(const std::string& id) {
}

void MockInputMethodManager::EnableHotkeys() {
}

void MockInputMethodManager::DisableHotkeys() {
}

bool MockInputMethodManager::SwitchToNextInputMethod() {
  return true;
}

bool MockInputMethodManager::SwitchToPreviousInputMethod() {
  return true;
}

bool MockInputMethodManager::SwitchInputMethod(
    const ui::Accelerator& accelerator) {
  return true;
}

InputMethodDescriptor MockInputMethodManager::GetCurrentInputMethod() const {
  InputMethodDescriptor descriptor =
      InputMethodDescriptor::GetFallbackInputMethodDescriptor();
  if (!current_input_method_id_.empty()) {
    return InputMethodDescriptor(current_input_method_id_,
                                 descriptor.name(),
                                 descriptor.keyboard_layout(),
                                 descriptor.language_code(),
                                 false);
  }
  return descriptor;
}

InputMethodPropertyList
MockInputMethodManager::GetCurrentInputMethodProperties() const {
  return InputMethodPropertyList();
}

XKeyboard* MockInputMethodManager::GetXKeyboard() {
  return &xkeyboard_;
}

InputMethodUtil* MockInputMethodManager::GetInputMethodUtil() {
  return &util_;
}

}  // namespace input_method
}  // namespace chromeos
