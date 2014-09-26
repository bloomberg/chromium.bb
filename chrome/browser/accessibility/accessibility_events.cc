// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_events.h"

#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/accessibility/accessibility_extension_api_constants.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace keys = extension_accessibility_api_constants;

void SendControlAccessibilityNotification(
  ui::AXEvent event,
  AccessibilityControlInfo* info) {
  Profile* profile = info->profile();
  if (profile->ShouldSendAccessibilityEvents()) {
    ExtensionAccessibilityEventRouter::GetInstance()->HandleControlEvent(
        event,
        info);
  }
}

void SendMenuAccessibilityNotification(
  ui::AXEvent event,
  AccessibilityMenuInfo* info) {
  Profile* profile = info->profile();
  if (profile->ShouldSendAccessibilityEvents()) {
    ExtensionAccessibilityEventRouter::GetInstance()->HandleMenuEvent(
        event,
        info);
  }
}

void SendWindowAccessibilityNotification(
  ui::AXEvent event,
  AccessibilityWindowInfo* info) {
  Profile* profile = info->profile();
  if (profile->ShouldSendAccessibilityEvents()) {
    ExtensionAccessibilityEventRouter::GetInstance()->HandleWindowEvent(
        event,
        info);
  }
}

AccessibilityControlInfo::AccessibilityControlInfo(
    Profile* profile, const std::string& name)
    : AccessibilityEventInfo(profile),
      name_(name) {
}

AccessibilityControlInfo::~AccessibilityControlInfo() {
}

void AccessibilityControlInfo::SerializeToDict(
    base::DictionaryValue *dict) const {
  dict->SetString(keys::kNameKey, name_);
  dict->SetString(keys::kTypeKey, type());
  if (!context_.empty())
    dict->SetString(keys::kContextKey, context_);
  if (!bounds_.IsEmpty()) {
    base::DictionaryValue* bounds_value = new base::DictionaryValue();
    bounds_value->SetInteger(keys::kLeft, bounds_.x());
    bounds_value->SetInteger(keys::kTop, bounds_.y());
    bounds_value->SetInteger(keys::kWidth, bounds_.width());
    bounds_value->SetInteger(keys::kHeight, bounds_.height());
    dict->Set(keys::kBoundsKey, bounds_value);
  }
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

AccessibilityStaticTextInfo::AccessibilityStaticTextInfo(Profile* profile,
                                                 const std::string& text,
                                                 const std::string& context)
    : AccessibilityControlInfo(profile, text) {
  set_context(context);
}

const char* AccessibilityStaticTextInfo::type() const {
  return keys::kTypeStaticText;
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
    base::DictionaryValue *dict) const {
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

void AccessibilityCheckboxInfo::SerializeToDict(
    base::DictionaryValue *dict) const {
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

void AccessibilityTabInfo::SerializeToDict(base::DictionaryValue *dict) const {
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

void AccessibilityComboBoxInfo::SerializeToDict(
    base::DictionaryValue *dict) const {
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

void AccessibilityTextBoxInfo::SerializeToDict(
    base::DictionaryValue *dict) const {
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

void AccessibilityListBoxInfo::SerializeToDict(
    base::DictionaryValue *dict) const {
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

void AccessibilityMenuItemInfo::SerializeToDict(
    base::DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetBoolean(keys::kHasSubmenuKey, has_submenu_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

AccessibilityTreeInfo::AccessibilityTreeInfo(Profile* profile,
                                             const std::string& menu_name)
    : AccessibilityControlInfo(profile, menu_name) {
}

const char* AccessibilityTreeInfo::type() const {
  return keys::kTypeTree;
}

AccessibilityTreeItemInfo::AccessibilityTreeItemInfo(Profile* profile,
                                                     const std::string& name,
                                                     const std::string& context,
                                                     int item_depth,
                                                     int item_index,
                                                     int item_count,
                                                     int children_count,
                                                     bool is_expanded)
    : AccessibilityControlInfo(profile, name),
      item_depth_(item_depth),
      item_index_(item_index),
      item_count_(item_count),
      children_count_(children_count),
      is_expanded_(is_expanded) {
  set_context(context);
}

const char* AccessibilityTreeItemInfo::type() const {
  return keys::kTypeTreeItem;
}

void AccessibilityTreeItemInfo::SerializeToDict(
    base::DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetInteger(keys::kItemDepthKey, item_depth_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
  dict->SetInteger(keys::kChildrenCountKey, children_count_);
  dict->SetBoolean(keys::kItemExpandedKey, is_expanded_);
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

void AccessibilitySliderInfo::SerializeToDict(
    base::DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kStringValueKey, value_);
}

AccessibilityAlertInfo::AccessibilityAlertInfo(Profile* profile,
                                               const std::string& name)
    : AccessibilityControlInfo(profile, name) {
}

const char* AccessibilityAlertInfo::type() const {
  return keys::kTypeAlert;
}
