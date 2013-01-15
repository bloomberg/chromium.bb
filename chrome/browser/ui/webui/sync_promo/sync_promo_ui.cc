// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_handler.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_trial.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

namespace {

const char kStringsJsFile[] = "strings.js";
const char kSyncPromoJsFile[] = "sync_promo.js";

const char kSyncPromoQueryKeyAutoClose[] = "auto_close";
const char kSyncPromoQueryKeyContinue[] = "continue";
const char kSyncPromoQueryKeyNextPage[] = "next_page";
const char kSyncPromoQueryKeySource[] = "source";

// Gaia cannot support about:blank as a continue URL, so using a hosted blank
// page instead.
const char kContinueUrl[] =
    "https://www.google.com/intl/%s/chrome/blank.html?%s=%d";

// The maximum number of times we want to show the sync promo at startup.
const int kSyncPromoShowAtStartupMaximum = 10;

// Forces the web based signin flow when set.
bool g_force_web_based_signin_flow = false;

// Checks we want to show the sync promo for the given brand.
bool AllowPromoAtStartupForCurrentBrand() {
  std::string brand;
  google_util::GetBrand(&brand);

  if (brand.empty())
    return true;

  if (google_util::IsInternetCafeBrandCode(brand))
    return false;

  // Enable for both organic and distribution.
  return true;
}

ChromeWebUIDataSource* CreateSyncUIHTMLSource(content::WebUI* web_ui) {
  ChromeWebUIDataSource* html_source =
      new ChromeWebUIDataSource(chrome::kChromeUISyncPromoHost);
  DictionaryValue localized_strings;
  options::CoreOptionsHandler::GetStaticLocalizedValues(&localized_strings);
  SyncSetupHandler::GetStaticLocalizedValues(&localized_strings, web_ui);
  html_source->AddLocalizedStrings(localized_strings);
  html_source->set_json_path(kStringsJsFile);
  html_source->add_resource_path(kSyncPromoJsFile, IDR_SYNC_PROMO_JS);
  html_source->set_default_resource(IDR_SYNC_PROMO_HTML);
  html_source->set_use_json_js_format_v2();
  return html_source;
}

}  // namespace

SyncPromoUI::SyncPromoUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  SyncPromoHandler* handler = new SyncPromoHandler(
      g_browser_process->profile_manager());
  web_ui->AddMessageHandler(handler);

  // Set up the chrome://theme/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ThemeSource* theme = new ThemeSource(profile);
  ChromeURLDataManager::AddDataSource(profile, theme);

  // Set up the sync promo source.
  ChromeURLDataManager::AddDataSource(profile, CreateSyncUIHTMLSource(web_ui));

  sync_promo_trial::RecordUserShownPromo(web_ui);
}

// static
bool SyncPromoUI::HasShownPromoAtStartup(Profile* profile) {
  return profile->GetPrefs()->HasPrefPath(prefs::kSyncPromoStartupCount);
}

// static
bool SyncPromoUI::ShouldShowSyncPromo(Profile* profile) {
#if defined(OS_CHROMEOS)
  // There's no need to show the sync promo on cros since cros users are logged
  // into sync already.
  return false;
#endif

  // Don't bother if we don't have any kind of network connection.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

  // Honor the sync policies.
  if (!profile->GetOriginalProfile()->IsSyncAccessible())
    return false;

  // If the user is already signed into sync then don't show the promo.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          profile->GetOriginalProfile());
  if (!service || service->HasSyncSetupCompleted())
    return false;

  // Default to allow the promo.
  return true;
}

// static
void SyncPromoUI::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterIntegerPref(prefs::kSyncPromoStartupCount, 0,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSyncPromoUserSkipped, false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSyncPromoShowOnFirstRunAllowed, true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);

  SyncPromoHandler::RegisterUserPrefs(prefs);
}

// static
bool SyncPromoUI::ShouldShowSyncPromoAtStartup(Profile* profile,
                                               bool is_new_profile) {
  DCHECK(profile);

  if (!ShouldShowSyncPromo(profile))
    return false;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kNoFirstRun))
    is_new_profile = false;

  if (!is_new_profile) {
    if (!HasShownPromoAtStartup(profile))
      return false;
  }

  if (HasUserSkippedSyncPromo(profile))
    return false;

  // For Chinese users skip the sync promo.
  if (g_browser_process->GetApplicationLocale() == "zh-CN")
    return false;

  PrefService* prefs = profile->GetPrefs();
  int show_count = prefs->GetInteger(prefs::kSyncPromoStartupCount);
  if (show_count >= kSyncPromoShowAtStartupMaximum)
    return false;

  // This pref can be set in the master preferences file to allow or disallow
  // showing the sync promo at startup.
  if (prefs->HasPrefPath(prefs::kSyncPromoShowOnFirstRunAllowed))
    return prefs->GetBoolean(prefs::kSyncPromoShowOnFirstRunAllowed);

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

void SyncPromoUI::DidShowSyncPromoAtStartup(Profile* profile) {
  int show_count = profile->GetPrefs()->GetInteger(
      prefs::kSyncPromoStartupCount);
  show_count++;
  profile->GetPrefs()->SetInteger(prefs::kSyncPromoStartupCount, show_count);
}

bool SyncPromoUI::HasUserSkippedSyncPromo(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kSyncPromoUserSkipped);
}

void SyncPromoUI::SetUserSkippedSyncPromo(Profile* profile) {
  profile->GetPrefs()->SetBoolean(prefs::kSyncPromoUserSkipped, true);
}

// static
GURL SyncPromoUI::GetSyncPromoURL(const GURL& next_page,
                                  Source source,
                                  bool auto_close) {
  DCHECK_NE(SOURCE_UNKNOWN, source);

  std::string url_string;

  if (UseWebBasedSigninFlow()) {
    // Build a Gaia-based URL that can be used to sign the user into chrome.
    // There are required request parameters:
    //
    //  - tell Gaia which service the user is signing into.  In this case,
    //    a chrome sign in uses the service "chromiumsync"
    //  - provide a continue URL.  This is the URL that Gaia will redirect to
    //    once the sign is complete.
    //
    // The continue URL includes a source parameter that can be extracted using
    // the function GetSourceForSyncPromoURL() below.  This is used to know
    // which of the chrome sign in access points was used to sign the userr in.
    // See OneClickSigninHelper for details.
    url_string = GaiaUrls::GetInstance()->service_login_url();
    url_string.append("?service=chromiumsync&sarp=1&rm=hide");

    const std::string& locale = g_browser_process->GetApplicationLocale();
    std::string continue_url = base::StringPrintf(kContinueUrl, locale.c_str(),
        kSyncPromoQueryKeySource, static_cast<int>(source));

    base::StringAppendF(&url_string, "&%s=%s", kSyncPromoQueryKeyContinue,
                        net::EscapeQueryParamValue(
                            continue_url, false).c_str());
  } else {
    url_string = base::StringPrintf("%s?%s=%d", chrome::kChromeUISyncPromoURL,
                                    kSyncPromoQueryKeySource,
                                    static_cast<int>(source));

    if (auto_close)
      base::StringAppendF(&url_string, "&%s=1", kSyncPromoQueryKeyAutoClose);

    if (!next_page.spec().empty()) {
      base::StringAppendF(&url_string, "&%s=%s", kSyncPromoQueryKeyNextPage,
                          net::EscapeQueryParamValue(next_page.spec(),
                                                     false).c_str());
    }
  }

  return GURL(url_string);
}

// static
GURL SyncPromoUI::GetNextPageURLForSyncPromoURL(const GURL& url) {
  const char* key_name = UseWebBasedSigninFlow() ? kSyncPromoQueryKeyContinue :
      kSyncPromoQueryKeyNextPage;
  std::string value;
  if (chrome_common_net::GetValueForKeyInQuery(url, key_name, &value)) {
    return GURL(value);
  }
  return GURL();
}

// static
SyncPromoUI::Source SyncPromoUI::GetSourceForSyncPromoURL(const GURL& url) {
  std::string value;
  if (chrome_common_net::GetValueForKeyInQuery(
          url, kSyncPromoQueryKeySource, &value)) {
    int source = 0;
    if (base::StringToInt(value, &source) && source >= SOURCE_START_PAGE &&
        source < SOURCE_UNKNOWN) {
      return static_cast<Source>(source);
    }
  }
  return SOURCE_UNKNOWN;
}

// static
bool SyncPromoUI::GetAutoCloseForSyncPromoURL(const GURL& url) {
  std::string value;
  if (chrome_common_net::GetValueForKeyInQuery(
          url, kSyncPromoQueryKeyAutoClose, &value)) {
    int source = 0;
    base::StringToInt(value, &source);
    return (source == 1);
  }
  return false;
}

// static
bool SyncPromoUI::UseWebBasedSigninFlow() {
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseWebBasedSigninFlow) ||
      g_force_web_based_signin_flow;
#else
  return false;
#endif
}

// static
void SyncPromoUI::ForceWebBasedSigninFlowForTesting(bool force) {
  g_force_web_based_signin_flow = force;
}
