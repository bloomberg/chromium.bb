// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/apps_promo.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/webui/shown_sections_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

const int AppsPromo::kDefaultAppsCounterMax = 10;

// static
void AppsPromo::RegisterPrefs(PrefService* local_state) {
  std::string empty;
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoId, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoHeader, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoButton, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoLink, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoExpire, empty);
}

// static
void AppsPromo::RegisterUserPrefs(PrefService* prefs) {
  // Set the default value for the counter to max+1 since we don't install
  // default apps for new users.
  prefs->RegisterIntegerPref(
      prefs::kAppsPromoCounter, kDefaultAppsCounterMax + 1);
  prefs->RegisterBooleanPref(prefs::kDefaultAppsInstalled, false);
  prefs->RegisterStringPref(prefs::kNTPWebStorePromoLastId, std::string());
}

// static
void AppsPromo::ClearPromo() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->ClearPref(prefs::kNTPWebStorePromoId);
  local_state->ClearPref(prefs::kNTPWebStorePromoHeader);
  local_state->ClearPref(prefs::kNTPWebStorePromoButton);
  local_state->ClearPref(prefs::kNTPWebStorePromoLink);
  local_state->ClearPref(prefs::kNTPWebStorePromoExpire);
}

// static
std::string AppsPromo::GetPromoButtonText() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetString(prefs::kNTPWebStorePromoButton);
}

// static
std::string AppsPromo::GetPromoId() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetString(prefs::kNTPWebStorePromoId);
}

// static
std::string AppsPromo::GetPromoHeaderText() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetString(prefs::kNTPWebStorePromoHeader);
}

// static
GURL AppsPromo::GetPromoLink() {
  PrefService* local_state = g_browser_process->local_state();
  return GURL(local_state->GetString(prefs::kNTPWebStorePromoLink));
}

// static
std::string AppsPromo::GetPromoExpireText() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetString(prefs::kNTPWebStorePromoExpire);
}

// static
void AppsPromo::SetPromo(const std::string& id,
                         const std::string& header_text,
                         const std::string& button_text,
                         const GURL& link,
                         const std::string& expire_text) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kNTPWebStorePromoId, id);
  local_state->SetString(prefs::kNTPWebStorePromoButton, button_text);
  local_state->SetString(prefs::kNTPWebStorePromoHeader, header_text);
  local_state->SetString(prefs::kNTPWebStorePromoLink, link.spec());
  local_state->SetString(prefs::kNTPWebStorePromoExpire, expire_text);
}

// static
bool AppsPromo::IsPromoSupportedForLocale() {
  PrefService* local_state = g_browser_process->local_state();
  // PromoResourceService will clear the promo data if the current locale is
  // not supported.
  return local_state->HasPrefPath(prefs::kNTPWebStorePromoId) &&
      local_state->HasPrefPath(prefs::kNTPWebStorePromoHeader) &&
      local_state->HasPrefPath(prefs::kNTPWebStorePromoButton) &&
      local_state->HasPrefPath(prefs::kNTPWebStorePromoLink) &&
      local_state->HasPrefPath(prefs::kNTPWebStorePromoExpire);
}

AppsPromo::AppsPromo(PrefService* prefs)
    : prefs_(prefs) {
  // Poppit, Entanglement
  old_default_app_ids_.insert("mcbkbpnkkkipelfledbfocopglifcfmi");
  old_default_app_ids_.insert("aciahcmjmecflokailenpkdchphgkefd");
}

AppsPromo::~AppsPromo() {}

bool AppsPromo::ShouldShowPromo(const ExtensionIdSet& installed_ids,
                                bool* just_expired) {
  *just_expired = false;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceAppsPromoVisible)) {
    return true;
  }

  // Don't show the promo if one wasn't served to this locale.
  if (!IsPromoSupportedForLocale())
    return false;

  int promo_counter = GetPromoCounter();
  if (GetDefaultAppsInstalled() && promo_counter <= kDefaultAppsCounterMax) {
    // If the default apps were installed from a previous Chrome version, we
    // should still show the promo. If we don't have the exact set of default
    // apps, this means that the user manually installed or uninstalled one.
    // We no longer keep track of the default apps once others have been
    // installed, so expire them immediately.
    if (old_default_app_ids_ != installed_ids) {
      ExpireDefaultApps();
      return false;
    }

    if (promo_counter == kDefaultAppsCounterMax) {
      *just_expired = true;

      // The default apps have expired due to inaction, so ping PROMO_EXPIRE.
      UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppsPromoHistogram,
                                extension_misc::PROMO_EXPIRE,
                                extension_misc::PROMO_BUCKET_BOUNDARY);

      ExpireDefaultApps();
      return true;
    } else {
      SetPromoCounter(++promo_counter);
      return true;
    }
  } else if (installed_ids.empty()) {
    return true;
  }

  return false;
}

bool AppsPromo::ShouldShowAppLauncher(const ExtensionIdSet& installed_ids) {
  // On Chrome OS the default apps are installed via a separate mechanism that
  // is always enabled. Therefore we always show the launcher.
#if defined(OS_CHROME)
  return true;
#else

  // Always show the app launcher if an app is installed.
  if (!installed_ids.empty())
    return true;

  // Otherwise, only show the app launcher if there's a promo for this locale.
  return IsPromoSupportedForLocale();
#endif
}

void AppsPromo::ExpireDefaultApps() {
  SetPromoCounter(kDefaultAppsCounterMax + 1);
}

void AppsPromo::MaximizeAppsIfFirstView() {
  std::string promo_id = GetPromoId();

  // Maximize the apps section of the NTP if this is the first time viewing the
  // specific promo.
  if (GetLastPromoId() != promo_id) {
    prefs_->SetString(prefs::kNTPWebStorePromoLastId, promo_id);
    ShownSectionsHandler::SetShownSection(prefs_, APPS);
  }
}

void AppsPromo::HidePromo() {
  UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppsPromoHistogram,
                            extension_misc::PROMO_CLOSE,
                            extension_misc::PROMO_BUCKET_BOUNDARY);

  // Put the apps section into menu mode, and maximize the recent section.
  ShownSectionsHandler::SetShownSection(prefs_, MENU_APPS);
  ShownSectionsHandler::SetShownSection(prefs_, THUMB);

  ExpireDefaultApps();
}

std::string AppsPromo::GetLastPromoId() {
  return prefs_->GetString(prefs::kNTPWebStorePromoLastId);
}

void AppsPromo::SetLastPromoId(const std::string& id) {
  prefs_->SetString(prefs::kNTPWebStorePromoLastId, id);
}

int AppsPromo::GetPromoCounter() const {
  return prefs_->GetInteger(prefs::kAppsPromoCounter);
}

void AppsPromo::SetPromoCounter(int val) {
  prefs_->SetInteger(prefs::kAppsPromoCounter, val);
  prefs_->ScheduleSavePersistentPrefs();
}

bool AppsPromo::GetDefaultAppsInstalled() const {
  return prefs_->GetBoolean(prefs::kDefaultAppsInstalled);
}
