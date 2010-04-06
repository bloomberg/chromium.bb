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
const wchar_t kHasSubmenuKey[] = L"details.hasSubmenu";

// Events.
const char kOnWindowOpened[] = "experimental.accessibility.onWindowOpened";
const char kOnWindowClosed[] = "experimental.accessibility.onWindowClosed";
const char kOnControlFocused[] = "experimental.accessibility.onControlFocused";
const char kOnControlAction[] = "experimental.accessibility.onControlAction";
const char kOnTextChanged[] = "experimental.accessibility.onTextChanged";
const char kOnMenuOpened[] = "experimental.accessibility.onMenuOpened";
const char kOnMenuClosed[] = "experimental.accessibility.onMenuClosed";

// Types of controls that can receive accessibility events.
const char kTypeButton[] = "button";
const char kTypeCheckbox[] = "checkbox";
const char kTypeComboBox[] = "combobox";
const char kTypeLink[] = "link";
const char kTypeListBox[] = "listbox";
const char kTypeMenu[] = "menu";
const char kTypeMenuItem[] = "menuitem";
const char kTypeRadioButton[] = "radiobutton";
const char kTypeTab[] = "tab";
const char kTypeTextBox[] = "textbox";
const char kTypeWindow[] = "window";

}  // namespace extension_accessibility_api_constants
