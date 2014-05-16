// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_PREFERENCES_H_

#include <string>
#include <vector>

#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/prefs/pref_service_syncable_observer.h"

class PrefRegistrySimple;
class PrefService;
class PrefServiceSyncable;

class TracingManager;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class User;

namespace input_method {
class InputMethodManager;
}

// The Preferences class handles Chrome OS preferences. When the class
// is first initialized, it will initialize the OS settings to what's stored in
// the preferences. These include touchpad settings, etc.
// When the preferences change, we change the settings to reflect the new value.
class Preferences : public PrefServiceSyncableObserver,
                    public ash::ShellObserver,
                    public UserManager::UserSessionStateObserver {
 public:
  Preferences();
  explicit Preferences(
      input_method::InputMethodManager* input_method_manager);  // for testing
  virtual ~Preferences();

  // These method will register the prefs associated with Chrome OS settings.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // This method will initialize Chrome OS settings to values in user prefs.
  // |user| is the user owning this preferences.
  void Init(PrefServiceSyncable* prefs, const User* user);

  void InitUserPrefsForTesting(PrefServiceSyncable* prefs, const User* user);
  void SetInputMethodListForTesting();

 private:
  enum ApplyReason {
    REASON_INITIALIZATION,
    REASON_ACTIVE_USER_CHANGED,
    REASON_PREF_CHANGED
  };

  // Initializes all member prefs.
  void InitUserPrefs(PrefServiceSyncable* prefs);

  // Callback method for preference changes.
  void OnPreferenceChanged(const std::string& pref_name);

  // This will set the OS settings when the preference changed or user owning
  // these preferences became active. Also this method is called on
  // initialization. The reason of call is stored in |reason| parameter.
  // |pref_name| keeps name of changed preference in |reason| is
  // |REASON_PREF_CHANGED|, otherwise it is empty.
  void ApplyPreferences(ApplyReason reason,
                        const std::string& pref_name);

  // A variant of SetLanguageConfigStringList. You can pass comma-separated
  // values. Examples of |value|: "", "Control+space,Hiragana"
  void SetLanguageConfigStringListAsCSV(const char* section,
                                        const char* name,
                                        const std::string& value);

  // Restores the user's preferred input method / keyboard layout on signing in.
  void SetInputMethodList();

  // Updates the initial key repeat delay and key repeat interval following
  // current prefs values. We set the delay and interval at once since an
  // underlying XKB API requires it.
  void UpdateAutoRepeatRate();

  // Force natural scroll to on if --enable-natural-scroll-default is specified
  // on the cmd line.
  void ForceNaturalScrollDefault();

  // PrefServiceSyncableObserver implementation.
  virtual void OnIsSyncingChanged() OVERRIDE;

  // Overriden from ash::ShellObserver.
  virtual void OnTouchHudProjectionToggled(bool enabled) OVERRIDE;

  // Overriden form UserManager::UserSessionStateObserver.
  virtual void ActiveUserChanged(const User* active_user) OVERRIDE;

  PrefServiceSyncable* prefs_;

  input_method::InputMethodManager* input_method_manager_;
  scoped_ptr<TracingManager> tracing_manager_;

  BooleanPrefMember performance_tracing_enabled_;
  BooleanPrefMember tap_to_click_enabled_;
  BooleanPrefMember tap_dragging_enabled_;
  BooleanPrefMember three_finger_click_enabled_;
  BooleanPrefMember natural_scroll_;
  BooleanPrefMember vert_edge_scroll_enabled_;
  IntegerPrefMember speed_factor_;
  IntegerPrefMember mouse_sensitivity_;
  IntegerPrefMember touchpad_sensitivity_;
  BooleanPrefMember primary_mouse_button_right_;
  FilePathPrefMember download_default_directory_;
  BooleanPrefMember touch_hud_projection_enabled_;

  // Input method preferences.
  StringPrefMember preload_engines_;
  StringPrefMember current_input_method_;
  StringPrefMember previous_input_method_;
  StringPrefMember enabled_extension_imes_;

  BooleanPrefMember xkb_auto_repeat_enabled_;
  IntegerPrefMember xkb_auto_repeat_delay_pref_;
  IntegerPrefMember xkb_auto_repeat_interval_pref_;

  // User owning these preferences.
  const User* user_;

  // Whether user is a primary user.
  bool user_is_primary_;

  DISALLOW_COPY_AND_ASSIGN(Preferences);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PREFERENCES_H_
