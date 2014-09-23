// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/google/google_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/omnibox_field_trial.h"
#include "components/search/search.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

#if defined(ENABLE_RLZ)
#include "chrome/browser/rlz/rlz.h"
#endif

using content::BrowserThread;

// static
std::string* UIThreadSearchTermsData::google_base_url_ = NULL;

UIThreadSearchTermsData::UIThreadSearchTermsData(Profile* profile)
    : profile_(profile) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
}

std::string UIThreadSearchTermsData::GoogleBaseURLValue() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (google_base_url_)
    return *google_base_url_;
  GURL base_url(google_util::CommandLineGoogleBaseURL());
  if (base_url.is_valid())
    return base_url.spec();
  return profile_ ?
      google_profile_helper::GetGoogleHomePageURL(profile_).spec() :
      SearchTermsData::GoogleBaseURLValue();
}

std::string UIThreadSearchTermsData::GetApplicationLocale() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_browser_process->GetApplicationLocale();
}

// Android implementations are in ui_thread_search_terms_data_android.cc.
#if !defined(OS_ANDROID)
base::string16 UIThreadSearchTermsData::GetRlzParameterValue(
    bool from_app_list) const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::string16 rlz_string;
#if defined(ENABLE_RLZ)
  // For organic brandcodes do not use rlz at all. Empty brandcode usually
  // means a chromium install. This is ok.
  std::string brand;
  if (google_brand::GetBrand(&brand) && !brand.empty() &&
      !google_brand::IsOrganic(brand)) {
    // This call will return false the first time(s) it is called until the
    // value has been cached. This normally would mean that at most one omnibox
    // search might not send the RLZ data but this is not really a problem.
    rlz_lib::AccessPoint access_point = RLZTracker::ChromeOmnibox();
#if !defined(OS_IOS)
    if (from_app_list)
      access_point = RLZTracker::ChromeAppList();
#endif
    RLZTracker::GetAccessPointRlz(access_point, &rlz_string);
  }
#endif
  return rlz_string;
}

// We can enable this on non-Android if other platforms ever want a non-empty
// search client string.  There is already a unit test in place for Android
// called TemplateURLTest::SearchClient.
std::string UIThreadSearchTermsData::GetSearchClient() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  return std::string();
}
#endif

std::string UIThreadSearchTermsData::GetSuggestClient() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_ANDROID)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE ?
      "chrome" : "chrome-omni";
#else
  return chrome::IsInstantExtendedAPIEnabled() ? "chrome-omni" : "chrome";
#endif
}

std::string UIThreadSearchTermsData::GetSuggestRequestIdentifier() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
    return OmniboxFieldTrial::EnableAnswersInSuggest() ?
        "chrome-mobile-ext-ansg" : "chrome-mobile-ext";
  }
  return OmniboxFieldTrial::EnableAnswersInSuggest() ?
      "chrome-ext-ansg" : "chrome-ext";
#elif defined(OS_IOS)
  return OmniboxFieldTrial::EnableAnswersInSuggest() ?
      "chrome-ext-ansg" : "chrome-ext";
#else
  return "chrome-ext";
#endif
}

bool UIThreadSearchTermsData::EnableAnswersInSuggest() const {
  return OmniboxFieldTrial::EnableAnswersInSuggest();
}

bool UIThreadSearchTermsData::IsShowingSearchTermsOnSearchResultsPages() const {
  return chrome::IsInstantExtendedAPIEnabled() &&
      chrome::IsQueryExtractionEnabled();
}

std::string UIThreadSearchTermsData::InstantExtendedEnabledParam(
    bool for_search) const {
  return chrome::InstantExtendedEnabledParam(for_search);
}

std::string UIThreadSearchTermsData::ForceInstantResultsParam(
    bool for_prerender) const {
  return chrome::ForceInstantResultsParam(for_prerender);
}

int UIThreadSearchTermsData::OmniboxStartMargin() const {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile_);
  // Android and iOS have no InstantService.
  return instant_service ?
      instant_service->omnibox_start_margin() : chrome::kDisableStartMargin;
}

std::string UIThreadSearchTermsData::NTPIsThemedParam() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(ENABLE_THEMES)
  if (!chrome::IsInstantExtendedAPIEnabled())
    return std::string();

  // TODO(dhollowa): Determine fraction of custom themes that don't affect the
  // NTP background and/or color.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  // NTP is considered themed if the theme is not default and not native (GTK+).
  if (theme_service && !theme_service->UsingDefaultTheme() &&
      !theme_service->UsingSystemTheme())
    return "es_th=1&";
#endif  // defined(ENABLE_THEMES)

  return std::string();
}

// It's acutally OK to call this method on any thread, but it's currently placed
// in UIThreadSearchTermsData since SearchTermsData cannot depend on
// VersionInfo.
std::string UIThreadSearchTermsData::GoogleImageSearchSource() const {
  chrome::VersionInfo version_info;
  if (version_info.is_valid()) {
    std::string version(version_info.Name() + " " + version_info.Version());
    if (version_info.IsOfficialBuild())
      version += " (Official)";
    version += " " + version_info.OSType();
    std::string modifier(version_info.GetVersionStringModifier());
    if (!modifier.empty())
      version += " " + modifier;
    return version;
  }
  return "unknown";
}

// static
void UIThreadSearchTermsData::SetGoogleBaseURL(const std::string& base_url) {
  delete google_base_url_;
  google_base_url_ = base_url.empty() ? NULL : new std::string(base_url);
}
