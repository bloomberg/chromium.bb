// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

DefaultApps::DefaultApps(PrefService* prefs)
    : prefs_(prefs) {
#if !defined(OS_CHROMEOS)
  // Poppit, Entanglement
  ids_.insert("mcbkbpnkkkipelfledbfocopglifcfmi");
  ids_.insert("aciahcmjmecflokailenpkdchphgkefd");
#endif  // OS_CHROMEOS
}

DefaultApps::~DefaultApps() {}

const ExtensionIdSet* DefaultApps::GetAppsToInstall() const {
  if (GetDefaultAppsInstalled())
    return NULL;
  else
    return &ids_;
}

const ExtensionIdSet* DefaultApps::GetDefaultApps() const {
  return &ids_;
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

bool DefaultApps::CheckShouldShowPromo(const ExtensionIdSet& installed_ids) {
#if defined(OS_CHROMEOS)
  // Don't show the promo at all on Chrome OS.
  return false;
#endif
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceAppsPromoVisible)) {
    return true;
  }

  if (GetDefaultAppsInstalled() && GetPromoCounter() < kAppsPromoCounterMax) {
    // If we have the exact set of default apps, show the promo. If we don't
    // have the exact set of default apps, this means that the user manually
    // installed one. The promo doesn't make sense if it shows apps the user
    // manually installed, so expire it immediately in that situation.
    if (installed_ids == ids_)
      return true;
    else
      SetPromoHidden();
  }

  return false;
}

void DefaultApps::DidShowPromo() {
  if (!GetDefaultAppsInstalled()) {
    NOTREACHED() << "Should not show promo until default apps are installed.";
    return;
  }

  int promo_counter = GetPromoCounter();
  if (promo_counter == kAppsPromoCounterMax) {
    NOTREACHED() << "Promo has already been shown the maximum number of times.";
    return;
  }

  if (promo_counter < kAppsPromoCounterMax) {
    if (promo_counter + 1 == kAppsPromoCounterMax)
      UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppsPromoHistogram,
                                extension_misc::PROMO_EXPIRE,
                                extension_misc::PROMO_BUCKET_BOUNDARY);
    SetPromoCounter(++promo_counter);
  } else {
    SetPromoHidden();
  }
}

void DefaultApps::SetPromoHidden() {
  SetPromoCounter(kAppsPromoCounterMax);
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
