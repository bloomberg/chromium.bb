// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_TYPES_H_
#define CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_TYPES_H_

// This enum must match the numbering for NTPBackgroundCustomizationAvailability
// in enums.xml. Do not reorder or remove items, only add new items before
// BACKGROUND_CUSTOMIZATION_MAX.
enum class BackgroundCustomization {
  BACKGROUND_CUSTOMIZATION_AVAILABLE = 0,
  BACKGROUND_CUSTOMIZATION_UNAVAILABLE_FEATURE = 1,
  BACKGROUND_CUSTOMIZATION_UNAVAILABLE_THEME = 2,
  BACKGROUND_CUSTOMIZATION_UNAVAILABLE_SEARCH_PROVIDER = 3,

  kMaxValue = BACKGROUND_CUSTOMIZATION_UNAVAILABLE_SEARCH_PROVIDER
};

// This enum must match the numbering for NTPShortcutCustomizationAvailability
// in enums.xml. Do not reorder or remove items, only add new items before
// SHORTCUT_CUSTOMIZATION_MAX.
enum class ShortcutCustomization {
  SHORTCUT_CUSTOMIZATION_AVAILABLE = 0,
  SHORTCUT_CUSTOMIZATION_UNAVAILABLE_FEATURE = 1,
  SHORTCUT_CUSTOMIZATION_UNAVAILABLE_SEARCH_PROVIDER = 2,

  kMaxValue = SHORTCUT_CUSTOMIZATION_UNAVAILABLE_SEARCH_PROVIDER
};

// This enum must match the numbering for NTPCustomizedFeatures in
// enums.xml. Do not reorder or remove items, only add new items before
// CUSTOMIZED_FEATURE_MAX.
enum class CustomizedFeature {
  CUSTOMIZED_FEATURE_BACKGROUND = 0,
  CUSTOMIZED_FEATURE_SHORTCUT = 1,

  kMaxValue = CUSTOMIZED_FEATURE_SHORTCUT
};

// This enum must match the numbering for NTPCustomizeAction in
// enums.xml. Do not reorder or remove items, and only add new items before
// CUSTOMIZE_ACTION_MAX.
enum class CustomizeAction {
  CUSTOMIZE_ACTION_CHROME_BACKGROUNDS = 0,
  CUSTOMIZE_ACTION_LOCAL_IMAGE = 1,
  CUSTOMIZE_ACTION_RESTORE_BACKGROUND = 2,
  CUSTOMIZE_ACTION_ATTRIBUTION = 3,
  CUSTOMIZE_ACTION_ADD_SHORTCUT = 4,
  CUSTOMIZE_ACTION_EDIT_SHORTCUT = 5,
  CUSTOMIZE_ACTION_RESTORE_SHORTCUT = 6,

  kMaxValue = CUSTOMIZE_ACTION_RESTORE_SHORTCUT
};

// This enum must match the numbering for NTPCustomizeChromeBackgroundAction in
// enums.xml. Do not reorder or remove items, and only add new items before
// CUSTOMIZE_CHROME_BACKGROUND_ACTION_MAX.
enum class CustomizeChromeBackgroundAction {
  CUSTOMIZE_CHROME_BACKGROUND_ACTION_SELECT_COLLECTION = 0,
  CUSTOMIZE_CHROME_BACKGROUND_ACTION_SELECT_IMAGE = 1,
  CUSTOMIZE_CHROME_BACKGROUND_ACTION_CANCEL = 2,
  CUSTOMIZE_CHROME_BACKGROUND_ACTION_DONE = 3,

  kMaxValue = CUSTOMIZE_CHROME_BACKGROUND_ACTION_DONE
};

// This enum must match the numbering for NTPCustomizeLocalImageBackgroundAction
// in enums.xml. Do not reorder or remove items, and only add new items before
// CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_MAX.
enum class CustomizeLocalImageBackgroundAction {
  CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_CANCEL = 0,
  CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_DONE = 1,

  kMaxValue = CUSTOMIZE_LOCAL_IMAGE_BACKGROUND_ACTION_DONE
};

// This enum must match the numbering for NTPCustomizeShortcutAction in
// enums.xml. Do not reorder or remove items, and only add new items before
// CUSTOMIZE_SHORTCUT_ACTION_MAX.
enum class CustomizeShortcutAction {
  CUSTOMIZE_SHORTCUT_ACTION_REMOVE = 0,
  CUSTOMIZE_SHORTCUT_ACTION_CANCEL = 1,
  CUSTOMIZE_SHORTCUT_ACTION_DONE = 2,
  CUSTOMIZE_SHORTCUT_ACTION_UNDO = 3,
  CUSTOMIZE_SHORTCUT_ACTION_RESTORE_ALL = 4,

  kMaxValue = CUSTOMIZE_SHORTCUT_ACTION_RESTORE_ALL
};

#endif  // CHROME_BROWSER_UI_SEARCH_NTP_USER_DATA_TYPES_H_
