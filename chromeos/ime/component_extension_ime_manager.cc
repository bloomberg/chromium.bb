// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"

namespace chromeos {

ComponentExtensionIME::ComponentExtensionIME() {
}

ComponentExtensionIME::~ComponentExtensionIME() {
}

ComponentExtentionIMEManagerDelegate::ComponentExtentionIMEManagerDelegate() {
}

ComponentExtentionIMEManagerDelegate::~ComponentExtentionIMEManagerDelegate() {
}

ComponentExtentionIMEManager::ComponentExtentionIMEManager(
    ComponentExtentionIMEManagerDelegate* delegate)
    : delegate_(delegate) {
}

ComponentExtentionIMEManager::~ComponentExtentionIMEManager() {
}

void ComponentExtentionIMEManager::Initialize() {
  component_extension_imes_ = delegate_->ListIME();
}

bool ComponentExtentionIMEManager::LoadComponentExtensionIME(
    const std::string& input_method_id) {
  ComponentExtensionIME ime;
  if (FindEngineEntry(input_method_id, &ime, NULL))
    return delegate_->Load(ime.id, ime.path);
  else
    return false;
}

bool ComponentExtentionIMEManager::UnloadComponentExtensionIME(
    const std::string& input_method_id) {
  ComponentExtensionIME ime;
  if (FindEngineEntry(input_method_id, &ime, NULL))
    return delegate_->Unload(ime.id, ime.path);
  else
    return false;
}

bool ComponentExtentionIMEManager::IsComponentExtensionIMEId(
    const std::string& input_method_id) {
  return FindEngineEntry(input_method_id, NULL, NULL);
}

std::string ComponentExtentionIMEManager::GetName(
    const std::string& input_method_id) {
  IBusComponent::EngineDescription engine;
  if (!FindEngineEntry(input_method_id, NULL, &engine))
    return "";
  return engine.display_name;
}

std::string ComponentExtentionIMEManager::GetDescription(
    const std::string& input_method_id) {
  IBusComponent::EngineDescription engine;
  if (!FindEngineEntry(input_method_id, NULL, &engine))
    return "";
  return engine.description;
}

std::vector<std::string> ComponentExtentionIMEManager::ListIMEByLanguage(
    const std::string& language) {
  std::vector<std::string> result;
  for (size_t i = 0; i < component_extension_imes_.size(); ++i) {
    for (size_t j = 0; j < component_extension_imes_[i].engines.size(); ++j) {
      if (component_extension_imes_[i].engines[j].language_code == language)
        result.push_back(
            extension_ime_util::GetInputMethodID(
                component_extension_imes_[i].id,
                component_extension_imes_[i].engines[j].engine_id));
    }
  }
  return result;
}

bool ComponentExtentionIMEManager::FindEngineEntry(
    const std::string& input_method_id,
    ComponentExtensionIME* out_extension,
    IBusComponent::EngineDescription* out_engine) {
  if (!extension_ime_util::IsExtensionIME(input_method_id))
    return false;
  for (size_t i = 0; i < component_extension_imes_.size(); ++i) {
    const std::string extension_id = component_extension_imes_[i].id;
    if (!extension_ime_util::IsMemberOfExtension(input_method_id, extension_id))
      continue;
    const std::vector<IBusComponent::EngineDescription>& engines =
        component_extension_imes_[i].engines;

    for (size_t j = 0; j < engines.size(); ++j) {
      const std::string trial_ime_id =
          extension_ime_util::GetInputMethodID(extension_id,
                                               engines[j].engine_id);
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
