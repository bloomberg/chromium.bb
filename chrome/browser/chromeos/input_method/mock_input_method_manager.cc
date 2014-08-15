// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"

namespace chromeos {
namespace input_method {

MockInputMethodManager::State::State(MockInputMethodManager* manager)
    : manager_(manager) {
  active_input_method_ids.push_back("xkb:us::eng");
}

MockInputMethodManager::State::~State() {
}

MockInputMethodManager::MockInputMethodManager()
    : add_observer_count_(0),
      remove_observer_count_(0),
      state_(new State(this)),
      util_(&delegate_),
      mod3_used_(false) {
}

MockInputMethodManager::~MockInputMethodManager() {
}

InputMethodManager::UISessionState MockInputMethodManager::GetUISessionState() {
  return InputMethodManager::STATE_BROWSER_SCREEN;
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

scoped_ptr<InputMethodDescriptors>
MockInputMethodManager::GetSupportedInputMethods() const {
  scoped_ptr<InputMethodDescriptors> result(new InputMethodDescriptors);
  result->push_back(
      InputMethodUtil::GetFallbackInputMethodDescriptor());
  return result.Pass();
}

scoped_ptr<InputMethodDescriptors>
MockInputMethodManager::State::GetActiveInputMethods() const {
  scoped_ptr<InputMethodDescriptors> result(new InputMethodDescriptors);
  result->push_back(
      InputMethodUtil::GetFallbackInputMethodDescriptor());
  return result.Pass();
}

const std::vector<std::string>&
MockInputMethodManager::State::GetActiveInputMethodIds() const {
  return active_input_method_ids;
}

size_t MockInputMethodManager::State::GetNumActiveInputMethods() const {
  return 1;
}

const InputMethodDescriptor*
MockInputMethodManager::State::GetInputMethodFromId(
    const std::string& input_method_id) const {
  static const InputMethodDescriptor defaultInputMethod =
      InputMethodUtil::GetFallbackInputMethodDescriptor();
  for (size_t i = 0; i < active_input_method_ids.size(); i++) {
    if (input_method_id == active_input_method_ids[i]) {
      return &defaultInputMethod;
    }
  }
  return NULL;
}

void MockInputMethodManager::State::EnableLoginLayouts(
    const std::string& language_code,
    const std::vector<std::string>& initial_layout) {
}

void MockInputMethodManager::State::EnableLockScreenLayouts() {
}

bool MockInputMethodManager::State::ReplaceEnabledInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  return true;
}

bool MockInputMethodManager::State::EnableInputMethod(
    const std::string& new_active_input_method_id) {
  return true;
}

void MockInputMethodManager::State::ChangeInputMethod(
    const std::string& input_method_id,
    bool show_message) {
}

void MockInputMethodManager::ActivateInputMethodMenuItem(
    const std::string& key) {
}

void MockInputMethodManager::State::AddInputMethodExtension(
    const std::string& extension_id,
    const InputMethodDescriptors& descriptors,
    InputMethodEngineInterface* instance) {
}

void MockInputMethodManager::State::RemoveInputMethodExtension(
    const std::string& extension_id) {
}

void MockInputMethodManager::State::GetInputMethodExtensions(
    InputMethodDescriptors* result) {
}

void MockInputMethodManager::State::SetEnabledExtensionImes(
    std::vector<std::string>* ids) {
}

void MockInputMethodManager::State::SetInputMethodLoginDefault() {
}

void MockInputMethodManager::State::SetInputMethodLoginDefaultFromVPD(
    const std::string& locale,
    const std::string& layout) {
}

bool MockInputMethodManager::State::SwitchToNextInputMethod() {
  return true;
}

bool MockInputMethodManager::State::SwitchToPreviousInputMethod(
    const ui::Accelerator& accelerator) {
  return true;
}

bool MockInputMethodManager::State::SwitchInputMethod(
    const ui::Accelerator& accelerator) {
  return true;
}

InputMethodDescriptor MockInputMethodManager::State::GetCurrentInputMethod()
    const {
  InputMethodDescriptor descriptor =
      InputMethodUtil::GetFallbackInputMethodDescriptor();
  if (!current_input_method_id.empty()) {
    return InputMethodDescriptor(current_input_method_id,
                                 descriptor.name(),
                                 descriptor.indicator(),
                                 descriptor.keyboard_layouts(),
                                 descriptor.language_codes(),
                                 true,
                                 GURL(),   // options page url.
                                 GURL());  // input view page url.
  }
  return descriptor;
}

bool MockInputMethodManager::IsISOLevel5ShiftUsedByCurrentInputMethod() const {
  return mod3_used_;
}

bool MockInputMethodManager::IsAltGrUsedByCurrentInputMethod() const {
  return false;
}

ImeKeyboard* MockInputMethodManager::GetImeKeyboard() { return &keyboard_; }

InputMethodUtil* MockInputMethodManager::GetInputMethodUtil() {
  return &util_;
}

ComponentExtensionIMEManager*
    MockInputMethodManager::GetComponentExtensionIMEManager() {
  return comp_ime_manager_.get();
}

void MockInputMethodManager::SetComponentExtensionIMEManager(
    scoped_ptr<ComponentExtensionIMEManager> comp_ime_manager) {
  comp_ime_manager_ = comp_ime_manager.Pass();
}

void MockInputMethodManager::set_application_locale(const std::string& value) {
  delegate_.set_active_locale(value);
}

bool MockInputMethodManager::IsLoginKeyboard(
    const std::string& layout) const {
  return true;
}

bool MockInputMethodManager::MigrateInputMethods(
    std::vector<std::string>* input_method_ids) {
  return false;
}
scoped_refptr<InputMethodManager::State> MockInputMethodManager::CreateNewState(
    Profile* profile) {
  NOTIMPLEMENTED();
  return state_;
}

scoped_refptr<InputMethodManager::State>
MockInputMethodManager::GetActiveIMEState() {
  return scoped_refptr<InputMethodManager::State>(state_.get());
}

scoped_refptr<InputMethodManager::State> MockInputMethodManager::State::Clone()
    const {
  NOTIMPLEMENTED();
  return manager_->GetActiveIMEState();
}

void MockInputMethodManager::SetState(
    scoped_refptr<InputMethodManager::State> state) {
  state_ = scoped_refptr<MockInputMethodManager::State>(
      static_cast<MockInputMethodManager::State*>(state.get()));
}

void MockInputMethodManager::SetCurrentInputMethodId(
    const std::string& input_method_id) {
  state_->current_input_method_id = input_method_id;
}

}  // namespace input_method
}  // namespace chromeos
