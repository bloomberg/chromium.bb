// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_events.h"

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
}

void AccessibilityWindowInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeWindow);
}

void AccessibilityButtonInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeButton);
}

void AccessibilityLinkInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeLink);
}

void AccessibilityRadioButtonInfo::SerializeToDict(
    DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeRadioButton);
  dict->SetBoolean(keys::kCheckedKey, checked_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

void AccessibilityCheckboxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeCheckbox);
  dict->SetBoolean(keys::kCheckedKey, checked_);
}

void AccessibilityTabInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeTab);
  dict->SetInteger(keys::kItemIndexKey, tab_index_);
  dict->SetInteger(keys::kItemCountKey, tab_count_);
}

void AccessibilityComboBoxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeComboBox);
  dict->SetString(keys::kValueKey, value_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

void AccessibilityTextBoxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeTextBox);
  dict->SetString(keys::kValueKey, value_);
  dict->SetBoolean(keys::kPasswordKey, password_);
  dict->SetInteger(keys::kSelectionStartKey, selection_start_);
  dict->SetInteger(keys::kSelectionEndKey, selection_end_);
}

void AccessibilityListBoxInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeListBox);
  dict->SetString(keys::kValueKey, value_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}

void AccessibilityMenuInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeMenu);
}

void AccessibilityMenuItemInfo::SerializeToDict(DictionaryValue *dict) const {
  AccessibilityControlInfo::SerializeToDict(dict);
  dict->SetString(keys::kTypeKey, keys::kTypeMenuItem);
  dict->SetBoolean(keys::kHasSubmenuKey, has_submenu_);
  dict->SetInteger(keys::kItemIndexKey, item_index_);
  dict->SetInteger(keys::kItemCountKey, item_count_);
}
