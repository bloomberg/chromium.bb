// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_API_H__
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"

class PrefService;

namespace base {
class Value;
}

class ExtensionPreferenceEventRouter : public content::NotificationObserver {
 public:
  explicit ExtensionPreferenceEventRouter(Profile* profile);
  virtual ~ExtensionPreferenceEventRouter();

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void OnPrefChanged(PrefService* pref_service, const std::string& pref_key);

  // This method dispatches events to the extension message service.
  void DispatchEvent(const std::string& extension_id,
                     const std::string& event_name,
                     const std::string& json_args);

  PrefChangeRegistrar registrar_;
  PrefChangeRegistrar incognito_registrar_;

  // Weak, owns us (transitively via ExtensionService).
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPreferenceEventRouter);
};

class PrefTransformerInterface {
 public:
  virtual ~PrefTransformerInterface() {}

  // Converts the representation of a preference as seen by the extension
  // into a representation that is used in the pref stores of the browser.
  // Returns the pref store representation in case of success or sets
  // |error| and returns NULL otherwise. |bad_message| is passed to simulate
  // the behavior of EXTENSION_FUNCTION_VALIDATE. It is never NULL.
  // The ownership of the returned value is passed to the caller.
  virtual base::Value* ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) = 0;

  // Converts the representation of the preference as stored in the browser
  // into a representation that is used by the extension.
  // Returns the extension representation in case of success or NULL otherwise.
  // The ownership of the returned value is passed to the caller.
  virtual base::Value* BrowserToExtensionPref(
      const base::Value* browser_pref) = 0;
};

// A base class to provide functionality common to the other *PreferenceFunction
// classes.
class PreferenceFunction : public SyncExtensionFunction {
 protected:
  virtual ~PreferenceFunction();

  // Given an |extension_pref_key|, provides its |browser_pref_key| from the
  // static map in extension_preference.cc. Returns true if the corresponding
  // browser pref exists and the extension has the API permission needed to
  // modify that pref. Sets |error_| if the extension doesn't have the needed
  // permission.
  bool ValidateBrowserPref(const std::string& extension_pref_key,
                           std::string* browser_pref_key);
};

class GetPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("types.ChromeSetting.get")

 protected:
  virtual ~GetPreferenceFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SetPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("types.ChromeSetting.set")

 protected:
  virtual ~SetPreferenceFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ClearPreferenceFunction : public PreferenceFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("types.ChromeSetting.clear")

 protected:
  virtual ~ClearPreferenceFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFERENCE_API_H__
