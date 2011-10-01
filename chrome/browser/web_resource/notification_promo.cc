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
      build_(0),
      time_slice_(0),
      max_group_(0),
      max_views_(0),
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

  if (err ||
      OutOfBounds(build_, PromoResourceService::NO_BUILD,
          PromoResourceService::ALL_BUILDS) ||
      OutOfBounds(time_slice_, 0, kMaxTimeSliceHours) ||
      OutOfBounds(max_group_, 0, kMaxGroupSize) ||
      OutOfBounds(max_views_, 0, kMaxViews)) {
    // If values are not valid, do not show promo notification.
    DLOG(ERROR) << "Invalid server data, question=" << question <<
        ", build=" << build_ <<
        ", time_slice=" << time_slice_ <<
        ", max_group=" << max_group_ <<
        ", max_views=" << max_views_;
    build_ = PromoResourceService::NO_BUILD;
    time_slice_ = 0;
    max_group_ = 0;
    max_views_ = 0;
  }
}

void NotificationPromo::CheckForNewNotification() {
  const double old_start = GetTimeFromPrefs(prefs_, prefs::kNTPPromoStart);
  const double old_end = GetTimeFromPrefs(prefs_, prefs::kNTPPromoEnd);
  const bool has_views = prefs_->HasPrefPath(prefs::kNTPPromoViewsMax);

  // Trigger a new notification if the times have changed, or if
  // we previously never wrote out a max_views preference.
  if (old_start != start_ || old_end != end_ || !has_views)
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
                             PromoResourceService::ALL_BUILDS,
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

  prefs_->SetString(prefs::kNTPPromoLine, text_);
  prefs_->SetInteger(prefs::kNTPPromoGroup, group_);
  prefs_->SetInteger(prefs::kNTPPromoViews, views_);
  prefs_->SetBoolean(prefs::kNTPPromoClosed, closed_);
}

void NotificationPromo::InitFromPrefs() {
  if (prefs_->HasPrefPath(prefs::kNTPPromoStart))
    start_ = prefs_->GetDouble(prefs::kNTPPromoStart);

  if (prefs_->HasPrefPath(prefs::kNTPPromoEnd))
    end_ = prefs_->GetDouble(prefs::kNTPPromoEnd);

  if (prefs_->HasPrefPath(prefs::kNTPPromoBuild))
    build_ = prefs_->GetInteger(prefs::kNTPPromoBuild);

  if (prefs_->HasPrefPath(prefs::kNTPPromoGroupTimeSlice))
    time_slice_ = prefs_->GetInteger(prefs::kNTPPromoGroupTimeSlice);

  if (prefs_->HasPrefPath(prefs::kNTPPromoGroupMax))
    max_group_ = prefs_->GetInteger(prefs::kNTPPromoGroupMax);

  if (prefs_->HasPrefPath(prefs::kNTPPromoViewsMax))
    max_views_ = prefs_->GetInteger(prefs::kNTPPromoViewsMax);

  if (prefs_->HasPrefPath(prefs::kNTPPromoLine))
    text_ = prefs_->GetString(prefs::kNTPPromoLine);

  if (prefs_->HasPrefPath(prefs::kNTPPromoGroup))
    group_ = prefs_->GetInteger(prefs::kNTPPromoGroup);

  if (prefs_->HasPrefPath(prefs::kNTPPromoViews))
    views_ = prefs_->GetInteger(prefs::kNTPPromoViews);

  if (prefs_->HasPrefPath(prefs::kNTPPromoClosed))
    closed_ = prefs_->GetBoolean(prefs::kNTPPromoClosed);
}

bool NotificationPromo::CanShow() const {
  return !closed_ &&
      !text_.empty() &&
      group_ < max_group_ &&
      views_ < max_views_ &&
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
         group_ == other.group_ &&
         views_ == other.views_ &&
         text_ == other.text_ &&
         closed_ == other.closed_;
}
