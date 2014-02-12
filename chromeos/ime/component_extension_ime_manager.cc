// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/component_extension_ime_manager.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromeos/ime/extension_ime_util.h"

namespace chromeos {

ComponentExtensionEngine::ComponentExtensionEngine() {
}

ComponentExtensionEngine::~ComponentExtensionEngine() {
}

ComponentExtensionIME::ComponentExtensionIME() {
}

ComponentExtensionIME::~ComponentExtensionIME() {
}

ComponentExtensionIMEManagerDelegate::ComponentExtensionIMEManagerDelegate() {
}

ComponentExtensionIMEManagerDelegate::~ComponentExtensionIMEManagerDelegate() {
}

ComponentExtensionIMEManager::ComponentExtensionIMEManager()
    : is_initialized_(false) {
}

ComponentExtensionIMEManager::~ComponentExtensionIMEManager() {
}

void ComponentExtensionIMEManager::Initialize(
    scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate) {
  delegate_ = delegate.Pass();
  component_extension_imes_ = delegate_->ListIME();
  is_initialized_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnInitialized());
}

bool ComponentExtensionIMEManager::IsInitialized() {
  return is_initialized_;
}

bool ComponentExtensionIMEManager::LoadComponentExtensionIME(
    const std::string& input_method_id) {
  ComponentExtensionIME ime;
  if (FindEngineEntry(input_method_id, &ime, NULL))
    return delegate_->Load(ime.id, ime.manifest, ime.path);
  else
    return false;
}

bool ComponentExtensionIMEManager::UnloadComponentExtensionIME(
    const std::string& input_method_id) {
  ComponentExtensionIME ime;
  if (FindEngineEntry(input_method_id, &ime, NULL))
    return delegate_->Unload(ime.id, ime.path);
  else
    return false;
}

bool ComponentExtensionIMEManager::IsWhitelisted(
    const std::string& input_method_id) {
  return extension_ime_util::IsComponentExtensionIME(input_method_id) &&
      FindEngineEntry(input_method_id, NULL, NULL);
}

bool ComponentExtensionIMEManager::IsWhitelistedExtension(
    const std::string& extension_id) {
  for (size_t i = 0; i < component_extension_imes_.size(); ++i) {
    if (component_extension_imes_[i].id == extension_id)
      return true;
  }
  return false;
}

std::string ComponentExtensionIMEManager::GetId(
    const std::string& extension_id,
    const std::string& engine_id) {
  ComponentExtensionEngine engine;
  const std::string& input_method_id =
      extension_ime_util::GetComponentInputMethodID(extension_id, engine_id);
  if (!FindEngineEntry(input_method_id, NULL, &engine))
    return "";
  return input_method_id;
}

std::string ComponentExtensionIMEManager::GetName(
    const std::string& input_method_id) {
  ComponentExtensionEngine engine;
  if (!FindEngineEntry(input_method_id, NULL, &engine))
    return "";
  return engine.display_name;
}

std::string ComponentExtensionIMEManager::GetDescription(
    const std::string& input_method_id) {
  ComponentExtensionEngine engine;
  if (!FindEngineEntry(input_method_id, NULL, &engine))
    return "";
  return engine.description;
}

std::vector<std::string> ComponentExtensionIMEManager::ListIMEByLanguage(
    const std::string& language) {
  std::vector<std::string> result;
  for (size_t i = 0; i < component_extension_imes_.size(); ++i) {
    for (size_t j = 0; j < component_extension_imes_[i].engines.size(); ++j) {
      const ComponentExtensionIME& ime = component_extension_imes_[i];
      if (std::find(ime.engines[j].language_codes.begin(),
                    ime.engines[j].language_codes.end(),
                    language) != ime.engines[j].language_codes.end()) {
        result.push_back(extension_ime_util::GetComponentInputMethodID(
            ime.id,
            ime.engines[j].engine_id));
      }
    }
  }
  return result;
}

input_method::InputMethodDescriptors
    ComponentExtensionIMEManager::GetAllIMEAsInputMethodDescriptor() {
  input_method::InputMethodDescriptors result;
  for (size_t i = 0; i < component_extension_imes_.size(); ++i) {
    for (size_t j = 0; j < component_extension_imes_[i].engines.size(); ++j) {
      result.push_back(
          input_method::InputMethodDescriptor(
              extension_ime_util::GetComponentInputMethodID(
                  component_extension_imes_[i].id,
                  component_extension_imes_[i].engines[j].engine_id),
              component_extension_imes_[i].engines[j].display_name,
              std::string(), // TODO(uekawa): Set short name.
              component_extension_imes_[i].engines[j].layouts,
              component_extension_imes_[i].engines[j].language_codes,
              false,  // Do not use IME on login screen.
              component_extension_imes_[i].options_page_url,
              component_extension_imes_[i].input_view_url));
    }
  }
  return result;
}

void ComponentExtensionIMEManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ComponentExtensionIMEManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ComponentExtensionIMEManager::FindEngineEntry(
    const std::string& input_method_id,
    ComponentExtensionIME* out_extension,
    ComponentExtensionEngine* out_engine) {
  if (!extension_ime_util::IsComponentExtensionIME(input_method_id))
    return false;
  for (size_t i = 0; i < component_extension_imes_.size(); ++i) {
    const std::string extension_id = component_extension_imes_[i].id;
    const std::vector<ComponentExtensionEngine>& engines =
        component_extension_imes_[i].engines;

    for (size_t j = 0; j < engines.size(); ++j) {
      const std::string trial_ime_id =
          extension_ime_util::GetComponentInputMethodID(
              extension_id, engines[j].engine_id);
      if (trial_ime_id != input_method_id)
        continue;

      if (out_extension)
        *out_extension = component_extension_imes_[i];
      if (out_engine)
        *out_engine = component_extension_imes_[i].engines[j];
      return true;
    }
  }
  return false;
}

}  // namespace chromeos
