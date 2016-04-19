// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_settings_helper.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/banners/app_banner_data_fetcher.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/rappor/rappor_utils.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace {

// Max number of apps (including ServiceWorker based web apps) that a particular
// site may show a banner for.
const size_t kMaxAppsPerSite = 3;

// Oldest could show banner event we care about, in days.
const unsigned int kOldestCouldShowBannerEventInDays = 14;

// Number of days that showing the banner will prevent it being seen again for.
const unsigned int kMinimumDaysBetweenBannerShows = 60;

const unsigned int kNumberOfMinutesInADay = 1440;

// Number of days that the banner being blocked will prevent it being seen again
// for.
const unsigned int kMinimumBannerBlockedToBannerShown = 90;

// Default scores assigned to direct and indirect navigations respectively.
const unsigned int kDefaultDirectNavigationEngagement = 1;
const unsigned int kDefaultIndirectNavigationEngagement = 1;

// Default number of navigations required to trigger the banner.
const unsigned int kDefaultTotalEngagementToTrigger = 2;

// Dictionary keys to use for the events.
const char* kBannerEventKeys[] = {
    "couldShowBannerEvents",
    "didShowBannerEvent",
    "didBlockBannerEvent",
    "didAddToHomescreenEvent",
};

// Keys to use when storing BannerEvent structs.
const char kBannerTimeKey[] = "time";
const char kBannerEngagementKey[] = "engagement";

// Keys to use when querying the variations params.
const char kBannerParamsKey[] = "AppBannerTriggering";
const char kBannerParamsDirectKey[] = "direct";
const char kBannerParamsIndirectKey[] = "indirect";
const char kBannerParamsTotalKey[] = "total";
const char kBannerParamsMinutesKey[] = "minutes";
const char kBannerSiteEngagementParamsKey[] = "app_banner_triggering";
const char kBannerSiteEngagementParamsTotalKey[] =
    "app_banner_triggering_total";

// Engagement weight assigned to direct and indirect navigations.
// By default, a direct navigation is a page visit via ui::PAGE_TRANSITION_TYPED
// or ui::PAGE_TRANSITION_GENERATED.
double gDirectNavigationEngagement = kDefaultDirectNavigationEngagement;
double gIndirectNavigationEnagagement = kDefaultIndirectNavigationEngagement;

// Number of minutes between visits that will trigger a could show banner event.
// Defaults to the number of minutes in a day.
unsigned int gMinimumMinutesBetweenVisits = kNumberOfMinutesInADay;

// Total engagement score required before a banner will actually be triggered.
double gTotalEngagementToTrigger = kDefaultTotalEngagementToTrigger;

std::unique_ptr<base::DictionaryValue> GetOriginDict(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  if (!settings)
    return base::WrapUnique(new base::DictionaryValue());

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          origin_url, origin_url, CONTENT_SETTINGS_TYPE_APP_BANNER,
          std::string(), NULL));
  if (!dict)
    return base::WrapUnique(new base::DictionaryValue());

  return dict;
}

base::DictionaryValue* GetAppDict(base::DictionaryValue* origin_dict,
                                  const std::string& key_name) {
  base::DictionaryValue* app_dict = nullptr;
  if (!origin_dict->GetDictionaryWithoutPathExpansion(key_name, &app_dict)) {
    // Don't allow more than kMaxAppsPerSite dictionaries.
    if (origin_dict->size() < kMaxAppsPerSite) {
      app_dict = new base::DictionaryValue();
      origin_dict->SetWithoutPathExpansion(key_name,
                                           base::WrapUnique(app_dict));
    }
  }

  return app_dict;
}

double GetEventEngagement(ui::PageTransition transition_type) {
  if (ui::PageTransitionCoreTypeIs(transition_type,
                                   ui::PAGE_TRANSITION_TYPED) ||
      ui::PageTransitionCoreTypeIs(transition_type,
                                   ui::PAGE_TRANSITION_GENERATED)) {
    return gDirectNavigationEngagement;
  } else {
    return gIndirectNavigationEnagagement;
  }
}

// Queries variations for the maximum site engagement score required to trigger
// the banner showing.
void UpdateSiteEngagementToTrigger() {
  std::string total_param = variations::GetVariationParamValue(
      SiteEngagementService::kEngagementParams,
      kBannerSiteEngagementParamsTotalKey);

  if (!total_param.empty()) {
    double total_engagement = -1;

    if (base::StringToDouble(total_param, &total_engagement) &&
        total_engagement > 0) {
      AppBannerSettingsHelper::SetTotalEngagementToTrigger(total_engagement);
    }
  }
}

// Queries variations for updates to the default engagement values assigned
// to direct and indirect navigations.
void UpdateEngagementWeights() {
  std::string direct_param = variations::GetVariationParamValue(
      kBannerParamsKey, kBannerParamsDirectKey);
  std::string indirect_param = variations::GetVariationParamValue(
      kBannerParamsKey, kBannerParamsIndirectKey);
  std::string total_param = variations::GetVariationParamValue(
      kBannerParamsKey, kBannerParamsTotalKey);

  if (!direct_param.empty() && !indirect_param.empty() &&
      !total_param.empty()) {
    double direct_engagement = -1;
    double indirect_engagement = -1;
    double total_engagement = -1;

    // Ensure that we get valid doubles from the field trial, and that both
    // values are greater than or equal to zero and less than or equal to the
    // total engagement required to trigger the banner.
    if (base::StringToDouble(direct_param, &direct_engagement) &&
        base::StringToDouble(indirect_param, &indirect_engagement) &&
        base::StringToDouble(total_param, &total_engagement) &&
        direct_engagement >= 0 && indirect_engagement >= 0 &&
        total_engagement > 0 && direct_engagement <= total_engagement &&
        indirect_engagement <= total_engagement) {
      AppBannerSettingsHelper::SetEngagementWeights(direct_engagement,
                                                    indirect_engagement);
      AppBannerSettingsHelper::SetTotalEngagementToTrigger(total_engagement);
    }
  }
}

// Queries variation for updates to the default number of minutes between
// site visits counted for the purposes of displaying a banner.
void UpdateMinutesBetweenVisits() {
  std::string param = variations::GetVariationParamValue(
      kBannerParamsKey, kBannerParamsMinutesKey);
  if (!param.empty()) {
    int minimum_minutes = 0;
    if (base::StringToInt(param, &minimum_minutes))
      AppBannerSettingsHelper::SetMinimumMinutesBetweenVisits(minimum_minutes);
  }
}

// Returns the site engagement karma score for the given origin URL under the
// current profile.
double GetSiteEngagementScoreForOrigin(
    content::WebContents* web_contents,
    const GURL& origin_url) {
  SiteEngagementService* service = SiteEngagementService::Get(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  return service ? service->GetScore(origin_url) : 0;
}

}  // namespace

void AppBannerSettingsHelper::ClearHistoryForURLs(
    Profile* profile,
    const std::set<GURL>& origin_urls) {
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  for (const GURL& origin_url : origin_urls) {
    settings->SetWebsiteSettingDefaultScope(origin_url, GURL(),
                                            CONTENT_SETTINGS_TYPE_APP_BANNER,
                                            std::string(), nullptr);
    settings->FlushLossyWebsiteSettings();
  }
}

void AppBannerSettingsHelper::RecordBannerInstallEvent(
    content::WebContents* web_contents,
    const std::string& package_name_or_start_url,
    AppBannerRapporMetric rappor_metric) {
  banners::TrackInstallEvent(banners::INSTALL_EVENT_WEB_APP_INSTALLED);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, web_contents->GetURL(),
      package_name_or_start_url,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      banners::AppBannerDataFetcher::GetCurrentTime());

  rappor::SampleDomainAndRegistryFromGURL(
      g_browser_process->rappor_service(),
      (rappor_metric == WEB ? "AppBanner.WebApp.Installed"
                            : "AppBanner.NativeApp.Installed"),
      web_contents->GetURL());
}

void AppBannerSettingsHelper::RecordBannerDismissEvent(
    content::WebContents* web_contents,
    const std::string& package_name_or_start_url,
    AppBannerRapporMetric rappor_metric) {
  banners::TrackDismissEvent(banners::DISMISS_EVENT_CLOSE_BUTTON);

  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents, web_contents->GetURL(),
      package_name_or_start_url,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK,
      banners::AppBannerDataFetcher::GetCurrentTime());

  rappor::SampleDomainAndRegistryFromGURL(
      g_browser_process->rappor_service(),
      (rappor_metric == WEB ? "AppBanner.WebApp.Dismissed"
                            : "AppBanner.NativeApp.Dismissed"),
      web_contents->GetURL());
}

void AppBannerSettingsHelper::RecordBannerEvent(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    AppBannerEvent event,
    base::Time time) {
  DCHECK(event != APP_BANNER_EVENT_COULD_SHOW);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord() || package_name_or_start_url.empty())
    return;

  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> origin_dict =
      GetOriginDict(settings, origin_url);
  if (!origin_dict)
    return;

  base::DictionaryValue* app_dict =
      GetAppDict(origin_dict.get(), package_name_or_start_url);
  if (!app_dict)
    return;

  // Dates are stored in their raw form (i.e. not local dates) to be resilient
  // to time zone changes.
  std::string event_key(kBannerEventKeys[event]);
  app_dict->SetDouble(event_key, time.ToInternalValue());

  settings->SetWebsiteSettingDefaultScope(origin_url, GURL(),
                                          CONTENT_SETTINGS_TYPE_APP_BANNER,
                                          std::string(), origin_dict.release());

  // App banner content settings are lossy, meaning they will not cause the
  // prefs to become dirty. This is fine for most events, as if they are lost it
  // just means the user will have to engage a little bit more. However the
  // DID_ADD_TO_HOMESCREEN event should always be recorded to prevent
  // spamminess.
  if (event == APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN)
    settings->FlushLossyWebsiteSettings();
}

void AppBannerSettingsHelper::RecordBannerCouldShowEvent(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    base::Time time,
    ui::PageTransition transition_type) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord() || package_name_or_start_url.empty())
    return;

  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> origin_dict =
      GetOriginDict(settings, origin_url);
  if (!origin_dict)
    return;

  base::DictionaryValue* app_dict =
      GetAppDict(origin_dict.get(), package_name_or_start_url);
  if (!app_dict)
    return;

  std::string event_key(kBannerEventKeys[APP_BANNER_EVENT_COULD_SHOW]);
  double engagement = GetEventEngagement(transition_type);

  base::ListValue* could_show_list = nullptr;
  if (!app_dict->GetList(event_key, &could_show_list)) {
    could_show_list = new base::ListValue();
    app_dict->Set(event_key, base::WrapUnique(could_show_list));
  }

  // Trim any items that are older than we should care about. For comparisons
  // the times are converted to local dates.
  base::Time date = BucketTimeToResolution(time, gMinimumMinutesBetweenVisits);
  base::ValueVector::iterator it = could_show_list->begin();
  while (it != could_show_list->end()) {
    if ((*it)->IsType(base::Value::TYPE_DICTIONARY)) {
      base::DictionaryValue* internal_value;
      double internal_date;
      (*it)->GetAsDictionary(&internal_value);

      if (internal_value->GetDouble(kBannerTimeKey, &internal_date)) {
        base::Time other_date =
            BucketTimeToResolution(base::Time::FromInternalValue(internal_date),
                                   gMinimumMinutesBetweenVisits);
        if (other_date == date) {
          double other_engagement = 0;
          if (internal_value->GetDouble(kBannerEngagementKey,
                                        &other_engagement) &&
              other_engagement >= engagement) {
            // This date has already been added, but with an equal or higher
            // engagement. Don't add the date again. If the conditional fails,
            // fall to the end of the loop where the existing entry is deleted.
            return;
          }
        } else {
          base::TimeDelta delta = date - other_date;
          if (delta <
              base::TimeDelta::FromDays(kOldestCouldShowBannerEventInDays)) {
            ++it;
            continue;
          }
        }
      }
    }

    // Either this date is older than we care about, or it isn't in the correct
    // format, or it is the same as the current date but with a lower
    // engagement, so remove it.
    it = could_show_list->Erase(it, nullptr);
  }

  // Dates are stored in their raw form (i.e. not local dates) to be resilient
  // to time zone changes.
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetDouble(kBannerTimeKey, time.ToInternalValue());
  value->SetDouble(kBannerEngagementKey, engagement);
  could_show_list->Append(std::move(value));

  settings->SetWebsiteSettingDefaultScope(origin_url, GURL(),
                                          CONTENT_SETTINGS_TYPE_APP_BANNER,
                                          std::string(), origin_dict.release());
}

bool AppBannerSettingsHelper::ShouldShowBanner(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    base::Time time) {
  // Ignore all checks if the flag to do so is set.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBypassAppBannerEngagementChecks)) {
    return true;
  }

  // Never show a banner when the package name or URL is empty.
  if (package_name_or_start_url.empty())
    return false;

  // Don't show if it has been added to the homescreen.
  base::Time added_time =
      GetSingleBannerEvent(web_contents, origin_url, package_name_or_start_url,
                           APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN);
  if (!added_time.is_null()) {
    banners::TrackDisplayEvent(banners::DISPLAY_EVENT_INSTALLED_PREVIOUSLY);
    return false;
  }

  base::Time blocked_time =
      GetSingleBannerEvent(web_contents, origin_url, package_name_or_start_url,
                           APP_BANNER_EVENT_DID_BLOCK);

  // Null times are in the distant past, so the delta between real times and
  // null events will always be greater than the limits.
  if (time - blocked_time <
      base::TimeDelta::FromDays(kMinimumBannerBlockedToBannerShown)) {
    banners::TrackDisplayEvent(banners::DISPLAY_EVENT_BLOCKED_PREVIOUSLY);
    return false;
  }

  base::Time shown_time =
      GetSingleBannerEvent(web_contents, origin_url, package_name_or_start_url,
                           APP_BANNER_EVENT_DID_SHOW);
  if (time - shown_time <
      base::TimeDelta::FromDays(kMinimumDaysBetweenBannerShows)) {
    banners::TrackDisplayEvent(banners::DISPLAY_EVENT_IGNORED_PREVIOUSLY);
    return false;
  }

  double total_engagement = 0;
  if (ShouldUseSiteEngagementScore()) {
    total_engagement =
        GetSiteEngagementScoreForOrigin(web_contents, origin_url);
  } else {
    std::vector<BannerEvent> could_show_events = GetCouldShowBannerEvents(
        web_contents, origin_url, package_name_or_start_url);

    for (const auto& event : could_show_events)
      total_engagement += event.engagement;
  }

  if (total_engagement < gTotalEngagementToTrigger) {
    banners::TrackDisplayEvent(banners::DISPLAY_EVENT_NOT_VISITED_ENOUGH);
    return false;
  }

  return true;
}

std::vector<AppBannerSettingsHelper::BannerEvent>
AppBannerSettingsHelper::GetCouldShowBannerEvents(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url) {
  std::vector<BannerEvent> result;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> origin_dict =
      GetOriginDict(settings, origin_url);

  if (!origin_dict)
    return result;

  base::DictionaryValue* app_dict =
      GetAppDict(origin_dict.get(), package_name_or_start_url);
  if (!app_dict)
    return result;

  std::string event_key(kBannerEventKeys[APP_BANNER_EVENT_COULD_SHOW]);
  base::ListValue* could_show_list = nullptr;
  if (!app_dict->GetList(event_key, &could_show_list))
    return result;

  for (auto value : *could_show_list) {
    if (value->IsType(base::Value::TYPE_DICTIONARY)) {
      base::DictionaryValue* internal_value;
      double internal_date = 0;
      value->GetAsDictionary(&internal_value);
      double engagement = 0;

      if (internal_value->GetDouble(kBannerTimeKey, &internal_date) &&
          internal_value->GetDouble(kBannerEngagementKey, &engagement)) {
        base::Time date = base::Time::FromInternalValue(internal_date);
        result.push_back({date, engagement});
      }
    }
  }

  return result;
}

base::Time AppBannerSettingsHelper::GetSingleBannerEvent(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    AppBannerEvent event) {
  DCHECK(event != APP_BANNER_EVENT_COULD_SHOW);
  DCHECK(event < APP_BANNER_EVENT_NUM_EVENTS);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> origin_dict =
      GetOriginDict(settings, origin_url);

  if (!origin_dict)
    return base::Time();

  base::DictionaryValue* app_dict =
      GetAppDict(origin_dict.get(), package_name_or_start_url);
  if (!app_dict)
    return base::Time();

  std::string event_key(kBannerEventKeys[event]);
  double internal_time;
  if (!app_dict->GetDouble(event_key, &internal_time))
    return base::Time();

  return base::Time::FromInternalValue(internal_time);
}

void AppBannerSettingsHelper::RecordMinutesFromFirstVisitToShow(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    base::Time time) {
  std::vector<BannerEvent> could_show_events = GetCouldShowBannerEvents(
      web_contents, origin_url, package_name_or_start_url);

  int minutes = 0;
  if (could_show_events.size())
    minutes = (time - could_show_events[0].time).InMinutes();

  banners::TrackMinutesFromFirstVisitToBannerShown(minutes);
}

void AppBannerSettingsHelper::SetEngagementWeights(double direct_engagement,
                                                   double indirect_engagement) {
  gDirectNavigationEngagement = direct_engagement;
  gIndirectNavigationEnagagement = indirect_engagement;
}

void AppBannerSettingsHelper::SetMinimumMinutesBetweenVisits(
    unsigned int minutes) {
  gMinimumMinutesBetweenVisits = minutes;
}

void AppBannerSettingsHelper::SetTotalEngagementToTrigger(
    double total_engagement) {
  gTotalEngagementToTrigger = total_engagement;
}

void AppBannerSettingsHelper::SetDefaultParameters() {
  SetEngagementWeights(kDefaultDirectNavigationEngagement,
                       kDefaultIndirectNavigationEngagement);
  SetMinimumMinutesBetweenVisits(kNumberOfMinutesInADay);
  SetTotalEngagementToTrigger(kDefaultTotalEngagementToTrigger);
}

// Given a time, returns that time scoped to the nearest minute resolution
// locally. For example, if the resolution is one hour, this function will
// return the time to the closest (previous) hour in the local time zone.
base::Time AppBannerSettingsHelper::BucketTimeToResolution(
    base::Time time,
    unsigned int minutes) {
  // Only support resolutions smaller than or equal to one day. Enforce
  // that resolutions divide evenly into one day. Otherwise, default to a
  // day resolution (each time converted to midnight local time).
  if (minutes == 0 || minutes >= kNumberOfMinutesInADay ||
      kNumberOfMinutesInADay % minutes != 0) {
    return time.LocalMidnight();
  }

  // Extract the number of minutes past midnight in local time. Divide that
  // number by the resolution size, and return the time converted to local
  // midnight with the resulting truncated number added.
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  int total_minutes = exploded.hour * 60 + exploded.minute;

  // Use truncating integer division here.
  return time.LocalMidnight() +
         base::TimeDelta::FromMinutes((total_minutes / minutes) * minutes);
}

void AppBannerSettingsHelper::UpdateFromFieldTrial() {
  // If we are using the site engagement score, only extract the total
  // engagement to trigger from the params variations.
  if (ShouldUseSiteEngagementScore()) {
    UpdateSiteEngagementToTrigger();
  } else {
    UpdateEngagementWeights();
    UpdateMinutesBetweenVisits();
  }
}

bool AppBannerSettingsHelper::ShouldUseSiteEngagementScore() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSiteEngagementAppBanner)) {
    return true;
  }

  // This experiment is controlled under the same key as the broader site
  // engagement experiment rather than the banner experiment. This avoids cross
  // pollution with other site engagement experiments. However, this experiment
  // must only be active when there is one singular group under the banner
  // experiment, otherwise the banner and site engagement banner experiments
  // will conflict.
  //
  // Making the experiment active when a variations key is present allows us
  // to have experiments which enable multiple features under site engagement.
  std::string param = variations::GetVariationParamValue(
      SiteEngagementService::kEngagementParams, kBannerSiteEngagementParamsKey);
  return !param.empty();
}
