// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
#pragma once

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/observer_list.h"
#include "base/singleton.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "content/common/notification_observer.h"

namespace base {
template <typename T> struct DefaultLazyInstanceTraits;
}

class Value;

namespace chromeos {

class CrosSettingsProvider;

// A class manages per-device/global settings.
class CrosSettings : public base::NonThreadSafe {
 public:
  // Class factory.
  static CrosSettings* Get();

  // Helper function to test if given path is a value cros settings name.
  static bool IsCrosSettings(const std::string& path);

  // Sets |in_value| to given |path| in cros settings.
  // Note that this takes ownership of |in_value|.
  void Set(const std::string& path, Value* in_value);

  // Fires system setting change notification.
  void FireObservers(const char* path);

  // Gets settings value of given |path| to |out_value|.
  // Note that the caller owns |out_value| returned.
  bool Get(const std::string& path, Value** out_value) const;

  // Convenience forms of Set().  These methods will replace any existing
  // value at that path, even if it has a different type.
  void SetBoolean(const std::string& path, bool in_value);
  void SetInteger(const std::string& path, int in_value);
  void SetDouble(const std::string& path, double in_value);
  void SetString(const std::string& path, const std::string& in_value);

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the path is valid and the value at
  // the end of the path can be returned in the form specified.
  bool GetBoolean(const std::string& path, bool* out_value) const;
  bool GetInteger(const std::string& path, int* out_value) const;
  bool GetDouble(const std::string& path, double* out_value) const;
  bool GetString(const std::string& path, std::string* out_value) const;

  // adding/removing of providers
  bool AddSettingsProvider(CrosSettingsProvider* provider);
  bool RemoveSettingsProvider(CrosSettingsProvider* provider);

  // If the pref at the given path changes, we call the observer's Observe
  // method with PREF_CHANGED.
  void AddSettingsObserver(const char* path, NotificationObserver* obs);
  void RemoveSettingsObserver(const char* path, NotificationObserver* obs);

 private:
  // List of ChromeOS system settings providers.
  std::vector<CrosSettingsProvider*> providers_;

  // A map from settings names to a list of observers. Observers get fired in
  // the order they are added.
  typedef ObserverList<NotificationObserver> NotificationObserverList;
  typedef base::hash_map<std::string, NotificationObserverList*>
      SettingsObserverMap;
  SettingsObserverMap settings_observers_;

  CrosSettings();
  ~CrosSettings();
  CrosSettingsProvider* GetProvider(const std::string& path) const;
  friend struct base::DefaultLazyInstanceTraits<CrosSettings>;

  DISALLOW_COPY_AND_ASSIGN(CrosSettings);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SETTINGS_H_
