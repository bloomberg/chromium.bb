// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_events.h"

#include "base/values.h"

#include "chrome/browser/accessibility/accessibility_extension_api_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace keys = extension_accessibility_api_constants;

void SendAccessibilityNotification(int type, AccessibilityEventInfo* info) {
  Profile *profile = info->profile();
  if (profile->ShouldSendAccessibilityEvents()) {
    content::NotificationService::current()->Notify(
        type,
        content::Source<Profile>(profile),
        content::Details<AccessibilityEventInfo>(info));
  }
}

AccessibilityControlInfo::AccessibilityControlInfo(
    Profile* profile, const std::string& name)
    : AccessibilityEventInfo(profile),
      name_(name) {
}

AccessibilityControlInfo::~AccessibilityControlInfo() {
}

void AccessibilityControlInfo::SerializeToDict(DictionaryValue *dict) const {
  dict->SetString(keys::kNameKey, name_);
  dict->SetString(keys::kTypeKey, type());
  if (!context_.empty())
    dict->SetString(keys::kContextKey, context_);
}

AccessibilityWindowInfo::AccessibilityWindowInfo(Profile* profile,
                                                 const std::string& window_name)
    : AccessibilityControlInfo(profile, window_name) {
}

const char* AccessibilityWindowInfo::type() const {
  return keys::kTypeWindow;
}

AccessibilityButtonInfo::AccessibilityButtonInfo(Profile* profile,
                                                 const std::string& button_name,
                                                 const std::string& context)
    : AccessibilityControlInfo(profile, button_name) {
  set_context(context);
}

const char* AccessibilityButtonInfo::type() const {
  return keys::kTypeButton;
}

AccessibilityLinkInfo::AccessibilityLinkInfo(Profile* profile,
                                             const std::string& link_name,
                                             const std::string& context)
    : AccessibilityControlInfo(profile, link_name) {
  set_context(context);
}

const char* AccessibilityLinkInfo::type() const {
  return keys::kTypeLink;
}

AccessibilityRadioButtonInfo::AccessibilityRadioButtonInfo(
    Profile* profile,
    const std::string& name,
    const std::string& context,
    bool checked,
    int item_index,
    int item_count)
    : AccessibilityControlInfo(profile, name),
      checked_(checked),
      item_index_(item_index),
      item_count_(item_count) {
  set_context(context);
}

const char* AccessibilityRadioButtonInfo::type() const {
  return keys::kTypeRadioButton;
}

void AccessibilityRadioButtonInfo::SerializeToDict(
    DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetBoolean(keys::kCheckedKey, checked_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

AccessibilityCheckboxInfo::AccessibilityCheckboxInfo(Profile* profile,
                                                     const std::string& name,
                                                     const std::string& context,
                                                     bool checked)
    : AccessibilityControlInfo(profile, name),
      checked_(checked) {
  set_context(context);
}

const char* AccessibilityCheckboxInfo::type() const {
  return keys::kTypeCheckbox;
}

void AccessibilityCheckboxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetBoolean(keys::kCheckedKey, checked_);
}

AccessibilityTabInfo::AccessibilityTabInfo(Profile* profile,
                                           const std::string& tab_name,
                                           const std::string& context,
                                           int tab_index,
                                           int tab_count)
    : AccessibilityControlInfo(profile, tab_name),
      tab_index_(tab_index),
      tab_count_(tab_count) {
  set_context(context);
}

const char* AccessibilityTabInfo::type() const {
  return keys::kTypeTab;
}

void AccessibilityTabInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetInteger(keys::kItemIndexKey, tab_index_);
  dict->SetInteger(keys::kItemCountKey, tab_count_);
}

AccessibilityComboBoxInfo::AccessibilityComboBoxInfo(Profile* profile,
                                                     const std::string& name,
                                                     const std::string& context,
                                                     const std::string& value,
                                                     int item_index,
                                                     int item_count)
    : AccessibilityControlInfo(profile, name),
      value_(value),
      item_index_(item_index),
      item_count_(item_count) {
  set_context(context);
}

const char* AccessibilityComboBoxInfo::type() const {
  return keys::kTypeComboBox;
}

void AccessibilityComboBoxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kValueKey, value_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

AccessibilityTextBoxInfo::AccessibilityTextBoxInfo(Profile* profile,
                                                   const std::string& name,
                                                   const std::string& context,
                                                   bool password)
    : AccessibilityControlInfo(profile, name),
      password_(password),
      selection_start_(0),
      selection_end_(0) {
  set_context(context);
}

const char* AccessibilityTextBoxInfo::type() const {
  return keys::kTypeTextBox;
}

void AccessibilityTextBoxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kValueKey, value_);
  dict->SetBoolean(keys::kPasswordKey, password_);
  dict->SetInteger(keys::kSelectionStartKey, selection_start_);
  dict->SetInteger(keys::kSelectionEndKey, selection_end_);
}

AccessibilityListBoxInfo::AccessibilityListBoxInfo(Profile* profile,
                                                   const std::string& name,
                                                   const std::string& context,
                                                   const std::string& value,
                                                   int item_index,
                                                   int item_count)
    : AccessibilityControlInfo(profile, name),
      value_(value),
      item_index_(item_index),
      item_count_(item_count) {
  set_context(context);
}

const char* AccessibilityListBoxInfo::type() const {
  return keys::kTypeListBox;
}

void AccessibilityListBoxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kValueKey, value_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

AccessibilityMenuInfo::AccessibilityMenuInfo(Profile* profile,
                                             const std::string& menu_name)
    : AccessibilityControlInfo(profile, menu_name) {
}

const char* AccessibilityMenuInfo::type() const {
  return keys::kTypeMenu;
}

AccessibilityMenuItemInfo::AccessibilityMenuItemInfo(Profile* profile,
                                                     const std::string& name,
                                                     const std::string& context,
                                                     bool has_submenu,
                                                     int item_index,
                                                     int item_count)
    : AccessibilityControlInfo(profile, name),
      has_submenu_(has_submenu),
      item_index_(item_index),
      item_count_(item_count) {
  set_context(context);
}

const char* AccessibilityMenuItemInfo::type() const {
  return keys::kTypeMenuItem;
}

void AccessibilityMenuItemInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetBoolean(keys::kHasSubmenuKey, has_submenu_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

AccessibilitySliderInfo::AccessibilitySliderInfo(Profile* profile,
                                                 const std::string& name,
                                                 const std::string& context,
                                                 const std::string& value)
    : AccessibilityControlInfo(profile, name),
      value_(value) {
  set_context(context);
}

const char* AccessibilitySliderInfo::type() const {
  return keys::kTypeSlider;
}

void AccessibilitySliderInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kStringValueKey, value_);
}
