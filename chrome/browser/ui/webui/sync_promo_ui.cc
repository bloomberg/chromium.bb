// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo_ui.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/options/core_options_handler.h"
#include "chrome/browser/ui/webui/sync_promo_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/url_util.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kStringsJsFile[] = "strings.js";
const char kSyncPromoJsFile[]  = "sync_promo.js";

const char kSyncPromoQueryKeyShowTitle[]  = "show_title";
const char kSyncPromoQueryKeyNextPage[]  = "next_page";

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

  if (google_util::IsOrganic(brand))
    return true;

  if (StartsWithASCII(brand, "CH", true))
    return true;

  // Default to disallow for all other brand codes.
  return false;
}

// The Web UI data source for the sync promo page.
class SyncPromoUIHTMLSource : public ChromeWebUIDataSource {
 public:
  SyncPromoUIHTMLSource();

 private:
  ~SyncPromoUIHTMLSource() {}
  DISALLOW_COPY_AND_ASSIGN(SyncPromoUIHTMLSource);
};

SyncPromoUIHTMLSource::SyncPromoUIHTMLSource()
    : ChromeWebUIDataSource(chrome::kChromeUISyncPromoHost) {
  DictionaryValue localized_strings;
  CoreOptionsHandler::GetStaticLocalizedValues(&localized_strings);
  SyncSetupHandler::GetStaticLocalizedValues(&localized_strings);
  AddLocalizedStrings(localized_strings);
}

// Looks for |search_key| in the query portion of |url|. Returns true if the
// key is found and sets |out_value| to the value for the key. Returns false if
// the key is not found.
bool GetValueForKeyInQuery(const GURL& url, const std::string& search_key,
                           std::string* out_value) {
  url_parse::Component query = url.parsed_for_possibly_invalid_spec().query;
  url_parse::Component key, value;
  while (url_parse::ExtractQueryKeyValue(
      url.spec().c_str(), &query, &key, &value)) {
    if (key.is_nonempty()) {
      std::string key_string = url.spec().substr(key.begin, key.len);
      if (key_string == search_key) {
        if (value.is_nonempty())
          *out_value = url.spec().substr(value.begin, value.len);
        else
          *out_value = "";
        return true;
      }
    }
  }
  return false;
}

}  // namespace

SyncPromoUI::SyncPromoUI(TabContents* contents) : ChromeWebUI(contents) {
  should_hide_url_ = true;

  SyncPromoHandler* handler = new SyncPromoHandler(
      g_browser_process->profile_manager());
  AddMessageHandler(handler);
  handler->Attach(this);

  // Set up the chrome://theme/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  ThemeSource* theme = new ThemeSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(theme);

  // Set up the sync promo source.
  SyncPromoUIHTMLSource* html_source = new SyncPromoUIHTMLSource();
  html_source->set_json_path(kStringsJsFile);
  html_source->add_resource_path(kSyncPromoJsFile, IDR_SYNC_PROMO_JS);
  html_source->set_default_resource(IDR_SYNC_PROMO_HTML);
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
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
      profile->GetOriginalProfile()->GetProfileSyncService();
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

bool SyncPromoUI::ShouldShowSyncPromoAtStartup(Profile* profile,
                                               bool is_new_profile) {
  if (!ShouldShowSyncPromo(profile))
    return false;

  const CommandLine& command_line  = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kNoFirstRun))
    is_new_profile = false;

  PrefService *prefs = profile->GetPrefs();
  if (!is_new_profile) {
    if (!prefs->HasPrefPath(prefs::kSyncPromoStartupCount))
      return false;
  }

  if (HasUserSkippedSyncPromo(profile))
    return false;

  // For Chinese users skip the sync promo.
  if (g_browser_process->GetApplicationLocale() == "zh-CN")
    return false;

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

  // Default to show the promo.
  return true;
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
GURL SyncPromoUI::GetSyncPromoURL(const GURL& next_page, bool show_title) {
  std::stringstream stream;
  stream << chrome::kChromeUISyncPromoURL << "?" << kSyncPromoQueryKeyShowTitle
         << "=" << (show_title ? "true" : "false");

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
bool SyncPromoUI::GetShowTitleForSyncPromoURL(const GURL& url) {
  std::string value;
  if (GetValueForKeyInQuery(url, kSyncPromoQueryKeyShowTitle, &value))
    return value == "true";
  return false;
}

// static
GURL SyncPromoUI::GetNextPageURLForSyncPromoURL(const GURL& url) {
  std::string value;
  if (GetValueForKeyInQuery(url, kSyncPromoQueryKeyNextPage, &value)) {
    url_canon::RawCanonOutputT<char16> output;
    url_util::DecodeURLEscapeSequences(value.c_str(), value.length(), &output);
    std::string url;
    UTF16ToUTF8(output.data(), output.length(), &url);
    return GURL(url);
  }
  return GURL();
}

// static
bool SyncPromoUI::UserHasSeenSyncPromoAtStartup(Profile* profile) {
  return profile->GetPrefs()->GetInteger(prefs::kSyncPromoStartupCount) > 0;
}
