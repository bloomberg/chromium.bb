// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/notification_promo.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"

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

static const char kGPlusDomainUrl[] = "http://plus.google.com/";
static const char kGPlusDomainSecureCookieId[] = "SID=";
static const char kSplitStringToken = ';';

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
      feature_mask_(NO_FEATURE),
      group_(0),
      views_(0),
      text_(),
      closed_(false),
      gplus_(false) {
  DCHECK(profile);
  DCHECK(prefs_);
}

NotificationPromo::~NotificationPromo() {}

void NotificationPromo::InitFromJson(const DictionaryValue& json,
                                     bool do_cookie_check) {
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
  if (do_cookie_check) {
    scoped_refptr<net::URLRequestContextGetter> getter(
        profile_->GetRequestContext());
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
          base::Bind(&NotificationPromo::GetCookies, this, getter));
  } else {
    CheckForNewNotification(false);
  }
}

// static
bool NotificationPromo::CheckForGPlusCookie(const std::string& cookies) {
  std::vector<std::string> cookie_list;
  base::SplitString(cookies, kSplitStringToken, &cookie_list);
  for (std::vector<std::string>::const_iterator current = cookie_list.begin();
       current != cookie_list.end();
       ++current) {
    if ((*current).find(kGPlusDomainSecureCookieId) == 0) {
      return true;
    }
  }
  return false;
}

void NotificationPromo::GetCookiesCallback(const std::string& cookies) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  bool found_cookie = NotificationPromo::CheckForGPlusCookie(cookies);
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&NotificationPromo::CheckForNewNotification, this,
                 found_cookie));
}

void NotificationPromo::GetCookies(
    scoped_refptr<net::URLRequestContextGetter> getter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  getter->GetURLRequestContext()->cookie_store()->
      GetCookiesWithOptionsAsync(
          GURL(kGPlusDomainUrl), net::CookieOptions(),
          base::Bind(&NotificationPromo::GetCookiesCallback, this));
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
  feature_mask_ = GetNextQuestionValue(question, &index, &err);

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
        ", platform_=" << platform_ <<
        ", feature_mask=" << feature_mask_;
    build_ = PromoResourceService::NO_BUILD;
    time_slice_ = 0;
    max_group_ = 0;
    max_views_ = 0;
    platform_ = PLATFORM_NONE;
    feature_mask_ = 0;
  }
}

void NotificationPromo::CheckForNewNotification(bool found_cookie) {
  double start = 0.0;
  double end = 0.0;
  bool new_notification = false;

  gplus_ = found_cookie;
  const double old_start = GetTimeFromPrefs(prefs_, prefs::kNTPPromoStart);
  const double old_end = GetTimeFromPrefs(prefs_, prefs::kNTPPromoEnd);
  const bool old_gplus = prefs_->GetBoolean(prefs::kNTPPromoIsLoggedInToPlus);
  const bool has_feature_mask =
      prefs_->HasPrefPath(prefs::kNTPPromoFeatureMask);
  // Trigger a new notification if the times have changed, or if
  // we previously never wrote out a feature_mask, or if the user's gplus
  // cookies have changed.
  if (old_start != start_ || old_end != end_ || old_gplus != gplus_ ||
      !has_feature_mask) {
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
  prefs->RegisterBooleanPref(prefs::kNTPPromoIsLoggedInToPlus,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoFeatureMask,
                             0,
                             PrefService::UNSYNCABLE_PREF);
}

// static
NotificationPromo* NotificationPromo::Create(Profile *profile,
    NotificationPromo::Delegate * delegate) {
  return new NotificationPromo(profile, delegate);
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
  prefs_->SetBoolean(prefs::kNTPPromoIsLoggedInToPlus, gplus_);
  prefs_->SetInteger(prefs::kNTPPromoFeatureMask, feature_mask_);
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

  if (prefs_->HasPrefPath(prefs::kNTPPromoIsLoggedInToPlus))
    gplus_ = prefs_->GetBoolean(prefs::kNTPPromoIsLoggedInToPlus);

  if (prefs_->HasPrefPath(prefs::kNTPPromoFeatureMask))
    feature_mask_ = prefs_->GetInteger(prefs::kNTPPromoFeatureMask);
}

bool NotificationPromo::CanShow() const {
  return !closed_ &&
      !text_.empty() &&
      group_ < max_group_ &&
      views_ < max_views_ &&
      IsPlatformAllowed(platform_) &&
      IsBuildAllowed(build_) &&
      base::Time::FromDoubleT(StartTimeWithOffset()) < base::Time::Now() &&
      base::Time::FromDoubleT(end_) > base::Time::Now() &&
      (!(feature_mask_ & NotificationPromo::FEATURE_GPLUS) || gplus_);
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
         closed_ == other.closed_ &&
         gplus_ == other.gplus_ &&
         feature_mask_ == other.feature_mask_;
}
