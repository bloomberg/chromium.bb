// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/notification_promo.h"

#include <cmath>
#include <vector>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/net/url_util.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "googleurl/src/gurl.h"

using content::UserMetricsAction;

namespace {

const int kDefaultGroupSize = 100;

const char promo_server_url[] = "https://clients3.google.com/crsignal/client";

double GetTimeFromPrefs(PrefService* prefs, const char* pref) {
  return prefs->HasPrefPath(pref) ? prefs->GetDouble(pref) : 0.0;
}

// Returns a string suitable for the Promo Server URL 'osname' value.
const char* PlatformString() {
#if defined(OS_WIN)
  return "win";
#elif defined(OS_MACOSX)
  return "mac";
#elif defined(OS_CHROMEOS)
  return "chromeos";
#elif defined(OS_LINUX)
  return "linux";
#else
  return "none";
#endif
}

// Returns a string suitable for the Promo Server URL 'dist' value.
const char* ChannelString() {
  // GetChannel hits the registry on Windows. See http://crbug.com/70898.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  const chrome::VersionInfo::Channel channel =
      chrome::VersionInfo::GetChannel();
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_CANARY:
      return "canary";
    case chrome::VersionInfo::CHANNEL_DEV:
      return "dev";
    case chrome::VersionInfo::CHANNEL_BETA:
      return "beta";
    case chrome::VersionInfo::CHANNEL_STABLE:
      return "stable";
    default:
      return "none";
  }
}

}  // namespace

NotificationPromo::NotificationPromo(Profile* profile)
    : profile_(profile),
      prefs_(profile_->GetPrefs()),
      start_(0.0),
      end_(0.0),
      num_groups_(kDefaultGroupSize),
      initial_segment_(0),
      increment_(1),
      time_slice_(0),
      max_group_(0),
      max_views_(0),
      group_(0),
      views_(0),
      closed_(false),
      gplus_required_(false),
      new_notification_(false) {
  DCHECK(profile);
  DCHECK(prefs_);
}

NotificationPromo::~NotificationPromo() {}

void NotificationPromo::InitFromJson(const DictionaryValue& json) {
  DictionaryValue* root = NULL;
  if (!json.GetDictionary("ntp_notification_promo", &root))
    return;

  // Strings. Assume the first one is the promo text.
  DictionaryValue* strings;
  if (root->GetDictionary("strings", &strings)) {
    DictionaryValue::Iterator iter(*strings);
    iter.value().GetAsString(&promo_text_);
    DVLOG(1) << "promo_text_=" << promo_text_;
  }

  // Date.
  ListValue* date_list;
  if (root->GetList("date", &date_list)) {
    DictionaryValue* date;
    if (date_list->GetDictionary(0, &date)) {
      std::string time_str;
      base::Time time;
      if (date->GetString("start", &time_str) &&
          base::Time::FromString(time_str.c_str(), &time)) {
        start_ = time.ToDoubleT();
        DVLOG(1) << "start str=" << time_str
                   << ", start_="<< base::DoubleToString(start_);
      }
      if (date->GetString("end", &time_str) &&
          base::Time::FromString(time_str.c_str(), &time)) {
        end_ = time.ToDoubleT();
        DVLOG(1) << "end str =" << time_str
                   << ", end_=" << base::DoubleToString(end_);
      }
    }
  }

  // Grouping.
  DictionaryValue* grouping;
  if (root->GetDictionary("grouping", &grouping)) {
    grouping->GetInteger("buckets", &num_groups_);
    grouping->GetInteger("segment", &initial_segment_);
    grouping->GetInteger("increment", &increment_);
    grouping->GetInteger("increment_frequency", &time_slice_);
    grouping->GetInteger("increment_max", &max_group_);

    DVLOG(1) << "num_groups_=" << num_groups_;
    DVLOG(1) << "initial_segment_ = " << initial_segment_;
    DVLOG(1) << "increment_ = " << increment_;
    DVLOG(1) << "time_slice_ = " << time_slice_;
    DVLOG(1) << "max_group_ = " << max_group_;
  }

  // Payload.
  DictionaryValue* payload;
  if (root->GetDictionary("payload", &payload)) {
    payload->GetBoolean("gplus_required", &gplus_required_);

    DVLOG(1) << "gplus_required_ = " << gplus_required_;
  }

  root->GetInteger("max_views", &max_views_);
  DVLOG(1) << "max_views_ " << max_views_;

  CheckForNewNotification();
}

void NotificationPromo::CheckForNewNotification() {
  const double old_start = GetTimeFromPrefs(prefs_, prefs::kNtpPromoStart);
  const double old_end = GetTimeFromPrefs(prefs_, prefs::kNtpPromoEnd);

  new_notification_ = old_start != start_ || old_end != end_;
  if (new_notification_)
    OnNewNotification();
}

void NotificationPromo::OnNewNotification() {
  // Create a new promo group.
  group_ = base::RandInt(0, num_groups_ - 1);
  WritePrefs();
}

// static
void NotificationPromo::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kNtpPromoLine,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);

  prefs->RegisterDoublePref(prefs::kNtpPromoStart,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kNtpPromoEnd,
                            0,
                            PrefService::UNSYNCABLE_PREF);

  prefs->RegisterIntegerPref(prefs::kNtpPromoNumGroups,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNtpPromoInitialSegment,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNtpPromoIncrement,
                             1,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNtpPromoGroupTimeSlice,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNtpPromoGroupMax,
                             0,
                             PrefService::UNSYNCABLE_PREF);

  prefs->RegisterIntegerPref(prefs::kNtpPromoViewsMax,
                             0,
                             PrefService::UNSYNCABLE_PREF);

  prefs->RegisterIntegerPref(prefs::kNtpPromoGroup,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNtpPromoViews,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kNtpPromoClosed,
                             false,
                             PrefService::UNSYNCABLE_PREF);

  prefs->RegisterBooleanPref(prefs::kNtpPromoGplusRequired,
                             false,
                             PrefService::UNSYNCABLE_PREF);

  // TODO(achuith): Delete this in M22.
  prefs->RegisterIntegerPref(prefs::kNtpPromoBuild,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNtpPromoPlatform,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->ClearPref(prefs::kNtpPromoBuild);
  prefs->ClearPref(prefs::kNtpPromoPlatform);
}

void NotificationPromo::WritePrefs() {
  prefs_->SetString(prefs::kNtpPromoLine, promo_text_);

  prefs_->SetDouble(prefs::kNtpPromoStart, start_);
  prefs_->SetDouble(prefs::kNtpPromoEnd, end_);

  prefs_->SetInteger(prefs::kNtpPromoNumGroups, num_groups_);
  prefs_->SetInteger(prefs::kNtpPromoInitialSegment, initial_segment_);
  prefs_->SetInteger(prefs::kNtpPromoIncrement, increment_);
  prefs_->SetInteger(prefs::kNtpPromoGroupTimeSlice, time_slice_);
  prefs_->SetInteger(prefs::kNtpPromoGroupMax, max_group_);

  prefs_->SetInteger(prefs::kNtpPromoViewsMax, max_views_);

  prefs_->SetInteger(prefs::kNtpPromoGroup, group_);
  prefs_->SetInteger(prefs::kNtpPromoViews, views_);
  prefs_->SetBoolean(prefs::kNtpPromoClosed, closed_);

  prefs_->SetBoolean(prefs::kNtpPromoGplusRequired, gplus_required_);
}

void NotificationPromo::InitFromPrefs() {
  promo_text_ = prefs_->GetString(prefs::kNtpPromoLine);

  start_ = prefs_->GetDouble(prefs::kNtpPromoStart);
  end_ = prefs_->GetDouble(prefs::kNtpPromoEnd);

  num_groups_ = prefs_->GetInteger(prefs::kNtpPromoNumGroups);
  initial_segment_ = prefs_->GetInteger(prefs::kNtpPromoInitialSegment);
  increment_ = prefs_->GetInteger(prefs::kNtpPromoIncrement);
  time_slice_ = prefs_->GetInteger(prefs::kNtpPromoGroupTimeSlice);
  max_group_ = prefs_->GetInteger(prefs::kNtpPromoGroupMax);

  max_views_ = prefs_->GetInteger(prefs::kNtpPromoViewsMax);

  group_ = prefs_->GetInteger(prefs::kNtpPromoGroup);
  views_ = prefs_->GetInteger(prefs::kNtpPromoViews);
  closed_ = prefs_->GetBoolean(prefs::kNtpPromoClosed);

  gplus_required_ = prefs_->GetBoolean(prefs::kNtpPromoGplusRequired);
}

bool NotificationPromo::CanShow() const {
  return !closed_ &&
      !promo_text_.empty() &&
      group_ < max_group_ &&
      !ExceedsMaxViews() &&
      base::Time::FromDoubleT(StartTimeForGroup()) < base::Time::Now() &&
      base::Time::FromDoubleT(EndTime()) > base::Time::Now() &&
      IsGPlusRequired();
}

void NotificationPromo::HandleClosed() {
  content::RecordAction(UserMetricsAction("NTPPromoClosed"));
  prefs_->SetBoolean(prefs::kNtpPromoClosed, true);
}

bool NotificationPromo::HandleViewed() {
  content::RecordAction(UserMetricsAction("NTPPromoShown"));
  if (prefs_->HasPrefPath(prefs::kNtpPromoViewsMax))
    max_views_ = prefs_->GetInteger(prefs::kNtpPromoViewsMax);

  if (prefs_->HasPrefPath(prefs::kNtpPromoViews))
    views_ = prefs_->GetInteger(prefs::kNtpPromoViews);

  prefs_->SetInteger(prefs::kNtpPromoViews, ++views_);
  return ExceedsMaxViews();
}

bool NotificationPromo::ExceedsMaxViews() const {
  return (max_views_ == 0) ? false : views_ >= max_views_;
}

bool NotificationPromo::IsGPlusRequired() const {
  return !gplus_required_ || prefs_->GetBoolean(prefs::kIsGooglePlusUser);
}

// static
GURL NotificationPromo::PromoServerURL() {
  GURL url(promo_server_url);
  url = chrome_common_net::AppendQueryParameter(
      url, "dist", ChannelString());
  url = chrome_common_net::AppendQueryParameter(
      url, "osname", PlatformString());
  url = chrome_common_net::AppendQueryParameter(
      url, "branding", chrome::VersionInfo().Version());
  DVLOG(1) << "PromoServerURL=" << url.spec();
  // Note that locale param is added by WebResourceService.
  return url;
}

double NotificationPromo::StartTimeForGroup() const {
  if (group_ < initial_segment_)
    return start_;
  return start_ +
      std::ceil(static_cast<float>(group_ - initial_segment_ + 1) / increment_)
      * time_slice_;
}

double NotificationPromo::EndTime() const {
  return end_;
}
