// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/ui_thread_search_terms_data.h"

#include "base/logging.h"
#include "base/strings/string16.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/search/search.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/google/google_brand.h"
#include "ios/chrome/browser/google/google_url_tracker_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_thread.h"
#include "net/base/escape.h"
#include "rlz/features/features.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

namespace ios {

UIThreadSearchTermsData::UIThreadSearchTermsData(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
}

UIThreadSearchTermsData::~UIThreadSearchTermsData() {}

std::string UIThreadSearchTermsData::GoogleBaseURLValue() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (google_base_url.is_valid())
    return google_base_url.spec();

  if (!browser_state_)
    return SearchTermsData::GoogleBaseURLValue();

  GoogleURLTracker* google_url_tracker =
      ios::GoogleURLTrackerFactory::GetForBrowserState(browser_state_);
  return google_url_tracker ? google_url_tracker->google_url().spec()
                            : GoogleURLTracker::kDefaultGoogleHomepage;
}

std::string UIThreadSearchTermsData::GetApplicationLocale() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetApplicationContext()->GetApplicationLocale();
}

base::string16 UIThreadSearchTermsData::GetRlzParameterValue(
    bool from_app_list) const {
  DCHECK(!from_app_list);
  DCHECK(thread_checker_.CalledOnValidThread());
  base::string16 rlz_string;
#if BUILDFLAG(ENABLE_RLZ)
  // For organic brandcode do not use rlz at all.
  std::string brand;
  if (ios::google_brand::GetBrand(&brand) &&
      !ios::google_brand::IsOrganic(brand)) {
    // This call will may return false until the value has been cached. This
    // normally would mean that a few omnibox searches might not send the RLZ
    // data but this is not really a problem (as the value will eventually be
    // set and cached).
    rlz::RLZTracker::GetAccessPointRlz(rlz::RLZTracker::ChromeOmnibox(),
                                       &rlz_string);
  }
#endif
  return rlz_string;
}

std::string UIThreadSearchTermsData::GetSearchClient() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::string();
}

std::string UIThreadSearchTermsData::GetSuggestClient() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return search::IsInstantExtendedAPIEnabled() ? "chrome-omni" : "chrome";
}

std::string UIThreadSearchTermsData::GetSuggestRequestIdentifier() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return "chrome-ext-ansg";
}

std::string UIThreadSearchTermsData::GoogleImageSearchSource() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string version(version_info::GetProductName() + " " +
                      version_info::GetVersionNumber());
  if (version_info::IsOfficialBuild())
    version += " (Official)";
  version += " " + version_info::GetOSType();
  std::string modifier(GetChannelString());
  if (!modifier.empty())
    version += " " + modifier;
  return version;
}

}  // namespace ios
