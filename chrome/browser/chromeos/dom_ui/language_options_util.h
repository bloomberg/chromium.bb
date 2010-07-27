// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_UTIL_H_

#include "chrome/browser/chromeos/language_preferences.h"

namespace {

// Returns an i18n-content value corresponding to |preference|.
template <typename T>
std::wstring GetI18nContentValue(const T& preference) {
  return ASCIIToWide(preference.ibus_config_name) + L"Content";
}

// Returns a property name of templateData corresponding to |preference|.
template <typename T>
std::wstring GetTemplateDataPropertyName(const T& preference) {
  return ASCIIToWide(preference.ibus_config_name) + L"Value";
}

// Returns an property name of templateData corresponding the value of the min
// attribute.
template <typename T>
std::wstring GetTemplateDataMinName(const T& preference) {
  return ASCIIToWide(preference.ibus_config_name) + L"Min";
}

// Returns an property name of templateData corresponding the value of the max
// attribute.
template <typename T>
std::wstring GetTemplateDataMaxName(const T& preference) {
  return ASCIIToWide(preference.ibus_config_name) + L"Max";
}

}  // namespace

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_UTIL_H_
