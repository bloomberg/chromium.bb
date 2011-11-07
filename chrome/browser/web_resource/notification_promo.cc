// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/notification_promo.h"

#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"

namespace {

// Maximum number of views.
static const int kMaxViews = 1000;

// Maximum number of hours for each time slice (4 weeks).
static const int kMaxTimeSliceHours = 24 * 7 * 4;

bool OutOfBounds(int var, int min, int max) {
  return var < min || var > max;
}

static const char kHeaderProperty[] = "topic";
static const char kArrayProperty[] = "answers";
static const char kIdentifierProperty[] = "name";
static const char kStartPropertyValue[] = "promo_start";
static const char kEndPropertyValue[] = "promo_end";
static const char kTextProperty[] = "tooltip";
static const char kTimeProperty[] = "inproduct";
static const char kParamsProperty[] = "question";

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

NotificationPromo::NotificationPromo(PrefService* prefs, Delegate* delegate)
    : prefs_(prefs),
      delegate_(delegate),
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
  DCHECK(prefs);
}

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
  const double old_start = GetTimeFromPrefs(prefs_, prefs::kNTPPromoStart);
  const double old_end = GetTimeFromPrefs(prefs_, prefs::kNTPPromoEnd);
  const bool has_platform = prefs_->HasPrefPath(prefs::kNTPPromoPlatform);

  // Trigger a new notification if the times have changed, or if
  // we previously never wrote out a platform preference. This handles
  // the case where we update to a new client in the middle of a promo.
  if (old_start != start_ || old_end != end_ || !has_platform)
    OnNewNotification();
}

void NotificationPromo::OnNewNotification() {
  group_ = NewGroup();
  WritePrefs();
  if (delegate_)
    delegate_->OnNewNotification(StartTimeWithOffset(), end_);
}

// static
int NotificationPromo::NewGroup() {
  return base::RandInt(0, kMaxGroupSize);
}

// static
void NotificationPromo::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDoublePref(prefs::kNTPPromoStart,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kNTPPromoEnd,
                            0,
                            PrefService::UNSYNCABLE_PREF);

  prefs->RegisterIntegerPref(prefs::kNTPPromoBuild,
                             PromoResourceService::NO_BUILD,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoGroupTimeSlice,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoGroupMax,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoViewsMax,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoPlatform,
                             PLATFORM_NONE,
                             PrefService::UNSYNCABLE_PREF);

  prefs->RegisterStringPref(prefs::kNTPPromoLine,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoGroup,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoViews,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kNTPPromoClosed,
                             false,
                             PrefService::UNSYNCABLE_PREF);
}


void NotificationPromo::WritePrefs() {
  prefs_->SetDouble(prefs::kNTPPromoStart, start_);
  prefs_->SetDouble(prefs::kNTPPromoEnd, end_);

  prefs_->SetInteger(prefs::kNTPPromoBuild, build_);
  prefs_->SetInteger(prefs::kNTPPromoGroupTimeSlice, time_slice_);
  prefs_->SetInteger(prefs::kNTPPromoGroupMax, max_group_);
  prefs_->SetInteger(prefs::kNTPPromoViewsMax, max_views_);
  prefs_->SetInteger(prefs::kNTPPromoPlatform, platform_);

  prefs_->SetString(prefs::kNTPPromoLine, text_);
  prefs_->SetInteger(prefs::kNTPPromoGroup, group_);
  prefs_->SetInteger(prefs::kNTPPromoViews, views_);
  prefs_->SetBoolean(prefs::kNTPPromoClosed, closed_);
}

void NotificationPromo::InitFromPrefs() {
  start_ = prefs_->GetDouble(prefs::kNTPPromoStart);
  end_ = prefs_->GetDouble(prefs::kNTPPromoEnd);
  build_ = prefs_->GetInteger(prefs::kNTPPromoBuild);
  time_slice_ = prefs_->GetInteger(prefs::kNTPPromoGroupTimeSlice);
  max_group_ = prefs_->GetInteger(prefs::kNTPPromoGroupMax);
  max_views_ = prefs_->GetInteger(prefs::kNTPPromoViewsMax);
  platform_ = prefs_->GetInteger(prefs::kNTPPromoPlatform);
  text_ = prefs_->GetString(prefs::kNTPPromoLine);
  group_ = prefs_->GetInteger(prefs::kNTPPromoGroup);
  views_ = prefs_->GetInteger(prefs::kNTPPromoViews);
  closed_ = prefs_->GetBoolean(prefs::kNTPPromoClosed);
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
  prefs_->SetBoolean(prefs::kNTPPromoClosed, true);
}

bool NotificationPromo::HandleViewed() {
  if (prefs_->HasPrefPath(prefs::kNTPPromoViewsMax))
    max_views_ = prefs_->GetInteger(prefs::kNTPPromoViewsMax);

  if (prefs_->HasPrefPath(prefs::kNTPPromoViews))
    views_ = prefs_->GetInteger(prefs::kNTPPromoViews);

  prefs_->SetInteger(prefs::kNTPPromoViews, ++views_);
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
