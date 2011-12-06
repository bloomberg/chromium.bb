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

static bool ShouldInstallInProfile(Profile* profile) {
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

  default_apps::InstallState state =
      static_cast<default_apps::InstallState>(profile->GetPrefs()->GetInteger(
          prefs::kDefaultAppsInstallState));
  switch (state) {
    case default_apps::kUnknown: {
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
    case default_apps::kAlwaysProvideDefaultApps:
      install_apps = true;
      break;
    case default_apps::kNeverProvideDefaultApps:
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
  if (state == default_apps::kUnknown) {
    if (install_apps) {
      profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                      default_apps::kAlwaysProvideDefaultApps);
    } else {
      profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                      default_apps::kNeverProvideDefaultApps);
    }
    profile->GetPrefs()->ScheduleSavePersistentPrefs();
  }

  return install_apps;
}

namespace default_apps {

void RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kDefaultAppsInstallState, kUnknown,
                             PrefService::UNSYNCABLE_PREF);
}

Provider::Provider(Profile* profile,
                   VisitorInterface* service,
                   ExternalExtensionLoader* loader,
                   Extension::Location crx_location,
                   Extension::Location download_location,
                   int creation_flags)
    : ExternalExtensionProviderImpl(service, loader, crx_location,
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

  ExternalExtensionProviderImpl::VisitRegisteredExtension();
}

}  // namespace default_apps
