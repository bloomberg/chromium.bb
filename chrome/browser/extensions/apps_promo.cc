// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/apps_promo.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/shown_sections_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/url_constants.h"
#include "content/common/notification_service.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

const int AppsPromo::kDefaultAppsCounterMax = 10;

namespace {

// The default logo for the promo.
const char kDefaultLogo[] = "chrome://theme/IDR_WEBSTORE_ICON";

// The default promo data (this is only used for testing with
// --force-apps-promo-visible).
const char kDefaultHeader[] = "Browse thousands of apps and games for Chrome";
const char kDefaultButton[] = "Visit the Chrome Web Store";
const char kDefaultExpire[] = "No thanks";
const char kDefaultLink[] = "https://chrome.google.com/webstore";

// Http success status code.
const int kHttpSuccess = 200;

// The match pattern for valid logo URLs.
const char kValidLogoPattern[] = "https://*.google.com/*.png";

// The prefix for 'data' URL images.
const char kPNGDataURLPrefix[] = "data:image/png;base64,";

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

AppsPromo::PromoData::PromoData() {}
AppsPromo::PromoData::PromoData(const std::string& id,
                                const std::string& header,
                                const std::string& button,
                                const GURL& link,
                                const std::string& expire,
                                const GURL& logo,
                                const int user_group)
    : id(id),
      header(header),
      button(button),
      link(link),
      expire(expire),
      logo(logo),
      user_group(user_group) {}

AppsPromo::PromoData::~PromoData() {}

// static
void AppsPromo::RegisterPrefs(PrefService* local_state) {
  std::string empty;
  local_state->RegisterBooleanPref(prefs::kNTPWebStoreEnabled, false);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoId, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoHeader, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoButton, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoLink, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoLogo, empty);
  local_state->RegisterStringPref(prefs::kNTPWebStorePromoLogoSource, empty);
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
void AppsPromo::SetWebStoreSupportedForLocale(bool supported) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(prefs::kNTPWebStoreEnabled, supported);
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
  local_state->ClearPref(prefs::kNTPWebStorePromoLogoSource);
  local_state->ClearPref(prefs::kNTPWebStorePromoExpire);
  local_state->ClearPref(prefs::kNTPWebStorePromoUserGroup);
}

// static
AppsPromo::PromoData AppsPromo::GetPromo() {
  PromoData data;
  PrefService* local_state = g_browser_process->local_state();

  data.id = GetStringPref(prefs::kNTPWebStorePromoId, "");
  data.link = GURL(GetStringPref(prefs::kNTPWebStorePromoLink, kDefaultLink));
  data.user_group = local_state->GetInteger(prefs::kNTPWebStorePromoUserGroup);
  data.header = GetStringPref(prefs::kNTPWebStorePromoHeader, kDefaultHeader);
  data.button = GetStringPref(prefs::kNTPWebStorePromoButton, kDefaultButton);
  data.expire = GetStringPref(prefs::kNTPWebStorePromoExpire, kDefaultExpire);

  GURL logo_url(local_state->GetString(prefs::kNTPWebStorePromoLogo));
  if (logo_url.is_valid() && logo_url.SchemeIs(chrome::kDataScheme))
    data.logo = logo_url;
  else
    data.logo = GURL(kDefaultLogo);

  return data;
}

// static
void AppsPromo::SetPromo(AppsPromo::PromoData data) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kNTPWebStorePromoId, data.id);
  local_state->SetString(prefs::kNTPWebStorePromoButton, data.button);
  local_state->SetString(prefs::kNTPWebStorePromoHeader, data.header);
  local_state->SetString(prefs::kNTPWebStorePromoLink, data.link.spec());
  local_state->SetString(prefs::kNTPWebStorePromoLogo, data.logo.spec());
  local_state->SetString(prefs::kNTPWebStorePromoExpire, data.expire);
  local_state->SetInteger(prefs::kNTPWebStorePromoUserGroup, data.user_group);
}

// static
GURL AppsPromo::GetSourcePromoLogoURL() {
  return GURL(GetStringPref(prefs::kNTPWebStorePromoLogoSource, ""));
}

// static
void AppsPromo::SetSourcePromoLogoURL(GURL logo_source) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kNTPWebStorePromoLogoSource,
                         logo_source.spec());
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
  PromoData promo = GetPromo();

  // Maximize the apps section of the NTP if this is the first time viewing the
  // specific promo and the current user group is targetted.
  if (GetLastPromoId() != promo.id) {
    if ((promo.user_group & GetCurrentUserGroup()) != 0)
      ShownSectionsHandler::SetShownSection(prefs_, APPS);
    SetLastPromoId(promo.id);
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

AppsPromoLogoFetcher::AppsPromoLogoFetcher(
    Profile* profile,
    AppsPromo::PromoData promo_data)
    : profile_(profile),
      promo_data_(promo_data) {
  if (SupportsLogoURL()) {
    if (HaveCachedLogo()) {
      promo_data_.logo = AppsPromo::GetPromo().logo;
      SavePromo();
    } else {
      FetchLogo();
    }
  } else {
    // We only care about the source URL when this fetches the logo.
    AppsPromo::SetSourcePromoLogoURL(GURL());
    SavePromo();
  }
}

AppsPromoLogoFetcher::~AppsPromoLogoFetcher() {}

void AppsPromoLogoFetcher::OnURLFetchComplete(const URLFetcher* source) {
  std::string data;
  std::string base64_data;

  CHECK(source == url_fetcher_.get());
  CHECK(source->GetResponseAsString(&data));

  if (source->status().is_success() &&
      source->response_code() == kHttpSuccess &&
      base::Base64Encode(data, &base64_data)) {
    AppsPromo::SetSourcePromoLogoURL(promo_data_.logo);
    promo_data_.logo = GURL(kPNGDataURLPrefix + base64_data);
  } else {
    // The logo wasn't downloaded correctly or we failed to encode it in
    // base64. Reset the source URL so we fetch it again next time. AppsPromo
    // will revert to the default logo.
    AppsPromo::SetSourcePromoLogoURL(GURL());
  }

  SavePromo();
}

void AppsPromoLogoFetcher::FetchLogo() {
  CHECK(promo_data_.logo.scheme() == chrome::kHttpsScheme);

  url_fetcher_.reset(URLFetcher::Create(
      0, promo_data_.logo, URLFetcher::GET, this));
  url_fetcher_->set_request_context(
      g_browser_process->system_request_context());
  url_fetcher_->set_load_flags(net::LOAD_DO_NOT_SEND_COOKIES |
                               net::LOAD_DO_NOT_SAVE_COOKIES);
  url_fetcher_->Start();
}

bool AppsPromoLogoFetcher::HaveCachedLogo() {
  return promo_data_.logo == AppsPromo::GetSourcePromoLogoURL();
}

void AppsPromoLogoFetcher::SavePromo() {
  AppsPromo::SetPromo(promo_data_);

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_STORE_PROMO_LOADED,
      Source<Profile>(profile_),
      NotificationService::NoDetails());
}

bool AppsPromoLogoFetcher::SupportsLogoURL() {
  URLPattern allowed_urls(URLPattern::SCHEME_HTTPS, kValidLogoPattern);
  return allowed_urls.MatchesURL(promo_data_.logo);
}
