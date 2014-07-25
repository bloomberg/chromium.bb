// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used to for the Accessibility API.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_CONSTANTS_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_CONSTANTS_H_

namespace extension_accessibility_api_constants {

// Keys.
extern const char kTypeKey[];
extern const char kNameKey[];
extern const char kContextKey[];
extern const char kValueKey[];
extern const char kChildrenCountKey[];
extern const char kPasswordKey[];
extern const char kItemCountKey[];
extern const char kItemDepthKey[];
extern const char kItemIndexKey[];
extern const char kItemExpandedKey[];
extern const char kSelectionStartKey[];
extern const char kSelectionEndKey[];
extern const char kCheckedKey[];
extern const char kHasSubmenuKey[];
extern const char kMessageKey[];
extern const char kStringValueKey[];

// Types of controls that can receive accessibility events.
extern const char kTypeAlert[];
extern const char kTypeButton[];
extern const char kTypeCheckbox[];
extern const char kTypeComboBox[];
extern const char kTypeLink[];
extern const char kTypeListBox[];
extern const char kTypeMenu[];
extern const char kTypeMenuItem[];
extern const char kTypeRadioButton[];
extern const char kTypeSlider[];
extern const char kTypeStaticText[];
extern const char kTypeTab[];
extern const char kTypeTextBox[];
extern const char kTypeTree[];
extern const char kTypeTreeItem[];
extern const char kTypeVolume[];
extern const char kTypeWindow[];

};  // namespace extension_accessibility_api_constants

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_CONSTANTS_H_
