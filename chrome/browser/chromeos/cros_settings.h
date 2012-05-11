// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/hash_tables.h"
#include "base/observer_list.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "content/public/browser/notification_observer.h"

namespace base {
template <typename T> struct DefaultLazyInstanceTraits;
class ListValue;
class Value;
}

namespace chromeos {

class CrosSettingsProvider;

// This class manages per-device/global settings.
class CrosSettings : public base::NonThreadSafe {
 public:
  // Class factory.
  static CrosSettings* Get();

  // Helper function to test if the given |path| is a valid cros setting.
  static bool IsCrosSettings(const std::string& path);

  // Sets |in_value| to given |path| in cros settings.
  void Set(const std::string& path, const base::Value& in_value);

  // Returns setting value for the given |path|.
  const base::Value* GetPref(const std::string& path) const;

  // Requests all providers to fetch their values from a trusted store, if they
  // haven't done so yet. Returns true if the cros settings returned by |this|
  // are trusted during the current loop cycle; otherwise returns false, and
  // |callback| will be invoked later when trusted values become available.
  // PrepareTrustedValues() should be tried again in that case.
  virtual bool PrepareTrustedValues(const base::Closure& callback) const;

  // Convenience forms of Set().  These methods will replace any existing
  // value at that |path|, even if it has a different type.
  void SetBoolean(const std::string& path, bool in_value);
  void SetInteger(const std::string& path, int in_value);
  void SetDouble(const std::string& path, double in_value);
  void SetString(const std::string& path, const std::string& in_value);

  // Convenience functions for manipulating lists. Note that the following
  // functions employs a read, modify and write pattern. If underlying settings
  // provider updates its value asynchronously such as DeviceSettingsProvider,
  // value cache they read from might not be fresh and multiple calls to those
  // function would lose data. See http://crbug.com/127215
  void AppendToList(const std::string& path, const base::Value* value);
  void RemoveFromList(const std::string& path, const base::Value* value);

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the |path| is valid and the value at
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

  // Adding/removing of providers.
  bool AddSettingsProvider(CrosSettingsProvider* provider);
  bool RemoveSettingsProvider(CrosSettingsProvider* provider);

  // If the pref at the given |path| changes, we call the observer's Observe
  // method with NOTIFICATION_SYSTEM_SETTING_CHANGED.
  void AddSettingsObserver(const char* path,
                           content::NotificationObserver* obs);
  void RemoveSettingsObserver(const char* path,
                              content::NotificationObserver* obs);

  // Returns the provider that handles settings with the |path| or prefix.
  CrosSettingsProvider* GetProvider(const std::string& path) const;

  // Forces all providers to reload their caches from the respective backing
  // stores if they have any.
  void ReloadProviders();

 private:
  friend struct base::DefaultLazyInstanceTraits<CrosSettings>;

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

  // Fires system setting change notification.
  void FireObservers(const std::string& path);

  DISALLOW_COPY_AND_ASSIGN(CrosSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
