// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the chrome.settingsPrivate.setBooleanPref method.
class SettingsPrivateSetBooleanPrefFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateSetBooleanPrefFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.setBooleanPref",
                             SETTINGSPRIVATE_SETBOOLEANPREF);

 protected:
  ~SettingsPrivateSetBooleanPrefFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateSetBooleanPrefFunction);
};

// Implements the chrome.settingsPrivate.setNumericPref method.
class SettingsPrivateSetNumericPrefFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateSetNumericPrefFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.setNumericPref",
                             SETTINGSPRIVATE_SETNUMERICPREF);

 protected:
  ~SettingsPrivateSetNumericPrefFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateSetNumericPrefFunction);
};

// Implements the chrome.settingsPrivate.setStringPref method.
class SettingsPrivateSetStringPrefFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateSetStringPrefFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.setStringPref",
                             SETTINGSPRIVATE_SETSTRINGPREF);

 protected:
  ~SettingsPrivateSetStringPrefFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateSetStringPrefFunction);
};

// Implements the chrome.settingsPrivate.setURLPref method.
class SettingsPrivateSetURLPrefFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateSetURLPrefFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.setURLPref",
                             SETTINGSPRIVATE_SETURLPREF);

 protected:
  ~SettingsPrivateSetURLPrefFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateSetURLPrefFunction);
};

// Implements the chrome.settingsPrivate.getAllPrefs method.
class SettingsPrivateGetAllPrefsFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateGetAllPrefsFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.getAllPrefs",
                             SETTINGSPRIVATE_GETALLPREFS);

 protected:
  ~SettingsPrivateGetAllPrefsFunction() override;

  // AsyncExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateGetAllPrefsFunction);
};

// Implements the chrome.settingsPrivate.getPref method.
class SettingsPrivateGetPrefFunction : public UIThreadExtensionFunction {
 public:
  SettingsPrivateGetPrefFunction() {}
  DECLARE_EXTENSION_FUNCTION("settingsPrivate.getPref",
                             SETTINGSPRIVATE_GETPREF);

 protected:
  ~SettingsPrivateGetPrefFunction() override;

  // AsyncExtensionFunction overrides.
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(SettingsPrivateGetPrefFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_API_H_
