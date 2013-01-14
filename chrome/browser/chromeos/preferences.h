// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_PREFERENCES_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/prefs/public/pref_member.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/prefs/pref_service_syncable_observer.h"

class PrefService;
class PrefServiceSyncable;

namespace chromeos {
namespace input_method {
class InputMethodManager;
}  // namespace input_method

// The Preferences class handles Chrome OS preferences. When the class
// is first initialized, it will initialize the OS settings to what's stored in
// the preferences. These include touchpad settings, etc.
// When the preferences change, we change the settings to reflect the new value.
class Preferences : public PrefServiceSyncableObserver {
 public:
  Preferences();
  explicit Preferences(
      input_method::InputMethodManager* input_method_manager);  // for testing
  virtual ~Preferences();

  // This method will register the prefs associated with Chrome OS settings.
  static void RegisterUserPrefs(PrefServiceSyncable* prefs);

  // This method will initialize Chrome OS settings to values in user prefs.
  void Init(PrefServiceSyncable* prefs);

  void InitUserPrefsForTesting(PrefServiceSyncable* prefs);
  void SetInputMethodListForTesting();

 private:
  // Initializes all member prefs.
  void InitUserPrefs(PrefServiceSyncable* prefs);

  // Callback method for preference changes.
  void OnPreferenceChanged(const std::string& pref_name);

  // This will set the OS settings when the preference changes.
  // If this method is called with NULL, it will set all OS settings to what's
  // stored in the preferences.
  void NotifyPrefChanged(const std::string* pref_name);

  // Writes boolean |value| to the input method (IBus) configuration daemon.
  // |section| (e.g. "general") and |name| (e.g. "use_global_engine") should
  // not be NULL.
  void SetLanguageConfigBoolean(const char* section,
                                const char* name,
                                bool value);

  // Writes integer |value| to the input method (IBus) configuration daemon.
  // |section| and |name| should not be NULL.
  void SetLanguageConfigInteger(const char* section,
                                const char* name,
                                int value);

  // Writes string |value| to the input method (IBus) configuration daemon.
  // |section| and |name| should not be NULL.
  void SetLanguageConfigString(const char* section,
                               const char* name,
                               const std::string& value);

  // Writes a string list to the input method (IBus) configuration daemon.
  // |section| and |name| should not be NULL.
  void SetLanguageConfigStringList(const char* section,
                                   const char* name,
                                   const std::vector<std::string>& values);

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

  PrefServiceSyncable* prefs_;

  input_method::InputMethodManager* input_method_manager_;

  BooleanPrefMember tap_to_click_enabled_;
  BooleanPrefMember tap_dragging_enabled_;
  BooleanPrefMember three_finger_click_enabled_;
  BooleanPrefMember three_finger_swipe_enabled_;
  BooleanPrefMember natural_scroll_;
  BooleanPrefMember vert_edge_scroll_enabled_;
  BooleanPrefMember accessibility_enabled_;
  BooleanPrefMember screen_magnifier_enabled_;
  IntegerPrefMember screen_magnifier_type_;
  DoublePrefMember screen_magnifier_scale_;
  IntegerPrefMember speed_factor_;
  IntegerPrefMember mouse_sensitivity_;
  IntegerPrefMember touchpad_sensitivity_;
  BooleanPrefMember primary_mouse_button_right_;
  BooleanPrefMember use_24hour_clock_;
  BooleanPrefMember disable_drive_;
  BooleanPrefMember disable_drive_over_cellular_;
  BooleanPrefMember disable_drive_hosted_files_;
  FilePathPrefMember download_default_directory_;

  // Input method preferences.
  StringPrefMember preferred_languages_;
  StringPrefMember preload_engines_;
  StringPrefMember current_input_method_;
  StringPrefMember previous_input_method_;
  StringPrefMember filtered_extension_imes_;

  BooleanPrefMember chewing_boolean_prefs_[
      language_prefs::kNumChewingBooleanPrefs];
  StringPrefMember chewing_multiple_choice_prefs_[
      language_prefs::kNumChewingMultipleChoicePrefs];
  IntegerPrefMember chewing_hsu_sel_key_type_;
  IntegerPrefMember chewing_integer_prefs_[
      language_prefs::kNumChewingIntegerPrefs];
  StringPrefMember hangul_keyboard_;
  StringPrefMember hangul_hanja_binding_keys_;
  StringPrefMember hangul_hanja_keys_;
  BooleanPrefMember pinyin_boolean_prefs_[
      language_prefs::kNumPinyinBooleanPrefs];
  IntegerPrefMember pinyin_int_prefs_[
      language_prefs::kNumPinyinIntegerPrefs];
  IntegerPrefMember pinyin_double_pinyin_schema_;
  BooleanPrefMember mozc_boolean_prefs_[
      language_prefs::kNumMozcBooleanPrefs];
  StringPrefMember mozc_multiple_choice_prefs_[
      language_prefs::kNumMozcMultipleChoicePrefs];
  IntegerPrefMember mozc_integer_prefs_[
      language_prefs::kNumMozcIntegerPrefs];
  BooleanPrefMember xkb_auto_repeat_enabled_;
  IntegerPrefMember xkb_auto_repeat_delay_pref_;
  IntegerPrefMember xkb_auto_repeat_interval_pref_;

  BooleanPrefMember enable_screen_lock_;

  BooleanPrefMember enable_drm_;

  DISALLOW_COPY_AND_ASSIGN(Preferences);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PREFERENCES_H_
