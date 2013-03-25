// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromeos/ime/component_extension_ime_manager.h"

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
  // TODO(nona): Implement this.
}

bool ComponentExtentionIMEManager::LoadComponentExtensionIME(
    const std::string& input_method_id) {
  // TODO(nona): Implement this.
  NOTIMPLEMENTED();
  return true;
}

bool ComponentExtentionIMEManager::UnloadComonentExtensionIME(
    const std::string& input_method_id) {
  // TODO(nona): Implement this.
  NOTIMPLEMENTED();
  return true;
}

void ComponentExtentionIMEManager::IsComponentExtensionIMEId(
    const std::string& input_method_id) {
  // TODO(nona): Implement this.
  NOTIMPLEMENTED();
}

std::string ComponentExtentionIMEManager::GetName(
    const std::string& input_method_id) {
  // TODO(nona): Implement this.
  NOTIMPLEMENTED();
  return "";
}

std::string ComponentExtentionIMEManager::GetDescription(
    const std::string& input_method_id) {
  // TODO(nona): Implement this.
  NOTIMPLEMENTED();
  return "";
}

std::vector<std::string> ComponentExtentionIMEManager::ListIMEByLanguage(
    const std::string& language) {
  // TODO(nona): Implement this.
  NOTIMPLEMENTED();
  return std::vector<std::string>();
}

}  // namespace chromeos
