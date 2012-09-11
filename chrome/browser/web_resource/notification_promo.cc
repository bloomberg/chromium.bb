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

// The name of the preference that stores the promotion object.
const char kPrefPromoObject[] = "promo";

// Keys in the kPrefPromoObject dictionary; used only here.
const char kPrefPromoText[] = "text";
const char kPrefPromoPayload[] = "payload";
const char kPrefPromoStart[] = "start";
const char kPrefPromoEnd[] = "end";
const char kPrefPromoNumGroups[] = "num_groups";
const char kPrefPromoSegment[] = "segment";
const char kPrefPromoIncrement[] = "increment";
const char kPrefPromoIncrementFrequency[] = "increment_frequency";
const char kPrefPromoIncrementMax[] = "increment_max";
const char kPrefPromoMaxViews[] = "max_views";
const char kPrefPromoGroup[] = "group";
const char kPrefPromoViews[] = "views";
const char kPrefPromoClosed[] = "closed";
const char kPrefPromoGPlusRequired[] = "gplus_required";

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
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kTabletUI);
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

struct PromoMapEntry {
  NotificationPromo::PromoType promo_type;
  const char* promo_type_str;
};

const PromoMapEntry kPromoMap[] = {
    { NotificationPromo::NO_PROMO, "" },
    { NotificationPromo::NTP_NOTIFICATION_PROMO, "ntp_notification_promo" },
    { NotificationPromo::NTP_BUBBLE_PROMO, "ntp_bubble_promo" },
    { NotificationPromo::MOBILE_NTP_SYNC_PROMO, "mobile_ntp_sync_promo" },
};

// Convert PromoType to appropriate string.
const char* PromoTypeToString(NotificationPromo::PromoType promo_type) {
  for (size_t i = 0; i < arraysize(kPromoMap); ++i) {
    if (kPromoMap[i].promo_type == promo_type)
      return kPromoMap[i].promo_type_str;
  }
  NOTREACHED();
  return "";
}

// TODO(achuith): http://crbug.com/143773 remove this by m24
void ClearDeprecatedPrefs(PrefService* prefs) {
#if defined(OS_ANDROID) || defined(OS_IOS)
  const char kNtpPromoLineLong[] = "ntp.promo_line_long";
  const char kNtpPromoActionType[] = "ntp.promo_action_type";
  const char kNtpPromoActionArgs[] = "ntp.promo_action_args";
  prefs->RegisterStringPref(kNtpPromoLineLong,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(kNtpPromoActionType,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(kNtpPromoActionArgs,
                          new base::ListValue,
                          PrefService::UNSYNCABLE_PREF);
  prefs->ClearPref(kNtpPromoLineLong);
  prefs->ClearPref(kNtpPromoActionType);
  prefs->ClearPref(kNtpPromoActionArgs);
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  const char kNtpPromoLine[] = "ntp.promo_line";
  const char kNtpPromoStart[] = "ntp.promo_start";
  const char kNtpPromoEnd[] = "ntp.promo_end";
  const char kNtpPromoNumGroups[] = "ntp.promo_num_groups";
  const char kNtpPromoInitialSegment[] = "ntp.promo_initial_segment";
  const char kNtpPromoIncrement[] = "ntp.promo_increment";
  const char kNtpPromoGroupTimeSlice[] = "ntp.promo_group_timeslice";
  const char kNtpPromoGroupMax[] = "ntp.promo_group_max";
  const char kNtpPromoClosed[] = "ntp.promo_closed";
  const char kNtpPromoGroup[] = "ntp.promo_group";
  const char kNtpPromoViews[] = "ntp.promo_views";
  const char kNtpPromoViewsMax[] = "ntp.promo_views_max";
  const char kNtpPromoGplusRequired[] = "ntp.gplus_required";

  prefs->RegisterStringPref(kNtpPromoLine,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(kNtpPromoStart,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(kNtpPromoEnd,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kNtpPromoNumGroups,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kNtpPromoInitialSegment,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kNtpPromoIncrement,
                             1,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kNtpPromoGroupTimeSlice,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kNtpPromoGroupMax,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kNtpPromoViewsMax,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kNtpPromoGroup,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kNtpPromoViews,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(kNtpPromoClosed,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(kNtpPromoGplusRequired,
                             false,
                             PrefService::UNSYNCABLE_PREF);

  prefs->ClearPref(kNtpPromoLine);
  prefs->ClearPref(kNtpPromoStart);
  prefs->ClearPref(kNtpPromoEnd);
  prefs->ClearPref(kNtpPromoNumGroups);
  prefs->ClearPref(kNtpPromoInitialSegment);
  prefs->ClearPref(kNtpPromoIncrement);
  prefs->ClearPref(kNtpPromoGroupTimeSlice);
  prefs->ClearPref(kNtpPromoGroupMax);
  prefs->ClearPref(kNtpPromoViewsMax);
  prefs->ClearPref(kNtpPromoGroup);
  prefs->ClearPref(kNtpPromoViews);
  prefs->ClearPref(kNtpPromoClosed);
  prefs->ClearPref(kNtpPromoGplusRequired);
}

// Deep-copies a node, replacing any "value" that is a key
// into "strings" dictionary with its value from "strings".
// E.g. for
//   {promo_action_args:['MSG_SHORT']} + strings:{MSG_SHORT:'yes'}
// it will return
//   {promo_action_args:['yes']}
// |node| - a value to be deep copied and resolved.
// |strings| - a dictionary of strings to be used for resolution.
// Returns a _new_ object that is a deep copy with replacements.
// TODO(aruslan): http://crbug.com/144320 Consider moving it to values.cc/h.
base::Value* DeepCopyAndResolveStrings(
    const base::Value* node,
    const base::DictionaryValue* strings) {
  switch (node->GetType()) {
    case base::Value::TYPE_LIST: {
      const base::ListValue* list = static_cast<const base::ListValue*>(node);
      base::ListValue* copy = new base::ListValue;
      for (base::ListValue::const_iterator it = list->begin();
           it != list->end(); ++it) {
        base::Value* child_copy = DeepCopyAndResolveStrings(*it, strings);
        copy->Append(child_copy);
      }
      return copy;
    }

    case Value::TYPE_DICTIONARY: {
      const base::DictionaryValue* dict =
          static_cast<const base::DictionaryValue*>(node);
      base::DictionaryValue* copy = new base::DictionaryValue;
      for (base::DictionaryValue::key_iterator it = dict->begin_keys();
           it != dict->end_keys(); ++it) {
        const base::Value* child = NULL;
        bool rv = dict->GetWithoutPathExpansion(*it, &child);
        DCHECK(rv);
        base::Value* child_copy = DeepCopyAndResolveStrings(child, strings);
        copy->SetWithoutPathExpansion(*it, child_copy);
      }
      return copy;
    }

    case Value::TYPE_STRING: {
      const base::StringValue* str =
          static_cast<const base::StringValue*>(node);
      std::string value;
      bool rv = str->GetAsString(&value);
      DCHECK(rv);
      std::string actual_value;
      if (!strings || !strings->GetString(value, &actual_value))
        actual_value = value;
      return new base::StringValue(actual_value);
    }

    default:
      // For everything else, just make a copy.
      return node->DeepCopy();
  }
}

}  // namespace

NotificationPromo::NotificationPromo(Profile* profile)
    : profile_(profile),
      prefs_(profile_->GetPrefs()),
      promo_type_(NO_PROMO),
      promo_payload_(new base::DictionaryValue()),
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

void NotificationPromo::InitFromJson(const DictionaryValue& json,
                                     PromoType promo_type) {
  promo_type_ = promo_type;
  const ListValue* promo_list = NULL;
  DVLOG(1) << "InitFromJson " << PromoTypeToString(promo_type_);
  if (!json.GetList(PromoTypeToString(promo_type_), &promo_list))
    return;

  // No support for multiple promos yet. Only consider the first one.
  const DictionaryValue* promo = NULL;
  if (!promo_list->GetDictionary(0, &promo))
    return;

  // Date.
  const ListValue* date_list = NULL;
  if (promo->GetList("date", &date_list)) {
    const DictionaryValue* date;
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
  const DictionaryValue* grouping = NULL;
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

  // Strings.
  const DictionaryValue* strings = NULL;
  promo->GetDictionary("strings", &strings);

  // Payload.
  const DictionaryValue* payload = NULL;
  if (promo->GetDictionary("payload", &payload)) {
    payload->GetBoolean("gplus_required", &gplus_required_);
    DVLOG(1) << "gplus_required_ = " << gplus_required_;

    base::Value* ppcopy = DeepCopyAndResolveStrings(payload, strings);
    DCHECK(ppcopy && ppcopy->IsType(base::Value::TYPE_DICTIONARY));
    promo_payload_.reset(static_cast<base::DictionaryValue*>(ppcopy));
  }

  if (!promo_payload_->GetString("promo_message_short", &promo_text_) &&
      strings) {
    // For compatibility with the legacy desktop version,
    // if no |payload.promo_message_short| is specified,
    // the first string in |strings| is used.
    DictionaryValue::Iterator iter(*strings);
    iter.value().GetAsString(&promo_text_);
  }
  DVLOG(1) << "promo_text_=" << promo_text_;

  promo->GetInteger("max_views", &max_views_);
  DVLOG(1) << "max_views_ " << max_views_;

  CheckForNewNotification();
}

void NotificationPromo::CheckForNewNotification() {
  NotificationPromo old_promo(profile_);
  old_promo.InitFromPrefs(promo_type_);
  const double old_start = old_promo.start_;
  const double old_end = old_promo.end_;
  const std::string old_promo_text = old_promo.promo_text_;

  new_notification_ =
      old_start != start_ || old_end != end_ || old_promo_text != promo_text_;
  if (new_notification_)
    OnNewNotification();
}

void NotificationPromo::OnNewNotification() {
  DVLOG(1) << "OnNewNotification";
  // Create a new promo group.
  group_ = base::RandInt(0, num_groups_ - 1);
  WritePrefs();
}

// static
void NotificationPromo::RegisterUserPrefs(PrefService* prefs) {
  ClearDeprecatedPrefs(prefs);
  prefs->RegisterDictionaryPref(kPrefPromoObject,
                                new base::DictionaryValue,
                                PrefService::UNSYNCABLE_PREF);
}

void NotificationPromo::WritePrefs() {
  DVLOG(1) << "WritePrefs";
  base::DictionaryValue* ntp_promo = new base::DictionaryValue;
  ntp_promo->SetString(kPrefPromoText, promo_text_);
  ntp_promo->Set(kPrefPromoPayload, promo_payload_->DeepCopy());
  ntp_promo->SetDouble(kPrefPromoStart, start_);
  ntp_promo->SetDouble(kPrefPromoEnd, end_);

  ntp_promo->SetInteger(kPrefPromoNumGroups, num_groups_);
  ntp_promo->SetInteger(kPrefPromoSegment, initial_segment_);
  ntp_promo->SetInteger(kPrefPromoIncrement, increment_);
  ntp_promo->SetInteger(kPrefPromoIncrementFrequency, time_slice_);
  ntp_promo->SetInteger(kPrefPromoIncrementMax, max_group_);

  ntp_promo->SetInteger(kPrefPromoMaxViews, max_views_);

  ntp_promo->SetInteger(kPrefPromoGroup, group_);
  ntp_promo->SetInteger(kPrefPromoViews, views_);
  ntp_promo->SetBoolean(kPrefPromoClosed, closed_);

  ntp_promo->SetBoolean(kPrefPromoGPlusRequired, gplus_required_);

  base::ListValue* promo_list = new base::ListValue;
  promo_list->Set(0, ntp_promo);  // Only support 1 promo for now.

  base::DictionaryValue promo_dict;
  promo_dict.Set(PromoTypeToString(promo_type_), promo_list);
  prefs_->Set(kPrefPromoObject, promo_dict);
}

void NotificationPromo::InitFromPrefs(PromoType promo_type) {
  promo_type_ = promo_type;
  const base::DictionaryValue* promo_dict =
      prefs_->GetDictionary(kPrefPromoObject);
  if (!promo_dict)
    return;

  const base::ListValue* promo_list = NULL;
  promo_dict->GetList(PromoTypeToString(promo_type_), &promo_list);
  if (!promo_list)
    return;

  const base::DictionaryValue* ntp_promo = NULL;
  promo_list->GetDictionary(0, &ntp_promo);
  if (!ntp_promo)
    return;

  ntp_promo->GetString(kPrefPromoText, &promo_text_);
  const base::DictionaryValue* promo_payload = NULL;
  if (ntp_promo->GetDictionary(kPrefPromoPayload, &promo_payload))
    promo_payload_.reset(promo_payload->DeepCopy());

  ntp_promo->GetDouble(kPrefPromoStart, &start_);
  ntp_promo->GetDouble(kPrefPromoEnd, &end_);

  ntp_promo->GetInteger(kPrefPromoNumGroups, &num_groups_);
  ntp_promo->GetInteger(kPrefPromoSegment, &initial_segment_);
  ntp_promo->GetInteger(kPrefPromoIncrement, &increment_);
  ntp_promo->GetInteger(kPrefPromoIncrementFrequency, &time_slice_);
  ntp_promo->GetInteger(kPrefPromoIncrementMax, &max_group_);

  ntp_promo->GetInteger(kPrefPromoMaxViews, &max_views_);

  ntp_promo->GetInteger(kPrefPromoGroup, &group_);
  ntp_promo->GetInteger(kPrefPromoViews, &views_);
  ntp_promo->GetBoolean(kPrefPromoClosed, &closed_);

  ntp_promo->GetBoolean(kPrefPromoGPlusRequired, &gplus_required_);
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

// static
void NotificationPromo::HandleClosed(Profile* profile, PromoType promo_type) {
  content::RecordAction(UserMetricsAction("NTPPromoClosed"));
  NotificationPromo promo(profile);
  promo.InitFromPrefs(promo_type);
  if (!promo.closed_) {
    promo.closed_ = true;
    promo.WritePrefs();
  }
}

// static
bool NotificationPromo::HandleViewed(Profile* profile, PromoType promo_type) {
  content::RecordAction(UserMetricsAction("NTPPromoShown"));
  NotificationPromo promo(profile);
  promo.InitFromPrefs(promo_type);
  ++promo.views_;
  promo.WritePrefs();
  return promo.ExceedsMaxViews();
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
