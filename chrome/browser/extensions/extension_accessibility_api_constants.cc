// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_accessibility_api_constants.h"

namespace extension_accessibility_api_constants {

// String keys for AccessibilityObject properties.
const wchar_t kTypeKey[] = L"type";
const wchar_t kNameKey[] = L"name";
const wchar_t kDetailsKey[] = L"details";
const wchar_t kValueKey[] = L"details.value";
const wchar_t kPasswordKey[] = L"details.isPassword";
const wchar_t kItemCountKey[] = L"details.itemCount";
const wchar_t kItemIndexKey[] = L"details.itemIndex";
const wchar_t kSelectionStartKey[] = L"details.selectionStart";
const wchar_t kSelectionEndKey[] = L"details.selectionEnd";
const wchar_t kCheckedKey[] = L"details.isChecked";

// Events.
const char kOnWindowOpened[] = "experimental.accessibility.onWindowOpened";
const char kOnWindowClosed[] = "experimental.accessibility.onWindowClosed";
const char kOnControlFocused[] = "experimental.accessibility.onControlFocused";
const char kOnControlAction[] = "experimental.accessibility.onControlAction";
const char kOnTextChanged[] = "experimental.accessibility.onTextChanged";

// Types of controls that can receive accessibility events.
extern const char kTypeButton[] = "button";
extern const char kTypeCheckbox[] = "checkbox";
extern const char kTypeComboBox[] = "combobox";
extern const char kTypeLink[] = "link";
extern const char kTypeRadioButton[] = "radiobutton";
extern const char kTypeTab[] = "tab";
extern const char kTypeTextBox[] = "textbox";
extern const char kTypeWindow[] = "window";

}  // namespace extension_accessibility_api_constants
