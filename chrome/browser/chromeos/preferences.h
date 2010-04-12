// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_PREFERENCES_H_

#include <string>
#include <vector>

#include "chrome/browser/pref_member.h"
#include "chrome/common/notification_observer.h"

class PrefService;

namespace chromeos {

// The Preferences class handles Chrome OS preferences. When the class
// is first initialized, it will initialize the OS settings to what's stored in
// the preferences. These include timezones, touchpad settings, etc.
// When the preferences change, we change the settings to reflect the new value.
class Preferences : public NotificationObserver {
 public:
  Preferences() {}
  virtual ~Preferences() {}

  // This method will register the prefs associated with Chrome OS settings.
  static void RegisterUserPrefs(PrefService* prefs);

  // This method will initialize Chrome OS settings to values in user prefs.
  void Init(PrefService* prefs);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // This will set the OS settings when the preference changes.
  // If this method is called with NULL, it will set all OS settings to what's
  // stored in the preferences.
  virtual void NotifyPrefChanged(const std::wstring* pref_name);

 private:
  void SetTimeZone(const std::wstring& id);

  // Writes boolean |value| to the IME (IBus) configuration daemon. |section|
  // (e.g. "general") and |name| (e.g. "use_global_engine") should not be NULL.
  void SetLanguageConfigBoolean(const char* section,
                                const char* name,
                                bool value);

  // Writes string |value| to the IME (IBus) configuration daemon. |section|
  // and |name| should not be NULL.
  void SetLanguageConfigString(const char* section,
                               const char* name,
                               const std::wstring& value);

  // Writes a string list to the IME (IBus) configuration daemon. |section|
  // and |name| should not be NULL.
  void SetLanguageConfigStringList(const char* section,
                                   const char* name,
                                   const std::vector<std::wstring>& values);

  // A variant of SetLanguageConfigStringList. You can pass comma-separated
  // values. Examples of |value|: "", "Control+space,Hiragana"
  void SetLanguageConfigStringListAsCSV(const char* section,
                                        const char* name,
                                        const std::wstring& value);

  StringPrefMember timezone_;
  BooleanPrefMember tap_to_click_enabled_;
  BooleanPrefMember vert_edge_scroll_enabled_;
  IntegerPrefMember speed_factor_;
  IntegerPrefMember sensitivity_;
  // Language (IME) preferences.
  BooleanPrefMember language_use_global_engine_;
  StringPrefMember language_hotkey_next_engine_;
  StringPrefMember language_hotkey_trigger_;
  StringPrefMember language_preload_engines_;
  StringPrefMember language_hangul_keyboard_;

  DISALLOW_COPY_AND_ASSIGN(Preferences);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PREFERENCES_H_
