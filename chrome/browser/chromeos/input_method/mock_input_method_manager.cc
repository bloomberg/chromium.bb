// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"

namespace chromeos {
namespace input_method {

MockInputMethodManager::MockInputMethodManager()
    : add_observer_count_(0),
      remove_observer_count_(0),
      util_(&delegate_),
      mod3_used_(false) {
  active_input_method_ids_.push_back("xkb:us::eng");
}

MockInputMethodManager::~MockInputMethodManager() {
}

void MockInputMethodManager::InitializeComponentExtension() {
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
MockInputMethodManager::GetActiveInputMethods() const {
  scoped_ptr<InputMethodDescriptors> result(new InputMethodDescriptors);
  result->push_back(
      InputMethodUtil::GetFallbackInputMethodDescriptor());
  return result.Pass();
}

const std::vector<std::string>&
MockInputMethodManager::GetActiveInputMethodIds() const {
  return active_input_method_ids_;
}

size_t MockInputMethodManager::GetNumActiveInputMethods() const {
  return 1;
}

const InputMethodDescriptor* MockInputMethodManager::GetInputMethodFromId(
    const std::string& input_method_id) const {
  static const InputMethodDescriptor defaultInputMethod =
      InputMethodUtil::GetFallbackInputMethodDescriptor();
  for (size_t i = 0; i < active_input_method_ids_.size(); i++) {
    if (input_method_id == active_input_method_ids_[i]) {
      return &defaultInputMethod;
    }
  }
  return NULL;
}

void MockInputMethodManager::EnableLoginLayouts(
    const std::string& language_code,
    const std::vector<std::string>& initial_layout) {
}

bool MockInputMethodManager::ReplaceEnabledInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  return true;
}

bool MockInputMethodManager::EnableInputMethod(
    const std::string& new_active_input_method_id) {
  return true;
}

void MockInputMethodManager::ChangeInputMethod(
    const std::string& input_method_id) {
}

void MockInputMethodManager::ActivateInputMethodMenuItem(
    const std::string& key) {
}

void MockInputMethodManager::AddInputMethodExtension(
    const std::string& extension_id,
    const InputMethodDescriptors& descriptors,
    InputMethodEngineInterface* instance) {
}

void MockInputMethodManager::RemoveInputMethodExtension(
    const std::string& extension_id) {
}

void MockInputMethodManager::GetInputMethodExtensions(
    InputMethodDescriptors* result) {
}

void MockInputMethodManager::SetEnabledExtensionImes(
    std::vector<std::string>* ids) {
}

void MockInputMethodManager::SetInputMethodLoginDefault() {
}

void MockInputMethodManager::SetInputMethodLoginDefaultFromVPD(
    const std::string& locale, const std::string& layout) {
}

bool MockInputMethodManager::SwitchToNextInputMethod() {
  return true;
}

bool MockInputMethodManager::SwitchToPreviousInputMethod(
    const ui::Accelerator& accelerator) {
  return true;
}

bool MockInputMethodManager::SwitchInputMethod(
    const ui::Accelerator& accelerator) {
  return true;
}

InputMethodDescriptor MockInputMethodManager::GetCurrentInputMethod() const {
  InputMethodDescriptor descriptor =
      InputMethodUtil::GetFallbackInputMethodDescriptor();
  if (!current_input_method_id_.empty()) {
    return InputMethodDescriptor(current_input_method_id_,
                                 descriptor.name(),
                                 descriptor.indicator(),
                                 descriptor.keyboard_layouts(),
                                 descriptor.language_codes(),
                                 true,
                                 GURL(),  // options page url.
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

}  // namespace input_method
}  // namespace chromeos
