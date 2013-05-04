// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/default_pinned_apps_field_trial.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"

namespace chromeos {
namespace default_pinned_apps_field_trial {

namespace {

// Name of the default pinned app experiment.
const char kExperimentName[] = "DefaultPinnedApps";

// Name of a local state pref to store the list of users that participate
// the experiment.
const char kExperimentUsers[] = "DefaultPinnedAppsExperimentUsers";

// Alternate default pinned apps.
const char* kAlternateDefaultPinnedApps[] = {
  extension_misc::kGmailAppId,
  extension_misc::kGoogleDocAppId,
  extension_misc::kGoogleSheetsAppId,
  extension_misc::kGoogleSlidesAppId,
  extension_misc::kGooglePlayMusicAppId,
};

// Global state of trial setup.
bool trial_configured = false;
int alternate_group = base::FieldTrial::kNotFinalized;

// Returns true if user participates the experiment.
bool IsUserInExperiment(const std::string& username) {
  const base::ListValue* users =
      g_browser_process->local_state()->GetList(kExperimentUsers);
  return users && users->Find(base::StringValue(username)) != users->end();
}

// Adds user to the experiment user list.
void AddUserToExperiment(const std::string& username) {
  ListPrefUpdate users(g_browser_process->local_state(), kExperimentUsers);
  users->AppendString(username);
}

// Gets click target type for given app id. Returns false if the app id is
// not interesting.
bool GetClickTargetForApp(const std::string& app_id,
                          ClickTarget* click_target) {
  static const struct AppIdToClickTarget {
    const char* app_id;
    ClickTarget click_target;
  } kApps[] = {
      { extension_misc::kChromeAppId, CHROME },
      { extension_misc::kGmailAppId, GMAIL },
      { extension_misc::kGoogleSearchAppId, SEARCH },
      { extension_misc::kYoutubeAppId, YOUTUBE },
      { extension_misc::kGoogleDocAppId, DOC },
      { extension_misc::kGoogleSheetsAppId, SHEETS },
      { extension_misc::kGoogleSlidesAppId, SLIDES },
      { extension_misc::kGooglePlayMusicAppId, PLAY_MUSIC },
  };

  // kApps should define an app id for all click types except APP_LAUNCHER.
  COMPILE_ASSERT(ARRAYSIZE_UNSAFE(kApps) == CLICK_TARGET_COUNT - 1,
                 app_to_click_target_incorrect_size);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kApps); ++i) {
    if (app_id == kApps[i].app_id) {
      *click_target = kApps[i].click_target;
      return true;
    }
  }

  return false;
}

}  // namespace

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kExperimentUsers);
}

void SetupTrial() {
  // No trial if multiple profiles are enabled.
  if (CommandLine::ForCurrentProcess()->HasSwitch(::switches::kMultiProfiles))
    return;

  // SetupForUser should only be called once for single profile mode.
  DCHECK(!trial_configured);

  const base::FieldTrial::Probability kDivisor = 100;
  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          kExperimentName, kDivisor, "Existing", 2013, 12, 31, NULL);
  trial->UseOneTimeRandomization();

  // Split 50/50 between "Control" and "Alternamte" group for new user.
  // Existing users already have their default pinned apps and have the trial
  // disabled to go to "Existing" group.
  trial->AppendGroup("Control", 50);
  alternate_group = trial->AppendGroup("Alternate", 50);
}

void SetupForUser(const std::string& username, bool is_new_user) {
  base::FieldTrial* trial = base::FieldTrialList::Find(kExperimentName);
  if (!trial)
    return;

  trial_configured = true;

  if (is_new_user) {
    AddUserToExperiment(username);
    return;
  }

  if (!IsUserInExperiment(username))
    trial->Disable();
}

base::ListValue* GetAlternateDefaultPinnedApps() {
  // This function is called for login manager profile, which happens
  // before any user signs in. Returns NULL in this case. The case is covered
  // by NULL trial check below but checking a boolean flag is faster.
  if (!trial_configured)
    return NULL;

  base::FieldTrial* trial = base::FieldTrialList::Find(kExperimentName);
  if (!trial || trial->group() != alternate_group)
    return NULL;

  scoped_ptr<base::ListValue> apps(new base::ListValue);
  for (size_t i = 0; i < arraysize(kAlternateDefaultPinnedApps); ++i)
    apps->Append(ash::CreateAppDict(kAlternateDefaultPinnedApps[i]));

  return apps.release();
}

void RecordShelfClick(ClickTarget click_target) {
  // The experiment does not include certain user types, such as public account,
  // retail mode user, local user and ephemeral user.
  if (!trial_configured)
    return;

  UMA_HISTOGRAM_ENUMERATION("Cros.ClickOnShelf",
                            click_target,
                            CLICK_TARGET_COUNT);
}

void RecordShelfAppClick(const std::string& app_id) {
  ClickTarget click_target = CLICK_TARGET_COUNT;
  if (GetClickTargetForApp(app_id, &click_target))
    RecordShelfClick(click_target);
}

void ResetStateForTest() {
  trial_configured = false;
  alternate_group = base::FieldTrial::kNotFinalized;
}

}  // namespace default_pinned_apps_field_trial
}  // namespace chromeos
