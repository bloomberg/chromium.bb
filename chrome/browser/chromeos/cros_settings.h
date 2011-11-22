// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#pragma once

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "content/public/browser/notification_observer.h"

namespace base {
template <typename T> struct DefaultLazyInstanceTraits;
class ListValue;
class Value;
}

namespace chromeos {

// A class manages per-device/global settings.
class CrosSettings : public base::NonThreadSafe {
 public:
  // Class factory.
  static CrosSettings* Get();

  // Helper function to test if given path is a value cros settings name.
  static bool IsCrosSettings(const std::string& path);

  // Sets |in_value| to given |path| in cros settings.
  void Set(const std::string& path, const base::Value& in_value);

  // Fires system setting change notification.
  // TODO(pastarmovj): Consider to remove this function from the public
  // interface.
  void FireObservers(const char* path);

  // Gets settings value of given |path| to |out_value|.
  const base::Value* GetPref(const std::string& path) const;

  // Starts a fetch from the trusted store for the value of |path| if not loaded
  // yet. It will call the |callback| function upon completion if a new fetch
  // was needed in which case the return value is false. Else it will return
  // true and won't call the |callback|.
  bool GetTrusted(const std::string& path,
                  const base::Closure& callback) const;

  // Convenience forms of Set().  These methods will replace any existing
  // value at that path, even if it has a different type.
  void SetBoolean(const std::string& path, bool in_value);
  void SetInteger(const std::string& path, int in_value);
  void SetDouble(const std::string& path, double in_value);
  void SetString(const std::string& path, const std::string& in_value);

  // Convenience functions for manipulating lists.
  void AppendToList(const std::string& path, const base::Value* value);
  void RemoveFromList(const std::string& path, const base::Value* value);

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the path is valid and the value at
  // the end of the path can be returned in the form specified.
  bool GetBoolean(const std::string& path, bool* out_value) const;
  bool GetInteger(const std::string& path, int* out_value) const;
  bool GetDouble(const std::string& path, double* out_value) const;
  bool GetString(const std::string& path, std::string* out_value) const;
  bool GetList(const std::string& path,
               const base::ListValue** out_value) const;

  // Helper function for the whitelist op. Implemented here because we will need
  // this in a few places. The functions searches for |email| in the pref |path|
  // It respects whitelists so foo@bar.baz will match *@bar.baz too.
  bool FindEmailInList(const std::string& path, const std::string& email) const;

  // adding/removing of providers
  bool AddSettingsProvider(CrosSettingsProvider* provider);
  bool RemoveSettingsProvider(CrosSettingsProvider* provider);

  // If the pref at the given path changes, we call the observer's Observe
  // method with PREF_CHANGED.
  void AddSettingsObserver(const char* path,
                           content::NotificationObserver* obs);
  void RemoveSettingsObserver(const char* path,
                              content::NotificationObserver* obs);

  // Returns the provider that handles settings with the path or prefix.
  CrosSettingsProvider* GetProvider(const std::string& path) const;

 private:
  // List of ChromeOS system settings providers.
  std::vector<CrosSettingsProvider*> providers_;

  // A map from settings names to a list of observers. Observers get fired in
  // the order they are added.
  typedef ObserverList<content::NotificationObserver> NotificationObserverList;
  typedef base::hash_map<std::string, NotificationObserverList*>
      SettingsObserverMap;
  SettingsObserverMap settings_observers_;

  CrosSettings();
  ~CrosSettings();
  friend struct base::DefaultLazyInstanceTraits<CrosSettings>;

  DISALLOW_COPY_AND_ASSIGN(CrosSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
