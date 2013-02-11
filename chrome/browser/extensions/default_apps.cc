// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/default_apps.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#if !defined(OS_ANDROID)
#include "chrome/browser/first_run/first_run.h"
#endif
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kGmailId[] = "pjkljhegncpnkpknbcohdijeoejaedia";
const char kSearchId[] = "coobgpohoikkiipiblmjeljniedjpjpf";
const char kYoutubeId[] = "blpcfgokakmgnkcojhhkbfbldkacnbeo";

// Returns true if the app was a default app in Chrome 22
bool IsOldDefaultApp(const std::string& extension_id) {
  return extension_id == kGmailId || extension_id == kSearchId
      || extension_id == kYoutubeId;
}

bool IsLocaleSupported() {
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

} // namespace

namespace default_apps {

void RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kDefaultAppsInstallState, kUnknown,
                                PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool Provider::ShouldInstallInProfile() {
  // We decide to install or not install default apps based on the following
  // criteria, from highest priority to lowest priority:
  //
  // - The command line option.  Tests use this option to disable installation
  //   of default apps in some cases.
  // - If the locale is not compatible with the defaults, don't install them.
  // - The kDefaultApps preferences value in the profile.  This value is
  //   usually set in the master_preferences file.
  bool install_apps =
      profile_->GetPrefs()->GetString(prefs::kDefaultApps) == "install";

  InstallState state =
      static_cast<InstallState>(profile_->GetPrefs()->GetInteger(
          prefs::kDefaultAppsInstallState));

  is_migration_ = (state == kProvideLegacyDefaultApps);

  switch (state) {
    case kUnknown: {
      // Only new installations and profiles get default apps. In theory the
      // new profile checks should catch new installations, but that is not
      // always the case (http:/crbug.com/145351).
      chrome::VersionInfo version_info;
      bool is_new_profile =
          profile_->WasCreatedByVersionOrLater(version_info.Version().c_str());
      // Android excludes most of the first run code, so it can't determine
      // if this is a first run. That's OK though, because Android doesn't
      // use default apps in general.
#if defined(OS_ANDROID)
      bool is_first_run = false;
#else
      bool is_first_run = first_run::IsChromeFirstRun();
#endif
      if (!is_first_run && !is_new_profile)
        install_apps = false;
      break;
    }

    // The old default apps were provided as external extensions and were
    // installed everytime Chrome was run. Thus, changing the list of default
    // apps affected all users. Migrate old default apps to new mechanism where
    // they are installed only once as INTERNAL.
    // TODO(grv) : remove after Q1-2013.
    case kProvideLegacyDefaultApps:
      profile_->GetPrefs()->SetInteger(
          prefs::kDefaultAppsInstallState,
          kAlreadyInstalledDefaultApps);
      break;

    case kAlreadyInstalledDefaultApps:
    case kNeverInstallDefaultApps:
      install_apps = false;
      break;
    default:
      NOTREACHED();
  }

  if (install_apps && !IsLocaleSupported())
    install_apps = false;

  // Default apps are only installed on profile creation or a new chrome
  // download.
  if (state == kUnknown) {
    if (install_apps) {
      profile_->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                       kAlreadyInstalledDefaultApps);
    } else {
      profile_->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                       kNeverInstallDefaultApps);
    }
  }

  return install_apps;
}

Provider::Provider(Profile* profile,
                   VisitorInterface* service,
                   extensions::ExternalLoader* loader,
                   extensions::Manifest::Location crx_location,
                   extensions::Manifest::Location download_location,
                   int creation_flags)
    : extensions::ExternalProviderImpl(service, loader, crx_location,
                                       download_location, creation_flags),
      profile_(profile),
      is_migration_(false) {
  DCHECK(profile);
  set_auto_acknowledge(true);
}

void Provider::VisitRegisteredExtension() {
  if (!profile_ || !ShouldInstallInProfile()) {
    base::DictionaryValue* prefs = new base::DictionaryValue;
    SetPrefs(prefs);
    return;
  }

  extensions::ExternalProviderImpl::VisitRegisteredExtension();
}

void Provider::SetPrefs(base::DictionaryValue* prefs) {
  if (is_migration_) {
    std::set<std::string> new_default_apps;
    for (base::DictionaryValue::key_iterator i = prefs->begin_keys();
         i != prefs->end_keys(); ++i) {
      if (!IsOldDefaultApp(*i)) {
        new_default_apps.insert(*i);
      }
    }
    // Filter out the new default apps for migrating users.
    for (std::set<std::string>::iterator it = new_default_apps.begin();
         it != new_default_apps.end(); ++it) {
      prefs->Remove(*it, NULL);
    }
  }

  ExternalProviderImpl::SetPrefs(prefs);
}

}  // namespace default_apps
