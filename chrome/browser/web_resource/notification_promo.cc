// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/notification_promo.h"

#include <cmath>
#include <vector>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
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

#if defined(OS_ANDROID)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif  // defined(OS_ANDROID)

using content::UserMetricsAction;

namespace {

const int kDefaultGroupSize = 100;

const char promo_server_url[] = "https://clients3.google.com/crsignal/client";

#if defined(OS_ANDROID)
const int kCurrentMobilePayloadFormatVersion = 3;
#endif  // defined(OS_ANDROID)

double GetTimeFromPrefs(PrefService* prefs, const char* pref) {
  return prefs->HasPrefPath(pref) ? prefs->GetDouble(pref) : 0.0;
}

// Returns a string suitable for the Promo Server URL 'osname' value.
std::string PlatformString() {
#if defined(OS_WIN)
  return "win";
#elif defined(OS_IOS)
  // TODO(noyau): add iOS-specific implementation
  const bool isTablet = false;
  return std::string("ios-") + (isTablet ? "tablet" : "phone");
#elif defined(OS_MACOSX)
  return "mac";
#elif defined(OS_CHROMEOS)
  return "chromeos";
#elif defined(OS_LINUX)
  return "linux";
#elif defined(OS_ANDROID)
  const bool isTablet =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kTabletUi);
  return std::string("android-") + (isTablet ? "tablet" : "phone");
#else
  return "none";
#endif
}

// Returns a string suitable for the Promo Server URL 'dist' value.
const char* ChannelString() {
#if defined (OS_WIN)
  // GetChannel hits the registry on Windows. See http://crbug.com/70898.
  // TODO(achuith): Move NotificationPromo::PromoServerURL to the blocking pool.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
#endif
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
#if defined(OS_ANDROID)
      promo_action_args_(new base::ListValue),
#endif  // defined(OS_ANDROID)
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
  ListValue* promo_list = NULL;
#if !defined(OS_ANDROID)
  if (!json.GetList("ntp_notification_promo", &promo_list))
    return;
#else
  if (!json.GetList("mobile_ntp_sync_promo", &promo_list)) {
    LOG(ERROR) << "Malfromed JSON: not a mobile_ntp_sync_promo";
    return;
  }
#endif  // !defined(OS_ANDROID)

  // No support for multiple promos yet. Only consider the first one.
  DictionaryValue* promo = NULL;
  if (!promo_list->GetDictionary(0, &promo))
    return;

  // Strings. Assume the first one is the promo text.
  DictionaryValue* strings = NULL;
  if (promo->GetDictionary("strings", &strings)) {
#if !defined(OS_ANDROID)
    DictionaryValue::Iterator iter(*strings);
    iter.value().GetAsString(&promo_text_);
    DVLOG(1) << "promo_text_=" << promo_text_;
#endif  // defined(OS_ANDROID)
  }

  // Date.
  ListValue* date_list = NULL;
  if (promo->GetList("date", &date_list)) {
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
  DictionaryValue* grouping = NULL;
  if (promo->GetDictionary("grouping", &grouping)) {
    grouping->GetInteger("buckets", &num_groups_);
    grouping->GetInteger("segment", &initial_segment_);
    grouping->GetInteger("increment", &increment_);
    grouping->GetInteger("increment_frequency", &time_slice_);
    grouping->GetInteger("increment_max", &max_group_);

    DVLOG(1) << "num_groups_ = " << num_groups_
             << ", initial_segment_ = " << initial_segment_
             << ", increment_ = " << increment_
             << ", time_slice_ = " << time_slice_
             << ", max_group_ = " << max_group_;
  }

  // Payload.
  DictionaryValue* payload = NULL;
  if (promo->GetDictionary("payload", &payload)) {
    payload->GetBoolean("gplus_required", &gplus_required_);

    DVLOG(1) << "gplus_required_ = " << gplus_required_;
  }

  promo->GetInteger("max_views", &max_views_);
  DVLOG(1) << "max_views_ " << max_views_;

#if defined(OS_ANDROID)
  int payload_version = 0;
  if (!payload) {
    LOG(ERROR) << "Malformed JSON: no payload";
    return;
  }
  if (!strings) {
    LOG(ERROR) << "Malformed JSON: no strings";
    return;
  }
  if (!payload->GetInteger("payload_format_version", &payload_version) ||
      payload_version != kCurrentMobilePayloadFormatVersion) {
    LOG(ERROR) << "Unsupported promo payload_format_version " << payload_version
               << "; expected " << kCurrentMobilePayloadFormatVersion;
    return;
  }
  std::string promo_key_short;
  std::string promo_key_long;
  if (!payload->GetString("promo_message_short", &promo_key_short) ||
      !payload->GetString("promo_message_long", &promo_key_long) ||
      !strings->GetString(promo_key_short, &promo_text_) ||
      !strings->GetString(promo_key_long, &promo_text_long_)) {
    LOG(ERROR) << "Malformed JSON: no promo_message_short or _long";
    return;
  }
  payload->GetString("promo_action_type", &promo_action_type_);
  // We need to be idempotent as the tests call us more than once.
  promo_action_args_.reset(new base::ListValue);
  ListValue* args;
  if (payload->GetList("promo_action_args", &args)) {
    // JSON format for args: "promo_action_args" : [ "<arg1>", "<arg2>"... ]
    // Every value comes from "strings" dictionary, either directly or not.
    // Every arg is either directly a key into "strings" dictionary,
    // or a key into "payload" dictionary with the value that is a key into
    // "strings" dictionary.
    for (std::size_t i = 0; i < args->GetSize(); ++i) {
      std::string name, key, value;
      if (!args->GetString(i, &name) ||
          !(strings->GetString(name, &value) ||
          (payload->GetString(name, &key) &&
              strings->GetString(key, &value)))) {
        LOG(ERROR) << "Malformed JSON: failed to parse promo_action_args";
        return;
      }
      promo_action_args_->Append(base::Value::CreateStringValue(value));
    }
  }
#endif  // defined(OS_ANDROID)

  CheckForNewNotification();
}

void NotificationPromo::CheckForNewNotification() {
  const double old_start = GetTimeFromPrefs(prefs_, prefs::kNtpPromoStart);
  const double old_end = GetTimeFromPrefs(prefs_, prefs::kNtpPromoEnd);
  const std::string old_promo_text = prefs_->GetString(prefs::kNtpPromoLine);

  new_notification_ =
      old_start != start_ || old_end != end_ || old_promo_text != promo_text_;
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
#if defined(OS_ANDROID)
  prefs->RegisterStringPref(prefs::kNtpPromoLineLong,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kNtpPromoActionType,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kNtpPromoActionArgs,
                          new base::ListValue,
                          PrefService::UNSYNCABLE_PREF);
#endif  // defined(OS_ANDROID)

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
#if defined(OS_ANDROID)
  prefs_->SetString(prefs::kNtpPromoLineLong, promo_text_long_);
  prefs_->SetString(prefs::kNtpPromoActionType, promo_action_type_);
  DCHECK(promo_action_args_.get() != NULL);
  prefs_->Set(prefs::kNtpPromoActionArgs, *promo_action_args_.get());
#endif  // defined(OS_ANDROID)

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
#if defined(OS_ANDROID)
  promo_text_long_ = prefs_->GetString(prefs::kNtpPromoLineLong);
  promo_action_type_ = prefs_->GetString(prefs::kNtpPromoActionType);
  const base::ListValue* lv = prefs_->GetList(prefs::kNtpPromoActionArgs);
  DCHECK(lv != NULL);
  promo_action_args_.reset(lv->DeepCopy());
#endif  // defined(OS_ANDROID)

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
      !ExceedsMaxGroup() &&
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

bool NotificationPromo::ExceedsMaxGroup() const {
  return (max_group_ == 0) ? false : group_ >= max_group_;
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
