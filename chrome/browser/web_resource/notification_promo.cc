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

// Users are randomly assigned to one of kDefaultGroupSize + 1 buckets, in order
// to be able to roll out promos slowly, or display different promos to
// different groups.
const int kDefaultGroupSize = 100;

bool OutOfBounds(int var, int min, int max) {
  return var < min || var > max;
}

const char kHeaderProperty[] = "topic";
const char kArrayProperty[] = "answers";
const char kIdentifierProperty[] = "name";
const char kStartPropertyValue[] = "promo_start";
const char kEndPropertyValue[] = "promo_end";
const char kTextProperty[] = "tooltip";
const char kTimeProperty[] = "inproduct";
const char kParamsProperty[] = "question";

const char promo_server_url[] = "http://clients3.google.com/crsignal/client";

// Time getters.
double GetTimeFromDict(const DictionaryValue* dict) {
  std::string time_str;
  if (!dict->GetString(kTimeProperty, &time_str))
    return 0.0;

  base::Time time;
  if (time_str.empty() || !base::Time::FromString(time_str.c_str(), &time))
    return 0.0;

  return time.ToDoubleT();
}

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
  const chrome::VersionInfo::Channel channel =
      PromoResourceService::GetChannel();
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

NotificationPromo::NotificationPromo(Profile* profile, Delegate* delegate)
    : profile_(profile),
      delegate_(delegate),
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
      build_(PromoResourceService::ALL_BUILDS),
      platform_(PLATFORM_ALL) {
  DCHECK(profile);
  DCHECK(prefs_);
}

NotificationPromo::~NotificationPromo() {}

void NotificationPromo::InitFromJsonLegacy(const DictionaryValue& json) {
  DictionaryValue* header = NULL;
  if (json.GetDictionary(kHeaderProperty, &header)) {
    ListValue* answers;
    if (header->GetList(kArrayProperty, &answers)) {
      for (ListValue::const_iterator it = answers->begin();
           it != answers->end();
           ++it) {
        if ((*it)->IsType(Value::TYPE_DICTIONARY))
          Parse(static_cast<DictionaryValue*>(*it));
      }
    }
  }
  CheckForNewNotification();
}

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

  root->GetInteger("max_views", &max_views_);
  DVLOG(1) << "max_views_ " << max_views_;

  CheckForNewNotification();
}

void NotificationPromo::Parse(const DictionaryValue* dict) {
  std::string key;
  if (dict->GetString(kIdentifierProperty, &key)) {
    if (key == kStartPropertyValue) {
      ParseParams(dict);
      dict->GetString(kTextProperty, &promo_text_);
      start_ = GetTimeFromDict(dict);
    } else if (key == kEndPropertyValue) {
      end_ = GetTimeFromDict(dict);
    }
  }
}

void NotificationPromo::ParseParams(const DictionaryValue* dict) {
  std::string question;
  if (!dict->GetString(kParamsProperty, &question))
    return;

  size_t index = 0;
  bool err = false;

  build_ = GetNextQuestionValue(question, &index, &err);
  time_slice_ = GetNextQuestionValue(question, &index, &err);
  max_group_ = GetNextQuestionValue(question, &index, &err);
  max_views_ = GetNextQuestionValue(question, &index, &err);
  platform_ = GetNextQuestionValue(question, &index, &err);

  if (err ||
      OutOfBounds(build_, PromoResourceService::NO_BUILD,
          PromoResourceService::ALL_BUILDS) ||
      OutOfBounds(max_group_, 0, kDefaultGroupSize) ||
      OutOfBounds(platform_, PLATFORM_NONE, PLATFORM_ALL)) {
    // If values are not valid, do not show promo notification.
    DLOG(ERROR) << "Invalid server data, question=" << question <<
        ", build=" << build_ <<
        ", time_slice=" << time_slice_ <<
        ", max_group=" << max_group_ <<
        ", max_views=" << max_views_ <<
        ", platform_=" << platform_;
    build_ = PromoResourceService::NO_BUILD;
    time_slice_ = 0;
    max_group_ = 0;
    max_views_ = 0;
    platform_ = PLATFORM_NONE;
  }
}

void NotificationPromo::CheckForNewNotification() {
  double start = 0.0;
  double end = 0.0;
  bool new_notification = false;

  const double old_start = GetTimeFromPrefs(prefs_, prefs::kNtpPromoStart);
  const double old_end = GetTimeFromPrefs(prefs_, prefs::kNtpPromoEnd);

  if (old_start != start_ || old_end != end_) {
    OnNewNotification();
    // For testing.
    start = StartTimeForGroup();
    end = end_;
    new_notification = true;
  }
  // For testing.
  if (delegate_) {
    // If no change needed, call delegate with default values.
    delegate_->OnNotificationParsed(start, end, new_notification);
  }
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

  prefs->RegisterIntegerPref(prefs::kNtpPromoBuild,
                             PromoResourceService::NO_BUILD,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNtpPromoPlatform,
                             PLATFORM_NONE,
                             PrefService::UNSYNCABLE_PREF);

  // TODO(achuith): Delete code below in M21. http://crbug.com/125974.
  prefs->RegisterBooleanPref(prefs::kNtpPromoIsLoggedInToPlus,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNtpPromoFeatureMask,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->ClearPref(prefs::kNtpPromoIsLoggedInToPlus);
  prefs->ClearPref(prefs::kNtpPromoFeatureMask);
}

// static
NotificationPromo* NotificationPromo::Create(Profile *profile,
    NotificationPromo::Delegate * delegate) {
  return new NotificationPromo(profile, delegate);
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

  prefs_->SetInteger(prefs::kNtpPromoBuild, build_);
  prefs_->SetInteger(prefs::kNtpPromoPlatform, platform_);
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

  build_ = prefs_->GetInteger(prefs::kNtpPromoBuild);
  platform_ = prefs_->GetInteger(prefs::kNtpPromoPlatform);
}

bool NotificationPromo::CanShow() const {
  return !closed_ &&
      !promo_text_.empty() &&
      group_ < max_group_ &&
      !ExceedsMaxViews() &&
      base::Time::FromDoubleT(StartTimeForGroup()) < base::Time::Now() &&
      base::Time::FromDoubleT(end_) > base::Time::Now() &&
      IsBuildAllowed(build_) &&
      IsPlatformAllowed(platform_);
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

bool NotificationPromo::IsBuildAllowed(int builds_allowed) const {
  if (delegate_)  // For testing.
    return delegate_->IsBuildAllowed(builds_allowed);
  else
    return PromoResourceService::IsBuildTargeted(
        PromoResourceService::GetChannel(), builds_allowed);
}

bool NotificationPromo::IsPlatformAllowed(int target_platform) const {
  const int current_platform = delegate_? delegate_->CurrentPlatform()
                                        : CurrentPlatform();
  return (target_platform & current_platform) != 0;
}

// static
int NotificationPromo::CurrentPlatform() {
  // Ignore OS_ANDROID, OS_FREEBSD, OS_OPENBSD, OS_SOLARIS, OS_NACL for now.
  // Order is important - OS_LINUX and OS_CHROMEOS can both be defined.
#if defined(OS_WIN)
  return PLATFORM_WIN;
#elif defined(OS_MACOSX)
  return PLATFORM_MAC;
#elif defined(OS_CHROMEOS)
  return PLATFORM_CHROMEOS;
#elif defined(OS_LINUX)
  return PLATFORM_LINUX;
#else
  return PLATFORM_NONE;
#endif
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
  // TODO(achuith): Write unittests for this.
  if (group_ < initial_segment_)
    return start_;
  return start_ +
      std::ceil(static_cast<float>(group_ - initial_segment_ + 1) / increment_)
      * time_slice_;
}

// static
int NotificationPromo::GetNextQuestionValue(const std::string& question,
                                            size_t* index,
                                            bool* err) {
  if (*err)
    return 0;

  size_t new_index = question.find(':', *index);
  // Note that substr correctly handles npos.
  std::string fragment(question.substr(*index, new_index - *index));
  *index = new_index + 1;

  int value;
  *err = !base::StringToInt(fragment, &value);
  return *err ? 0 : value;
}
