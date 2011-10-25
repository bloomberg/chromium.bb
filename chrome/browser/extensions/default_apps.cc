// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/default_apps.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace default_apps {

void RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kDefaultAppsInstallState, kUnknown,
                             PrefService::UNSYNCABLE_PREF);
}

bool ShouldInstallInProfile(Profile* profile) {
  // We decide to install or not install default apps based on the following
  // criteria, from highest priority to lowest priority:
  //
  // - If this instance of chrome is participating in the default apps
  //   field trial, then install apps based on the group.
  // - The command line option.  Tests use this option to disable installation
  //   of default apps in some cases.
  // - If the locale is not compatible with the defaults, don't install them.
  // - If the profile says to either always install or never install default
  //   apps, obey.
  // - The kDefaultApps preferences value in the profile.  This value is
  //   usually set in the master_preferences file.
  bool install_apps =
      profile->GetPrefs()->GetString(prefs::kDefaultApps) == "install";

  InstallState state = static_cast<InstallState>(profile->GetPrefs()->
      GetInteger(prefs::kDefaultAppsInstallState));
  switch (state) {
    case kUnknown: {
      // We get here for either new profile, or profiles created before the
      // default apps feature was implemented.  In the former case, we always
      // want to install default apps.  In the latter case, we don't want to
      // disturb a user that has already installed and possibly curated a list
      // of favourite apps, so we only install if there are no apps in the
      // profile.  We can check for both these cases by looking to see if
      // any apps already exist.
      ExtensionService* extension_service = profile->GetExtensionService();
      if (extension_service && extension_service->HasApps())
        install_apps = false;
      break;
    }
    case kAlwaysProvideDefaultApps:
      install_apps = true;
      break;
    case kNeverProvideDefaultApps:
      install_apps = false;
      break;
    default:
      NOTREACHED();
  }

  if (install_apps) {
    // Don't bother installing default apps in locales where it is known that
    // they don't work.
    // TODO(rogerta): Do this check dynamically once the webstore can expose
    // an API. See http://crbug.com/101357
    const std::string& locale = g_browser_process->GetApplicationLocale();
    static const char* unsupported_locales[] = {"CN", "TR", "IR"};
    for (size_t i = 0; i < arraysize(unsupported_locales); ++i) {
      if (EndsWith(locale, unsupported_locales[i], false)) {
        install_apps = false;
        break;
      }
    }
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDefaultApps)) {
    install_apps = false;
  }

  if (base::FieldTrialList::TrialExists(kDefaultAppsTrial_Name)) {
    install_apps = base::FieldTrialList::Find(
        kDefaultAppsTrial_Name)->group_name() != kDefaultAppsTrial_NoAppsGroup;
  }

  // Save the state if needed.
  if (state == kUnknown) {
    if (install_apps) {
      profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                      kAlwaysProvideDefaultApps);
    } else {
      profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                      kNeverProvideDefaultApps);
    }
    profile->GetPrefs()->ScheduleSavePersistentPrefs();
  }

  return install_apps;
}

}  // namespace default_apps
