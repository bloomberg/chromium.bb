// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/net/url_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::WebContents;
using net::GetValueForKeyInQuery;

namespace {

// Gaia cannot support about:blank as a continue URL, so using a hosted blank
// page instead.
const char kSignInLandingUrlPrefix[] =
    "https://www.google.com/intl/%s/chrome/blank.html";

// The maximum number of times we want to show the sign in promo at startup.
const int kSignInPromoShowAtStartupMaximum = 10;

// Forces the web based signin flow when set.
bool g_force_web_based_signin_flow = false;

// Checks we want to show the sign in promo for the given brand.
bool AllowPromoAtStartupForCurrentBrand() {
  std::string brand;
  google_brand::GetBrand(&brand);

  if (brand.empty())
    return true;

  if (google_brand::IsInternetCafeBrandCode(brand))
    return false;

  // Enable for both organic and distribution.
  return true;
}

// Returns true if a user has seen the sign in promo at startup previously.
bool HasShownPromoAtStartup(Profile* profile) {
  return profile->GetPrefs()->HasPrefPath(prefs::kSignInPromoStartupCount);
}

// Returns true if the user has previously skipped the sign in promo.
bool HasUserSkippedPromo(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kSignInPromoUserSkipped);
}

}  // namespace

namespace signin {

bool ShouldShowPromo(Profile* profile) {
#if defined(OS_CHROMEOS)
  // There's no need to show the sign in promo on cros since cros users are
  // already logged in.
  return false;
#else

  // Don't bother if we don't have any kind of network connection.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

  // Don't show for supervised profiles.
  if (profile->IsSupervised())
    return false;

  // Display the signin promo if the user is not signed in.
  SigninManager* signin = SigninManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
  return !signin->AuthInProgress() && signin->IsSigninAllowed() &&
      signin->GetAuthenticatedUsername().empty();
#endif
}

bool ShouldShowPromoAtStartup(Profile* profile, bool is_new_profile) {
  DCHECK(profile);

  // Don't show if the profile is an incognito.
  if (profile->IsOffTheRecord())
    return false;

  if (!ShouldShowPromo(profile))
    return false;

  if (!is_new_profile) {
    if (!HasShownPromoAtStartup(profile))
      return false;
  }

  if (HasUserSkippedPromo(profile))
    return false;

  // For Chinese users skip the sign in promo.
  if (g_browser_process->GetApplicationLocale() == "zh-CN")
    return false;

  PrefService* prefs = profile->GetPrefs();
  int show_count = prefs->GetInteger(prefs::kSignInPromoStartupCount);
  if (show_count >= kSignInPromoShowAtStartupMaximum)
    return false;

  // This pref can be set in the master preferences file to allow or disallow
  // showing the sign in promo at startup.
  if (prefs->HasPrefPath(prefs::kSignInPromoShowOnFirstRunAllowed))
    return prefs->GetBoolean(prefs::kSignInPromoShowOnFirstRunAllowed);

  // For now don't show the promo for some brands.
  if (!AllowPromoAtStartupForCurrentBrand())
    return false;

  // Default to show the promo for Google Chrome builds.
#if defined(GOOGLE_CHROME_BUILD)
  return true;
#else
  return false;
#endif
}

void DidShowPromoAtStartup(Profile* profile) {
  int show_count = profile->GetPrefs()->GetInteger(
      prefs::kSignInPromoStartupCount);
  show_count++;
  profile->GetPrefs()->SetInteger(prefs::kSignInPromoStartupCount, show_count);
}

void SetUserSkippedPromo(Profile* profile) {
  profile->GetPrefs()->SetBoolean(prefs::kSignInPromoUserSkipped, true);
}

GURL GetLandingURL(const char* option, int value) {
  const std::string& locale = g_browser_process->GetApplicationLocale();
  std::string url = base::StringPrintf(kSignInLandingUrlPrefix, locale.c_str());
  base::StringAppendF(&url, "?%s=%d", option, value);
  return GURL(url);
}

GURL GetPromoURL(Source source, bool auto_close) {
  return GetPromoURL(source, auto_close, false /* is_constrained */);
}

GURL GetPromoURL(Source source, bool auto_close, bool is_constrained) {
  return GetPromoURLWithContinueURL(source, auto_close, is_constrained, GURL());
}

GURL GetPromoURLWithContinueURL(Source source,
                                bool auto_close,
                                bool is_constrained,
                                GURL continue_url) {
  DCHECK_NE(SOURCE_UNKNOWN, source);

  if (!switches::IsEnableWebBasedSignin()) {
    std::string url(chrome::kChromeUIChromeSigninURL);
    base::StringAppendF(&url, "?%s=%d", kSignInPromoQueryKeySource, source);
    if (auto_close)
      base::StringAppendF(&url, "&%s=1", kSignInPromoQueryKeyAutoClose);
    if (is_constrained)
      base::StringAppendF(&url, "&%s=1", kSignInPromoQueryKeyConstrained);
    if (!continue_url.is_empty()) {
      DCHECK(continue_url.is_valid());
      std::string escaped_continue_url =
          net::EscapeQueryParamValue(continue_url.spec(), false);
      base::StringAppendF(&url,
                          "&%s=%s",
                          kSignInPromoQueryKeyContinue,
                          escaped_continue_url.c_str());
    }
    return GURL(url);
  }

  // Build a Gaia-based URL that can be used to sign the user into chrome.
  // There are required request parameters:
  //
  //  - tell Gaia which service the user is signing into.  In this case,
  //    a chrome sign in uses the service "chromiumsync"
  //  - provide a continue URL.  This is the URL that Gaia will redirect to
  //    once the sign is complete.
  //
  // The continue URL includes a source parameter that can be extracted using
  // the function GetSourceForSignInPromoURL() below.  This is used to know
  // which of the chrome sign in access points was used to sign the user in.
  // It is also parsed for the |auto_close| flag, which indicates that the tab
  // must be closed after sync setup is successful.
  // See OneClickSigninHelper for details.
  std::string query_string = "?service=chromiumsync&sarp=1";

  DCHECK(continue_url.is_empty());
  std::string continue_url_str = GetLandingURL(kSignInPromoQueryKeySource,
                                               static_cast<int>(source)).spec();
  if (auto_close) {
    base::StringAppendF(
        &continue_url_str, "&%s=1", kSignInPromoQueryKeyAutoClose);
  }

  base::StringAppendF(
      &query_string,
      "&%s=%s",
      kSignInPromoQueryKeyContinue,
      net::EscapeQueryParamValue(continue_url_str, false).c_str());

  return GaiaUrls::GetInstance()->service_login_url().Resolve(query_string);
}

GURL GetReauthURL(Profile* profile, const std::string& account_id) {
  if (switches::IsEnableWebBasedSignin()) {
    return net::AppendQueryParameter(
        signin::GetPromoURL(signin::SOURCE_SETTINGS, true),
        "Email",
        account_id);
  }

  signin::Source source = switches::IsNewAvatarMenu() ?
      signin::SOURCE_REAUTH : signin::SOURCE_SETTINGS;

  GURL url = signin::GetPromoURL(
      source, true /* auto_close */,
      switches::IsNewAvatarMenu() /* is_constrained */);
  url = net::AppendQueryParameter(url, "email", account_id);
  url = net::AppendQueryParameter(url, "validateEmail", "1");
  return net::AppendQueryParameter(url, "readOnlyEmail", "1");
}

GURL GetNextPageURLForPromoURL(const GURL& url) {
  std::string value;
  if (GetValueForKeyInQuery(url, kSignInPromoQueryKeyContinue, &value)) {
    GURL continue_url = GURL(value);
    if (continue_url.is_valid())
      return continue_url;
  }

  return GURL();
}

Source GetSourceForPromoURL(const GURL& url) {
  std::string value;
  if (GetValueForKeyInQuery(url, kSignInPromoQueryKeySource, &value)) {
    int source = 0;
    if (base::StringToInt(value, &source) && source >= SOURCE_START_PAGE &&
        source < SOURCE_UNKNOWN) {
      return static_cast<Source>(source);
    }
  }
  return SOURCE_UNKNOWN;
}

bool IsAutoCloseEnabledInURL(const GURL& url) {
  std::string value;
  if (GetValueForKeyInQuery(url, kSignInPromoQueryKeyAutoClose, &value)) {
    int enabled = 0;
    if (base::StringToInt(value, &enabled) && enabled == 1)
      return true;
  }
  return false;
}

bool ShouldShowAccountManagement(const GURL& url) {
  std::string value;
  if (GetValueForKeyInQuery(
          url, kSignInPromoQueryKeyShowAccountManagement, &value)) {
    int enabled = 0;
    if (base::StringToInt(value, &enabled) && enabled == 1)
      return true;
  }
  return false;
}

bool IsContinueUrlForWebBasedSigninFlow(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  const std::string& locale = g_browser_process->GetApplicationLocale();
  GURL continue_url =
      GURL(base::StringPrintf(kSignInLandingUrlPrefix, locale.c_str()));
  return (
      google_util::IsGoogleDomainUrl(
          url,
          google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS) &&
      url.ReplaceComponents(replacements).path() ==
        continue_url.ReplaceComponents(replacements).path());
}

void ForceWebBasedSigninFlowForTesting(bool force) {
  g_force_web_based_signin_flow = force;
}

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kSignInPromoStartupCount,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSignInPromoUserSkipped,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSignInPromoShowOnFirstRunAllowed,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSignInPromoShowNTPBubble,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace signin
