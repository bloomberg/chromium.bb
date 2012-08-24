// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/default_apps.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace default_apps {

bool ShouldInstallInProfile(Profile* profile) {
  // We decide to install or not install default apps based on the following
  // criteria, from highest priority to lowest priority:
  //
  // - If this instance of chrome is participating in the default apps
  //   field trial, then install apps based on the group.
  // - The command line option.  Tests use this option to disable installation
  //   of default apps in some cases.
  // - If the locale is not compatible with the defaults, don't install them.
  // - The kDefaultApps preferences value in the profile.  This value is
  //   usually set in the master_preferences file.
  bool install_apps =
      profile->GetPrefs()->GetString(prefs::kDefaultApps) == "install";

  default_apps::InstallState state =
      static_cast<default_apps::InstallState>(profile->GetPrefs()->GetInteger(
          prefs::kDefaultAppsInstallState));

  switch (state) {
    case default_apps::kUnknown: {
      // This is the first time the default apps feature runs on this profile.
      // Determine if we want to install them or not.
      chrome::VersionInfo version_info;
      if (!profile->WasCreatedByVersionOrLater(version_info.Version().c_str()))
        install_apps = false;
      break;
    }

    // The old default apps were provided as external extensions and were
    // installed everytime Chrome was run. Thus, changing the list of default
    // apps affected all users. Migrate old default apps to new mechanism where
    // they are installed only once as INTERNAL.
    // TODO(grv) : remove after Q1-2013.
    case default_apps::kProvideLegacyDefaultApps:
      profile->GetPrefs()->SetInteger(
          prefs::kDefaultAppsInstallState,
          default_apps::kAlreadyInstalledDefaultApps);
      break;

    case default_apps::kAlreadyInstalledDefaultApps:
    case default_apps::kNeverInstallDefaultApps:
      install_apps = false;
      break;
    default:
      NOTREACHED();
  }

  if (install_apps && !isLocaleSupported()) {
    install_apps = false;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDefaultApps)) {
    install_apps = false;
  }

  if (base::FieldTrialList::TrialExists(kDefaultAppsTrialName)) {
    install_apps = base::FieldTrialList::Find(
        kDefaultAppsTrialName)->group_name() != kDefaultAppsTrialNoAppsGroup;
  }

  // Default apps are only installed on profile creation or a new chrome
  // download.
  if (state == default_apps::kUnknown) {
    if (install_apps) {
      profile->GetPrefs()->SetInteger(
          prefs::kDefaultAppsInstallState,
          default_apps::kAlreadyInstalledDefaultApps);
    } else {
      profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                      default_apps::kNeverInstallDefaultApps);
    }
  }

  return install_apps;
}

void RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kDefaultAppsInstallState, kUnknown,
                             PrefService::UNSYNCABLE_PREF);
}

Provider::Provider(Profile* profile,
                   VisitorInterface* service,
                   extensions::ExternalLoader* loader,
                   extensions::Extension::Location crx_location,
                   extensions::Extension::Location download_location,
                   int creation_flags)
    : extensions::ExternalProviderImpl(service, loader, crx_location,
                                       download_location, creation_flags),
      profile_(profile) {
  DCHECK(profile);
  set_auto_acknowledge(true);
}

void Provider::VisitRegisteredExtension() {
  if (!profile_ || !ShouldInstallInProfile(profile_)) {
    base::DictionaryValue* prefs = new base::DictionaryValue;
    SetPrefs(prefs);
    return;
  }

  extensions::ExternalProviderImpl::VisitRegisteredExtension();
}

bool isLocaleSupported() {
  // Don't bother installing default apps in locales where it is known that
  // they don't work.
  // TODO(rogerta): Do this check dynamically once the webstore can expose
  // an API. See http://crbug.com/101357
  const std::string& locale = g_browser_process->GetApplicationLocale();
  static const char* unsupported_locales[] = {"CN", "TR", "IR"};
  for (size_t i = 0; i < arraysize(unsupported_locales); ++i) {
    if (EndsWith(locale, unsupported_locales[i], false)) {
      return false;
    }
  }
  return true;
}

}  // namespace default_apps
