// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/component_extension_ime_manager.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromeos/ime/extension_ime_util.h"

namespace chromeos {

namespace {

// The whitelist for enabling extension based xkb keyboards at login session.
const char* kLoginLayoutWhitelist[] = {
  "be",
  "br",
  "ca",
  "ca(eng)",
  "ca(multix)",
  "ch",
  "ch(fr)",
  "cz",
  "cz(qwerty)",
  "de",
  "de(neo)",
  "dk",
  "ee",
  "es",
  "es(cat)",
  "fi",
  "fr",
  "gb(dvorak)",
  "gb(extd)",
  "hr",
  "hu",
  "is",
  "it",
  "jp",
  "latam",
  "lt",
  "lv(apostrophe)",
  "no",
  "pl",
  "pt",
  "ro",
  "se",
  "si",
  "tr",
  "us",
  "us(altgr-intl)",
  "us(colemak)",
  "us(dvorak)",
  "us(intl)"
};

} // namespace

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

ComponentExtensionIMEManager::ComponentExtensionIMEManager() {
  for (size_t i = 0; i < arraysize(kLoginLayoutWhitelist); ++i) {
    login_layout_set_.insert(kLoginLayoutWhitelist[i]);
  }
}

ComponentExtensionIMEManager::~ComponentExtensionIMEManager() {
}

void ComponentExtensionIMEManager::Initialize(
    scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate) {
  delegate_ = delegate.Pass();
  std::vector<ComponentExtensionIME> ext_list = delegate_->ListIME();
  for (size_t i = 0; i < ext_list.size(); ++i) {
    ComponentExtensionIME& ext = ext_list[i];
    bool extension_exists = IsWhitelistedExtension(ext.id);
    if (!extension_exists)
      component_extension_imes_[ext.id] = ext;
    for (size_t j = 0; j < ext.engines.size(); ++j) {
      ComponentExtensionEngine& ime = ext.engines[j];
      const std::string input_method_id =
          extension_ime_util::GetComponentInputMethodID(ext.id, ime.engine_id);
      if (extension_exists && !IsWhitelisted(input_method_id))
        component_extension_imes_[ext.id].engines.push_back(ime);
      input_method_id_set_.insert(input_method_id);
    }
  }
}

bool ComponentExtensionIMEManager::LoadComponentExtensionIME(
    const std::string& input_method_id) {
  ComponentExtensionIME ime;
  if (FindEngineEntry(input_method_id, &ime))
    return delegate_->Load(ime.id, ime.manifest, ime.path);
  else
    return false;
}

bool ComponentExtensionIMEManager::UnloadComponentExtensionIME(
    const std::string& input_method_id) {
  ComponentExtensionIME ime;
  if (!FindEngineEntry(input_method_id, &ime))
    return false;
  delegate_->Unload(ime.id, ime.path);
  return true;
}

bool ComponentExtensionIMEManager::IsWhitelisted(
    const std::string& input_method_id) {
  return input_method_id_set_.find(input_method_id) !=
         input_method_id_set_.end();
}

bool ComponentExtensionIMEManager::IsWhitelistedExtension(
    const std::string& extension_id) {
  return component_extension_imes_.find(extension_id) !=
         component_extension_imes_.end();
}

input_method::InputMethodDescriptors
    ComponentExtensionIMEManager::GetAllIMEAsInputMethodDescriptor() {
  input_method::InputMethodDescriptors result;
  for (std::map<std::string, ComponentExtensionIME>::const_iterator it =
          component_extension_imes_.begin();
       it != component_extension_imes_.end(); ++it) {
    const ComponentExtensionIME& ext = it->second;
    for (size_t j = 0; j < ext.engines.size(); ++j) {
      const ComponentExtensionEngine& ime = ext.engines[j];
      const std::string input_method_id =
          extension_ime_util::GetComponentInputMethodID(
              ext.id, ime.engine_id);
      const std::vector<std::string>& layouts = ime.layouts;
      result.push_back(
          input_method::InputMethodDescriptor(
              input_method_id,
              ime.display_name,
              std::string(), // TODO(uekawa): Set short name.
              layouts,
              ime.language_codes,
              // Enables extension based xkb keyboards on login screen.
              extension_ime_util::IsKeyboardLayoutExtension(
                  input_method_id) && IsInLoginLayoutWhitelist(layouts),
              ime.options_page_url,
              ime.input_view_url));
    }
  }
  return result;
}

input_method::InputMethodDescriptors
ComponentExtensionIMEManager::GetXkbIMEAsInputMethodDescriptor() {
  input_method::InputMethodDescriptors result;
  const input_method::InputMethodDescriptors& descriptors =
      GetAllIMEAsInputMethodDescriptor();
  for (size_t i = 0; i < descriptors.size(); ++i) {
    if (extension_ime_util::IsKeyboardLayoutExtension(descriptors[i].id()))
      result.push_back(descriptors[i]);
  }
  return result;
}

bool ComponentExtensionIMEManager::FindEngineEntry(
    const std::string& input_method_id,
    ComponentExtensionIME* out_extension) {
  if (!IsWhitelisted(input_method_id))
    return false;

  std::string extension_id =
      extension_ime_util::GetExtensionIDFromInputMethodID(input_method_id);
  std::map<std::string, ComponentExtensionIME>::iterator it =
      component_extension_imes_.find(extension_id);
  if (it == component_extension_imes_.end())
    return false;

  if (out_extension)
    *out_extension = it->second;
  return true;
}

bool ComponentExtensionIMEManager::IsInLoginLayoutWhitelist(
    const std::vector<std::string>& layouts) {
  for (size_t i = 0; i < layouts.size(); ++i) {
    if (login_layout_set_.find(layouts[i]) != login_layout_set_.end())
      return true;
  }
  return false;
}

}  // namespace chromeos
