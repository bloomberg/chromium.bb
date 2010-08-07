// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_events.h"

#include "base/values.h"

#include "chrome/browser/extensions/extension_accessibility_api_constants.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

namespace keys = extension_accessibility_api_constants;

void SendAccessibilityNotification(
    NotificationType type, AccessibilityControlInfo* info) {
  Profile *profile = info->profile();
  if (profile->ShouldSendAccessibilityEvents()) {
    NotificationService::current()->Notify(
        type,
        Source<Profile>(profile),
        Details<AccessibilityControlInfo>(info));
  }
}

void AccessibilityControlInfo::SerializeToDict(DictionaryValue *dict) const {
  dict->SetString(keys::kNameKey, name_);
  dict->SetString(keys::kTypeKey, type());
}

const char* AccessibilityWindowInfo::type() const {
  return keys::kTypeWindow;
}

const char* AccessibilityButtonInfo::type() const {
  return keys::kTypeButton;
}

const char* AccessibilityLinkInfo::type() const {
  return keys::kTypeLink;
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

const char* AccessibilityCheckboxInfo::type() const {
  return keys::kTypeCheckbox;
}

void AccessibilityCheckboxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetBoolean(keys::kCheckedKey, checked_);
}

const char* AccessibilityTabInfo::type() const {
  return keys::kTypeTab;
}

void AccessibilityTabInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetInteger(keys::kItemIndexKey, tab_index_);
  dict->SetInteger(keys::kItemCountKey, tab_count_);
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

const char* AccessibilityListBoxInfo::type() const {
  return keys::kTypeListBox;
}

void AccessibilityListBoxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kValueKey, value_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

const char* AccessibilityMenuInfo::type() const {
  return keys::kTypeMenu;
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
