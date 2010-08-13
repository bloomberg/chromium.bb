// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_UTIL_H_
#pragma once

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/language_preferences.h"

class ListValue;

namespace {

// Returns an i18n-content value corresponding to |preference|.
template <typename T>
std::string GetI18nContentValue(const T& preference) {
  return std::string(preference.ibus_config_name) + "Content";
}

// Returns a property name of templateData corresponding to |preference|.
template <typename T>
std::string GetTemplateDataPropertyName(const T& preference) {
  return std::string(preference.ibus_config_name) + "Value";
}

// Returns an property name of templateData corresponding the value of the min
// attribute.
template <typename T>
std::string GetTemplateDataMinName(const T& preference) {
  return std::string(preference.ibus_config_name) + "Min";
}

// Returns an property name of templateData corresponding the value of the max
// attribute.
template <typename T>
std::string GetTemplateDataMaxName(const T& preference) {
  return std::string(preference.ibus_config_name) + "Max";
}

}  // namespace

namespace chromeos {

ListValue* CreateMultipleChoiceList(
    const LanguageMultipleChoicePreference<const char*>& preference);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DOM_UI_LANGUAGE_OPTIONS_UTIL_H_
