// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/notification_promo.h"

#include <vector>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "googleurl/src/gurl.h"

using content::UserMetricsAction;

namespace {

// Maximum number of views.
const int kMaxViews = 1000;

// Maximum number of hours for each time slice (4 weeks).
const int kMaxTimeSliceHours = 24 * 7 * 4;

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

}  // namespace

NotificationPromo::NotificationPromo(Profile* profile, Delegate* delegate)
    : profile_(profile),
      delegate_(delegate),
      prefs_(profile_->GetPrefs()),
      start_(0.0),
      end_(0.0),
      build_(PromoResourceService::NO_BUILD),
      time_slice_(0),
      max_group_(0),
      max_views_(0),
      platform_(PLATFORM_NONE),
      group_(0),
      views_(0),
      text_(),
      closed_(false) {
  DCHECK(profile);
  DCHECK(prefs_);
}

NotificationPromo::~NotificationPromo() {}

void NotificationPromo::InitFromJson(const DictionaryValue& json) {
  DictionaryValue* dict;
  if (json.GetDictionary(kHeaderProperty, &dict)) {
    ListValue* answers;
    if (dict->GetList(kArrayProperty, &answers)) {
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

void NotificationPromo::Parse(const DictionaryValue* dict) {
  std::string key;
  if (dict->GetString(kIdentifierProperty, &key)) {
    if (key == kStartPropertyValue) {
      ParseParams(dict);
      dict->GetString(kTextProperty, &text_);
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
      OutOfBounds(time_slice_, 0, kMaxTimeSliceHours) ||
      OutOfBounds(max_group_, 0, kMaxGroupSize) ||
      OutOfBounds(max_views_, 0, kMaxViews) ||
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
  // Trigger a new notification if the times have changed.
  if (old_start != start_ || old_end != end_) {
    OnNewNotification();
    start = StartTimeWithOffset();
    end = end_;
    new_notification = true;
  }
  if (delegate_) {
    // If no change needed, call delegate with default values (this
    // is for testing purposes).
    delegate_->OnNotificationParsed(start, end, new_notification);
  }
}

void NotificationPromo::OnNewNotification() {
  group_ = NewGroup();
  WritePrefs();
}

// static
int NotificationPromo::NewGroup() {
  return base::RandInt(0, kMaxGroupSize);
}

// static
void NotificationPromo::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDoublePref(prefs::kNtpPromoStart,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kNtpPromoEnd,
                            0,
                            PrefService::UNSYNCABLE_PREF);

  prefs->RegisterIntegerPref(prefs::kNtpPromoBuild,
                             PromoResourceService::NO_BUILD,
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
  prefs->RegisterIntegerPref(prefs::kNtpPromoPlatform,
                             PLATFORM_NONE,
                             PrefService::UNSYNCABLE_PREF);

  prefs->RegisterStringPref(prefs::kNtpPromoLine,
                            std::string(),
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
  prefs_->SetDouble(prefs::kNtpPromoStart, start_);
  prefs_->SetDouble(prefs::kNtpPromoEnd, end_);

  prefs_->SetInteger(prefs::kNtpPromoBuild, build_);
  prefs_->SetInteger(prefs::kNtpPromoGroupTimeSlice, time_slice_);
  prefs_->SetInteger(prefs::kNtpPromoGroupMax, max_group_);
  prefs_->SetInteger(prefs::kNtpPromoViewsMax, max_views_);
  prefs_->SetInteger(prefs::kNtpPromoPlatform, platform_);

  prefs_->SetString(prefs::kNtpPromoLine, text_);
  prefs_->SetInteger(prefs::kNtpPromoGroup, group_);
  prefs_->SetInteger(prefs::kNtpPromoViews, views_);
  prefs_->SetBoolean(prefs::kNtpPromoClosed, closed_);
}

void NotificationPromo::InitFromPrefs() {
  start_ = prefs_->GetDouble(prefs::kNtpPromoStart);
  end_ = prefs_->GetDouble(prefs::kNtpPromoEnd);
  build_ = prefs_->GetInteger(prefs::kNtpPromoBuild);
  time_slice_ = prefs_->GetInteger(prefs::kNtpPromoGroupTimeSlice);
  max_group_ = prefs_->GetInteger(prefs::kNtpPromoGroupMax);
  max_views_ = prefs_->GetInteger(prefs::kNtpPromoViewsMax);
  platform_ = prefs_->GetInteger(prefs::kNtpPromoPlatform);
  text_ = prefs_->GetString(prefs::kNtpPromoLine);
  group_ = prefs_->GetInteger(prefs::kNtpPromoGroup);
  views_ = prefs_->GetInteger(prefs::kNtpPromoViews);
  closed_ = prefs_->GetBoolean(prefs::kNtpPromoClosed);
}

bool NotificationPromo::CanShow() const {
  return !closed_ &&
      !text_.empty() &&
      group_ < max_group_ &&
      views_ < max_views_ &&
      IsPlatformAllowed(platform_) &&
      IsBuildAllowed(build_) &&
      base::Time::FromDoubleT(StartTimeWithOffset()) < base::Time::Now() &&
      base::Time::FromDoubleT(end_) > base::Time::Now();
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
  return views_ >= max_views_;
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

double NotificationPromo::StartTimeWithOffset() const {
  // Adjust start using group and time slice, adjusted from hours to seconds.
  static const double kSecondsInHour = 60.0 * 60.0;
  return start_ + group_ * time_slice_ * kSecondsInHour;
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

bool NotificationPromo::operator==(const NotificationPromo& other) const {
  return prefs_ == other.prefs_ &&
         delegate_ == other.delegate_ &&
         start_ == other.start_ &&
         end_ == other.end_ &&
         build_ == other.build_ &&
         time_slice_ == other.time_slice_ &&
         max_group_ == other.max_group_ &&
         max_views_ == other.max_views_ &&
         platform_ == other.platform_ &&
         group_ == other.group_ &&
         views_ == other.views_ &&
         text_ == other.text_ &&
         closed_ == other.closed_;
}
