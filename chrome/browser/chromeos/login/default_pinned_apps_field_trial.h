// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_PINNED_APPS_FIELD_TRIAL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_PINNED_APPS_FIELD_TRIAL_H_

#include <string>

namespace base {
class ListValue;
}

class PrefRegistrySimple;

namespace chromeos {
namespace default_pinned_apps_field_trial {

// Enumeration to track clicks on the shelf.
enum ClickTarget {
  CHROME = 0,
  APP_LAUNCHER = 1,
  GMAIL = 2,
  SEARCH = 3,
  YOUTUBE = 4,
  DOC = 5,
  SHEETS = 6,
  SLIDES = 7,
  PLAY_MUSIC = 8,
  CLICK_TARGET_COUNT = 9
};

// Registers a local state that keeps track of users in experiment.
void RegisterPrefs(PrefRegistrySimple* registry);

// Creates the trail and setup groups.
void SetupTrial();

// Sets up the trial for a user. |username| is the email of the user.
// |is_new_user| is a boolean flag to indicate whether the user is new to
// the device. It is true when the user signs in the device for the first time.
void SetupForUser(const std::string& username, bool is_new_user);

// Returns the alternate default pinned apps if the current user is in
// "Alternate" group. Otherwise, returns NULL. The caller takes the ownership
// of the returned list.
base::ListValue* GetAlternateDefaultPinnedApps();

// Record UMA for shelf clicks.
void RecordShelfClick(ClickTarget click_target);
void RecordShelfAppClick(const std::string& app_id);

// Resets global states from test.
void ResetStateForTest();

}  // namespace default_pinned_apps_field_trial
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_PINNED_APPS_FIELD_TRIAL_H_
