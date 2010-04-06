// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to for the Accessibility API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACCESSIBILITY_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACCESSIBILITY_API_CONSTANTS_H_

namespace extension_accessibility_api_constants {

// Keys.
extern const wchar_t kTypeKey[];
extern const wchar_t kNameKey[];
extern const wchar_t kDetailsKey[];
extern const wchar_t kValueKey[];
extern const wchar_t kPasswordKey[];
extern const wchar_t kItemCountKey[];
extern const wchar_t kItemIndexKey[];
extern const wchar_t kSelectionStartKey[];
extern const wchar_t kSelectionEndKey[];
extern const wchar_t kCheckedKey[];
extern const wchar_t kHasSubmenuKey[];

// Events.
extern const char kOnWindowOpened[];
extern const char kOnWindowClosed[];
extern const char kOnControlFocused[];
extern const char kOnControlAction[];
extern const char kOnTextChanged[];
extern const char kOnMenuOpened[];
extern const char kOnMenuClosed[];

// Types of controls that can receive accessibility events
extern const char kTypeButton[];
extern const char kTypeCheckbox[];
extern const char kTypeComboBox[];
extern const char kTypeLink[];
extern const char kTypeListBox[];
extern const char kTypeMenu[];
extern const char kTypeMenuItem[];
extern const char kTypeRadioButton[];
extern const char kTypeTab[];
extern const char kTypeTextBox[];
extern const char kTypeWindow[];

};  // namespace extension_accessibility_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACCESSIBILITY_API_CONSTANTS_H_
