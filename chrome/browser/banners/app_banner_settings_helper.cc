// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_settings_helper.h"

#include <algorithm>
#include <string>

#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace {

// Max number of apps (including ServiceWorker based web apps) that a particular
// site may show a banner for.
const size_t kMaxAppsPerSite = 3;

// Oldest could show banner event we care about, in days.
const unsigned int kOldestCouldShowBannerEventInDays = 14;

// Dictionary key to use for the 'could show banner' events.
const char kCouldShowBannerEventsKey[] = "couldShowBannerEvents";

// Dictionary key to use whether the banner has been blocked.
const char kHasBlockedKey[] = "hasBlocked";

base::Time DateFromTime(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  return base::Time::FromLocalExploded(exploded);
}

scoped_ptr<base::DictionaryValue> GetOriginDict(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  if (!settings)
    return scoped_ptr<base::DictionaryValue>();

  scoped_ptr<base::Value> value = settings->GetWebsiteSetting(
      origin_url, origin_url, CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(),
      NULL);
  if (!value.get())
    return make_scoped_ptr(new base::DictionaryValue());

  if (!value->IsType(base::Value::TYPE_DICTIONARY))
    return make_scoped_ptr(new base::DictionaryValue());

  return make_scoped_ptr(static_cast<base::DictionaryValue*>(value.release()));
}

base::DictionaryValue* GetAppDict(base::DictionaryValue* origin_dict,
                                  const std::string& key_name) {
  base::DictionaryValue* app_dict = nullptr;
  if (!origin_dict->GetDictionaryWithoutPathExpansion(key_name, &app_dict)) {
    // Don't allow more than kMaxAppsPerSite dictionaries.
    if (origin_dict->size() < kMaxAppsPerSite) {
      app_dict = new base::DictionaryValue();
      origin_dict->SetWithoutPathExpansion(key_name, make_scoped_ptr(app_dict));
    }
  }

  return app_dict;
}

}  // namespace

void AppBannerSettingsHelper::RecordCouldShowBannerEvent(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url,
    base::Time time) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord() || web_contents->GetURL() != origin_url ||
      package_name_or_start_url.empty()) {
    return;
  }

  ContentSettingsPattern pattern(ContentSettingsPattern::FromURL(origin_url));
  if (!pattern.IsValid())
    return;

  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();
  scoped_ptr<base::DictionaryValue> origin_dict =
      GetOriginDict(settings, origin_url);
  if (!origin_dict)
    return;

  base::DictionaryValue* app_dict =
      GetAppDict(origin_dict.get(), package_name_or_start_url);
  if (!app_dict)
    return;

  base::ListValue* could_show_list = nullptr;
  if (!app_dict->GetList(kCouldShowBannerEventsKey, &could_show_list)) {
    could_show_list = new base::ListValue();
    app_dict->Set(kCouldShowBannerEventsKey, make_scoped_ptr(could_show_list));
  }

  // Trim any items that are older than we should care about. For comparisons
  // the times are converted to local dates.
  base::Time date = DateFromTime(time);
  base::ValueVector::iterator it = could_show_list->begin();
  while (it != could_show_list->end()) {
    if ((*it)->IsType(base::Value::TYPE_DOUBLE)) {
      double internal_date;
      (*it)->GetAsDouble(&internal_date);
      base::Time other_date =
          DateFromTime(base::Time::FromInternalValue(internal_date));
      // This date has already been added. Don't add the date again, and don't
      // bother trimming values as it will have been done the first time the
      // date was added (unless the local date has changed, which we can live
      // with).
      if (other_date == date)
        return;

      base::TimeDelta delta = date - other_date;
      if (delta <
          base::TimeDelta::FromDays(kOldestCouldShowBannerEventInDays)) {
        ++it;
        continue;
      }
    }

    // Either this date is older than we care about, or it isn't a date, so
    // remove it;
    it = could_show_list->Erase(it, nullptr);
  }

  // Dates are stored in their raw form (i.e. not local dates) to be resilient
  // to time zone changes.
  could_show_list->AppendDouble(time.ToInternalValue());
  settings->SetWebsiteSetting(pattern, ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(),
                              origin_dict.release());
}

std::vector<base::Time> AppBannerSettingsHelper::GetCouldShowBannerEvents(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url) {
  std::vector<base::Time> result;
  if (package_name_or_start_url.empty())
    return result;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();
  scoped_ptr<base::DictionaryValue> origin_dict =
      GetOriginDict(settings, origin_url);

  if (!origin_dict)
    return result;

  base::DictionaryValue* app_dict =
      GetAppDict(origin_dict.get(), package_name_or_start_url);
  if (!app_dict)
    return result;

  base::ListValue* could_show_list = nullptr;
  if (!app_dict->GetList(kCouldShowBannerEventsKey, &could_show_list))
    return result;

  for (auto value : *could_show_list) {
    if (value->IsType(base::Value::TYPE_DOUBLE)) {
      double internal_date;
      value->GetAsDouble(&internal_date);
      base::Time date = base::Time::FromInternalValue(internal_date);
      result.push_back(date);
    }
  }

  return result;
}

bool AppBannerSettingsHelper::IsAllowed(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord() || web_contents->GetURL() != origin_url ||
      package_name_or_start_url.empty()) {
    return false;
  }

  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();
  scoped_ptr<base::DictionaryValue> origin_dict =
      GetOriginDict(settings, origin_url);

  if (!origin_dict)
    return true;

  base::DictionaryValue* app_dict =
      GetAppDict(origin_dict.get(), package_name_or_start_url);
  if (!app_dict)
    return true;

  bool has_blocked;
  if (!app_dict->GetBoolean(kHasBlockedKey, &has_blocked))
    return true;

  return !has_blocked;
}

void AppBannerSettingsHelper::Block(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& package_name_or_start_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord() || web_contents->GetURL() != origin_url ||
      package_name_or_start_url.empty()) {
    return;
  }

  ContentSettingsPattern pattern(ContentSettingsPattern::FromURL(origin_url));
  if (!pattern.IsValid())
    return;

  HostContentSettingsMap* settings = profile->GetHostContentSettingsMap();
  scoped_ptr<base::DictionaryValue> origin_dict =
      GetOriginDict(settings, origin_url);

  if (!origin_dict)
    return;

  base::DictionaryValue* app_dict =
      GetAppDict(origin_dict.get(), package_name_or_start_url);
  if (!app_dict)
    return;

  // Update the setting and save it back.
  app_dict->SetBoolean(kHasBlockedKey, true);
  settings->SetWebsiteSetting(pattern, ContentSettingsPattern::Wildcard(),
                              CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(),
                              origin_dict.release());
}
