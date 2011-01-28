// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/extensions/default_apps.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

const int DefaultApps::kAppsPromoCounterMax = 10;

// static
void DefaultApps::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kDefaultAppsInstalled, false);
  prefs->RegisterIntegerPref(prefs::kAppsPromoCounter, 0);
}

DefaultApps::DefaultApps(PrefService* prefs,
                         const std::string& application_locale)
    : prefs_(prefs), application_locale_(application_locale) {
  // Poppit, Entanglement
  ids_.insert("mcbkbpnkkkipelfledbfocopglifcfmi");
  ids_.insert("aciahcmjmecflokailenpkdchphgkefd");
}

DefaultApps::~DefaultApps() {}

const ExtensionIdSet& DefaultApps::default_apps() const {
  return ids_;
}

bool DefaultApps::DefaultAppSupported() {
  // On Chrome OS the default apps are installed via a different mechanism.
#if defined(OS_CHROMEOS)
  return false;
#else
  return DefaultAppsSupportedForLanguage();
#endif
}

bool DefaultApps::DefaultAppsSupportedForLanguage() {
  return application_locale_ == "en-US";
}

bool DefaultApps::ShouldInstallDefaultApps(
    const ExtensionIdSet& installed_ids) {
  if (!DefaultAppSupported())
    return false;

  if (GetDefaultAppsInstalled())
    return false;

  // If there are any non-default apps installed, we should never try to install
  // the default apps again, even if the non-default apps are later removed.
  if (NonDefaultAppIsInstalled(installed_ids)) {
    SetDefaultAppsInstalled(true);
    return false;
  }

  return true;
}

bool DefaultApps::ShouldShowAppLauncher(const ExtensionIdSet& installed_ids) {
  // On Chrome OS the default apps are installed via a separate mechanism that
  // is always enabled. Therefore we always show the launcher.
#if defined(OS_CHROME)
  return true;
#else
  // The app store only supports en-us at the moment, so we don't show the apps
  // section by default for users in other locales. But we do show it if a user
  // from a non-supported locale somehow installs an app (eg by navigating
  // directly to the store).
  if (!DefaultAppsSupportedForLanguage())
    return !installed_ids.empty();

  // For supported locales, we need to wait for all the default apps to be
  // installed before showing the apps section. We also show it if any
  // non-default app is installed (eg because the user installed the app in a
  // previous version of Chrome).
  if (GetDefaultAppsInstalled() || NonDefaultAppIsInstalled(installed_ids))
    return true;
  else
    return false;
#endif
}

bool DefaultApps::ShouldShowPromo(const ExtensionIdSet& installed_ids,
                                  bool* just_expired) {
  *just_expired = false;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceAppsPromoVisible)) {
    return true;
  }

  if (!DefaultAppSupported())
    return false;

  if (!GetDefaultAppsInstalled())
    return false;

  int promo_counter = GetPromoCounter();
  if (promo_counter <= kAppsPromoCounterMax) {
    // If we have the exact set of default apps, show the promo. If we don't
    // have the exact set of default apps, this means that the user manually
    // installed or uninstalled one. The promo doesn't make sense if it shows
    // apps the user manually installed, so expire it immediately in that
    // situation.
    if (ids_ != installed_ids) {
      SetPromoHidden();
      return false;
    }

    if (promo_counter == kAppsPromoCounterMax) {
      *just_expired = true;
      UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppsPromoHistogram,
                                extension_misc::PROMO_EXPIRE,
                                extension_misc::PROMO_BUCKET_BOUNDARY);
      SetPromoCounter(++promo_counter);
      return false;
    } else {
      UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppsPromoHistogram,
                                extension_misc::PROMO_SEEN,
                                extension_misc::PROMO_BUCKET_BOUNDARY);
      SetPromoCounter(++promo_counter);
      return true;
    }
  }

  return false;
}

void DefaultApps::DidInstallApp(const ExtensionIdSet& installed_ids) {
  // If all the default apps have been installed, stop trying to install them.
  // Note that we use std::includes here instead of == because apps might have
  // been manually installed while the the default apps were installing and we
  // wouldn't want to keep trying to install them in that case.
  if (!GetDefaultAppsInstalled() &&
      std::includes(installed_ids.begin(), installed_ids.end(),
                    ids_.begin(), ids_.end())) {
    SetDefaultAppsInstalled(true);
  }
}

bool DefaultApps::NonDefaultAppIsInstalled(
    const ExtensionIdSet& installed_ids) const {
  for (ExtensionIdSet::const_iterator iter = installed_ids.begin();
       iter != installed_ids.end(); ++iter) {
    if (ids_.find(*iter) == ids_.end())
      return true;
  }

  return false;
}

void DefaultApps::SetPromoHidden() {
  SetPromoCounter(kAppsPromoCounterMax + 1);
}

int DefaultApps::GetPromoCounter() const {
  return prefs_->GetInteger(prefs::kAppsPromoCounter);
}

void DefaultApps::SetPromoCounter(int val) {
  prefs_->SetInteger(prefs::kAppsPromoCounter, val);
  prefs_->ScheduleSavePersistentPrefs();
}

bool DefaultApps::GetDefaultAppsInstalled() const {
  return prefs_->GetBoolean(prefs::kDefaultAppsInstalled);
}

void DefaultApps::SetDefaultAppsInstalled(bool val) {
  prefs_->SetBoolean(prefs::kDefaultAppsInstalled, val);
  prefs_->ScheduleSavePersistentPrefs();
}
