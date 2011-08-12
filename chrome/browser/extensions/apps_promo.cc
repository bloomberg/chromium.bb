// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/apps_promo.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/webui/ntp/shown_sections_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

const int AppsPromo::kDefaultAppsCounterMax = 10;

namespace {

// The default logo for the promo.
const char kDefaultPromoLogo[] = "chrome://theme/IDR_WEBSTORE_ICON";

// Returns the string pref at |path|, using |fallback| as the default (if there
// is no pref value present). |fallback| is used for debugging in concert with
// --force-apps-promo-visible.
std::string GetStringPref(const char* path, const std::string& fallback) {
  PrefService* local_state = g_browser_process->local_state();
  std::string retval(local_state->GetString(path));
  if (retval.empty() && CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceAppsPromoVisible)) {
    retval = fallback;
  }
  return retval;
}

} // namespace

// static
void AppsPromo::RegisterPrefs(PrefService* local_state) {
  std::string empty;
  local_state->RegisterBooleanPref(prefs::kNTPWebStoreEnabled, false);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoId, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoHeader, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoButton, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoLink, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoLogo, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoExpire, empty);
  local_state->RegisterIntegerPref(prefs::kNTPWebStorePromoUserGroup, 0);
}

// static
void AppsPromo::RegisterUserPrefs(PrefService* prefs) {
  // Set the default value for the counter to max+1 since we don't install
  // default apps for new users.
  prefs->RegisterIntegerPref(prefs::kAppsPromoCounter,
                             kDefaultAppsCounterMax + 1,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDefaultAppsInstalled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kNTPWebStorePromoLastId,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kNTPHideWebStorePromo,
                            false,
                            PrefService::UNSYNCABLE_PREF);
}

// static
void AppsPromo::ClearPromo() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->ClearPref(prefs::kNTPWebStoreEnabled);
  local_state->ClearPref(prefs::kNTPWebStorePromoId);
  local_state->ClearPref(prefs::kNTPWebStorePromoHeader);
  local_state->ClearPref(prefs::kNTPWebStorePromoButton);
  local_state->ClearPref(prefs::kNTPWebStorePromoLink);
  local_state->ClearPref(prefs::kNTPWebStorePromoLogo);
  local_state->ClearPref(prefs::kNTPWebStorePromoExpire);
  local_state->ClearPref(prefs::kNTPWebStorePromoUserGroup);
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
      local_state->HasPrefPath(prefs::kNTPWebStorePromoLogo) &&
      local_state->HasPrefPath(prefs::kNTPWebStorePromoExpire) &&
      local_state->HasPrefPath(prefs::kNTPWebStorePromoUserGroup);
}

// static
bool AppsPromo::IsWebStoreSupportedForLocale() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetBoolean(prefs::kNTPWebStoreEnabled);
}

// static
std::string AppsPromo::GetPromoButtonText() {
  return GetStringPref(prefs::kNTPWebStorePromoButton, "Click here now");
}

// static
std::string AppsPromo::GetPromoId() {
  return GetStringPref(prefs::kNTPWebStorePromoId, "");
}

// static
std::string AppsPromo::GetPromoHeaderText() {
  return GetStringPref(prefs::kNTPWebStorePromoHeader, "Get great apps!");
}

// static
GURL AppsPromo::GetPromoLink() {
  return GURL(GetStringPref(prefs::kNTPWebStorePromoLink,
                            "https://chrome.google.com/webstore"));
}

// static
GURL AppsPromo::GetPromoLogo() {
  PrefService* local_state = g_browser_process->local_state();
  GURL logo_url(local_state->GetString(prefs::kNTPWebStorePromoLogo));
  if (logo_url.is_valid() && logo_url.SchemeIs("data"))
    return logo_url;
  return GURL(kDefaultPromoLogo);
}

// static
std::string AppsPromo::GetPromoExpireText() {
  return GetStringPref(prefs::kNTPWebStorePromoExpire, "No thanks.");
}

// static
int AppsPromo::GetPromoUserGroup() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetInteger(prefs::kNTPWebStorePromoUserGroup);
}

// static
void AppsPromo::SetPromo(const std::string& id,
                         const std::string& header_text,
                         const std::string& button_text,
                         const GURL& link,
                         const std::string& expire_text,
                         const GURL& logo,
                         const int user_group) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kNTPWebStorePromoId, id);
  local_state->SetString(prefs::kNTPWebStorePromoButton, button_text);
  local_state->SetString(prefs::kNTPWebStorePromoHeader, header_text);
  local_state->SetString(prefs::kNTPWebStorePromoLink, link.spec());
  local_state->SetString(prefs::kNTPWebStorePromoLogo, logo.spec());
  local_state->SetString(prefs::kNTPWebStorePromoExpire, expire_text);
  local_state->SetInteger(prefs::kNTPWebStorePromoUserGroup, user_group);
}

// static
void AppsPromo::SetWebStoreSupportedForLocale(bool supported) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kNTPWebStoreEnabled, supported);
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

  // Don't show the promo if the policy says not to.
  if (prefs_->GetBoolean(prefs::kNTPHideWebStorePromo)) {
    ExpireDefaultApps();
    return false;
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
    } else {
      SetPromoCounter(++promo_counter);
    }
    return true;
  } else if (installed_ids.empty()) {
    return true;
  }

  return false;
}

bool AppsPromo::ShouldShowAppLauncher(const ExtensionIdSet& installed_ids) {
  // On Chrome OS the default apps are installed via a separate mechanism that
  // is always enabled. Therefore we always show the launcher.
#if defined(OS_CHROMEOS)
  return true;
#else

  // Always show the app launcher if an app is installed.
  if (!installed_ids.empty())
    return true;

  // Otherwise, only show the app launcher if the web store is supported for the
  // current locale.
  return IsWebStoreSupportedForLocale();
#endif
}

void AppsPromo::ExpireDefaultApps() {
  SetPromoCounter(kDefaultAppsCounterMax + 1);
}

void AppsPromo::MaximizeAppsIfNecessary() {
  std::string promo_id = GetPromoId();
  int maximize_setting = GetPromoUserGroup();

  // Maximize the apps section of the NTP if this is the first time viewing the
  // specific promo and the current user group is targetted.
  if (GetLastPromoId() != promo_id) {
    if ((maximize_setting & GetCurrentUserGroup()) != 0)
      ShownSectionsHandler::SetShownSection(prefs_, APPS);
    SetLastPromoId(promo_id);
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

AppsPromo::UserGroup AppsPromo::GetCurrentUserGroup() const {
  const PrefService::Preference* last_promo_id
      = prefs_->FindPreference(prefs::kNTPWebStorePromoLastId);
  CHECK(last_promo_id);
  return last_promo_id->IsDefaultValue() ? USERS_NEW : USERS_EXISTING;
}
