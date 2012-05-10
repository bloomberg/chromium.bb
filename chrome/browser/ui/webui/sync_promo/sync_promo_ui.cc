// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
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
#include "chrome/browser/ui/webui/options2/core_options_handler2.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_handler.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_trial.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/net/url_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "googleurl/src/url_util.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

namespace {

const char kStringsJsFile[] = "strings.js";
const char kSyncPromoJsFile[] = "sync_promo.js";

const char kSyncPromoQueryKeyNextPage[] = "next_page";
const char kSyncPromoQueryKeySource[] = "source";

// The maximum number of times we want to show the sync promo at startup.
const int kSyncPromoShowAtStartupMaximum = 10;

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

// The Web UI data source for the sync promo page.
class SyncPromoUIHTMLSource : public ChromeWebUIDataSource {
 public:
  explicit SyncPromoUIHTMLSource(content::WebUI* web_ui);

 private:
  ~SyncPromoUIHTMLSource() {}
  DISALLOW_COPY_AND_ASSIGN(SyncPromoUIHTMLSource);
};

SyncPromoUIHTMLSource::SyncPromoUIHTMLSource(content::WebUI* web_ui)
    : ChromeWebUIDataSource(chrome::kChromeUISyncPromoHost) {
  DictionaryValue localized_strings;
  options2::CoreOptionsHandler::GetStaticLocalizedValues(&localized_strings);
  SyncSetupHandler::GetStaticLocalizedValues(&localized_strings, web_ui);
  AddLocalizedStrings(localized_strings);
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
  SyncPromoUIHTMLSource* html_source = new SyncPromoUIHTMLSource(web_ui);
  html_source->set_json_path(kStringsJsFile);
  html_source->add_resource_path(kSyncPromoJsFile, IDR_SYNC_PROMO_JS);
  html_source->set_default_resource(IDR_SYNC_PROMO_HTML);
  ChromeURLDataManager::AddDataSource(profile, html_source);

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
void SyncPromoUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(
      prefs::kSyncPromoStartupCount, 0, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(
      prefs::kSyncPromoUserSkipped, false, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSyncPromoShowOnFirstRunAllowed, true,
      PrefService::UNSYNCABLE_PREF);

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
GURL SyncPromoUI::GetSyncPromoURL(const GURL& next_page, Source source) {
  DCHECK_NE(SOURCE_UNKNOWN, source);

  std::stringstream stream;
  stream << chrome::kChromeUISyncPromoURL << "?"
         << kSyncPromoQueryKeySource << "=" << static_cast<int>(source);

  if (!next_page.spec().empty()) {
    url_canon::RawCanonOutputT<char> output;
    url_util::EncodeURIComponent(
        next_page.spec().c_str(), next_page.spec().length(), &output);
    std::string escaped_spec(output.data(), output.length());
    stream << "&" << kSyncPromoQueryKeyNextPage << "=" << escaped_spec;
  }

  return GURL(stream.str());
}

// static
GURL SyncPromoUI::GetNextPageURLForSyncPromoURL(const GURL& url) {
  std::string value;
  if (chrome_common_net::GetValueForKeyInQuery(
          url, kSyncPromoQueryKeyNextPage, &value)) {
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
