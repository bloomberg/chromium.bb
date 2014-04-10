// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_terms_data.h"

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "sync/protocol/sync.pb.h"
#include "url/gurl.h"

#if defined(ENABLE_RLZ)
#include "chrome/browser/rlz/rlz.h"
#endif

using content::BrowserThread;

SearchTermsData::SearchTermsData() {
}

SearchTermsData::~SearchTermsData() {
}

std::string SearchTermsData::GoogleBaseURLValue() const {
  return GoogleURLTracker::kDefaultGoogleHomepage;
}

std::string SearchTermsData::GoogleBaseSuggestURLValue() const {
  // Start with the Google base URL.
  const GURL base_url(GoogleBaseURLValue());
  DCHECK(base_url.is_valid());

  GURL::Replacements repl;

  // Replace any existing path with "/complete/".
  // SetPathStr() requires its argument to stay in scope as long as |repl| is,
  // so "/complete/" can't be passed to SetPathStr() directly, it needs to be in
  // a variable.
  const std::string suggest_path("/complete/");
  repl.SetPathStr(suggest_path);

  // Clear the query and ref.
  repl.ClearQuery();
  repl.ClearRef();
  return base_url.ReplaceComponents(repl).spec();
}

std::string SearchTermsData::GetApplicationLocale() const {
  return "en";
}

base::string16 SearchTermsData::GetRlzParameterValue(bool from_app_list) const {
  return base::string16();
}

std::string SearchTermsData::GetSearchClient() const {
  return std::string();
}

std::string SearchTermsData::GetSuggestClient() const {
  return std::string();
}

std::string SearchTermsData::GetSuggestRequestIdentifier() const {
  return std::string();
}

std::string SearchTermsData::NTPIsThemedParam() const {
  return std::string();
}

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
  return profile_ ? GoogleURLTracker::GoogleURL(profile_).spec() :
      SearchTermsData::GoogleBaseURLValue();
}

std::string UIThreadSearchTermsData::GetApplicationLocale() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_browser_process->GetApplicationLocale();
}

// Android implementations are located in search_terms_data_android.cc.
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
  if (google_util::GetBrand(&brand) && !brand.empty() &&
      !google_util::IsOrganic(brand)) {
    // This call will return false the first time(s) it is called until the
    // value has been cached. This normally would mean that at most one omnibox
    // search might not send the RLZ data but this is not really a problem.
    rlz_lib::AccessPoint access_point = RLZTracker::CHROME_OMNIBOX;
#if !defined(OS_IOS)
    if (from_app_list)
      access_point = RLZTracker::CHROME_APP_LIST;
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
  sync_pb::SyncEnums::DeviceType device_type =
      browser_sync::DeviceInfo::GetLocalDeviceType();
  return device_type == sync_pb::SyncEnums_DeviceType_TYPE_PHONE ?
    "chrome" : "chrome-omni";
#else
  return chrome::IsInstantExtendedAPIEnabled() ? "chrome-omni" : "chrome";
#endif
}

std::string UIThreadSearchTermsData::GetSuggestRequestIdentifier() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
#if defined(OS_ANDROID)
  sync_pb::SyncEnums::DeviceType device_type =
      browser_sync::DeviceInfo::GetLocalDeviceType();
  return device_type == sync_pb::SyncEnums_DeviceType_TYPE_PHONE ?
    "chrome-mobile-ext" : "chrome-ext";
#else
  return "chrome-ext";
#endif
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
      !theme_service->UsingNativeTheme())
    return "es_th=1&";
#endif  // defined(ENABLE_THEMES)

  return std::string();
}

// static
void UIThreadSearchTermsData::SetGoogleBaseURL(const std::string& base_url) {
  delete google_base_url_;
  google_base_url_ = base_url.empty() ? NULL : new std::string(base_url);
}
