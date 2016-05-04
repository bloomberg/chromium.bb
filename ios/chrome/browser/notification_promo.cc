// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/notification_promo.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ios {

namespace {

const char kNTPPromoFinchExperiment[] = "IOSNTPPromotion";

// The name of the preference that stores the promotion object.
const char kPrefPromoObject[] = "ios.ntppromo";

// Keys in the kPrefPromoObject dictionary; used only here.
const char kPrefPromoText[] = "text";
const char kPrefPromoPayload[] = "payload";
const char kPrefPromoStart[] = "start";
const char kPrefPromoEnd[] = "end";
const char kPrefPromoID[] = "id";
const char kPrefPromoMaxViews[] = "max_views";
const char kPrefPromoMaxSeconds[] = "max_seconds";
const char kPrefPromoFirstViewTime[] = "first_view_time";
const char kPrefPromoViews[] = "views";
const char kPrefPromoClosed[] = "closed";

struct PromoMapEntry {
  NotificationPromo::PromoType promo_type;
  const char* promo_type_str;
};

const PromoMapEntry kPromoMap[] = {
    {NotificationPromo::NO_PROMO, ""},
    {NotificationPromo::NTP_NOTIFICATION_PROMO, "ntp_notification_promo"},
    {NotificationPromo::NTP_BUBBLE_PROMO, "ntp_bubble_promo"},
    {NotificationPromo::MOBILE_NTP_SYNC_PROMO, "mobile_ntp_sync_promo"},
    {NotificationPromo::MOBILE_NTP_WHATS_NEW_PROMO,
     "mobile_ntp_whats_new_promo"},
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

}  // namespace

NotificationPromo::NotificationPromo(PrefService* local_state)
    : local_state_(local_state),
      promo_type_(NO_PROMO),
      promo_payload_(new base::DictionaryValue()),
      start_(0.0),
      end_(0.0),
      promo_id_(-1),
      max_views_(0),
      max_seconds_(0),
      first_view_time_(0),
      views_(0),
      closed_(false),
      new_notification_(false) {
  DCHECK(local_state_);
}

NotificationPromo::~NotificationPromo() {}

void NotificationPromo::InitFromVariations() {
  std::map<std::string, std::string> params;
  if (!variations::GetVariationParams(kNTPPromoFinchExperiment, &params)) {
    // If there is no finch config, clear prefs to prevent promo from appearing.
    MigrateUserPrefs(local_state_);
    return;
  }

  // Build dictionary of parameters to pass to |InitFromJson|. Some parameters
  // are stored in a payload subdictionary, so two dictionaries are
  // built: one to represent the payload and one to represent all of the
  // paremeters. The payload is then added to the overall dictionary. This code
  // can be removed if this class is refactored and the payload is then
  // disregarded (crbug.com/608525).
  base::DictionaryValue json;
  base::DictionaryValue payload;
  std::map<std::string, std::string>::iterator iter;
  for (iter = params.begin(); iter != params.end(); ++iter) {
    int converted_number;
    bool converted = base::StringToInt(iter->second, &converted_number);
    // Choose the dictionary to which parameter is added based on whether the
    // parameter belongs in the payload or not.
    base::DictionaryValue& json_or_payload =
        IsPayloadParam(iter->first) ? payload : json;
    if (converted) {
      json_or_payload.SetInteger(iter->first, converted_number);
    } else {
      json_or_payload.SetString(iter->first, iter->second);
    }
  }
  json.Set("payload", payload.DeepCopy());

  InitFromJson(json, MOBILE_NTP_WHATS_NEW_PROMO);
}

void NotificationPromo::InitFromJson(const base::DictionaryValue& promo,
                                     PromoType promo_type) {
  promo_type_ = promo_type;

  std::string time_str;
  base::Time time;
  if (promo.GetString("start", &time_str) &&
      base::Time::FromString(time_str.c_str(), &time)) {
    start_ = time.ToDoubleT();
    DVLOG(1) << "start str=" << time_str
             << ", start_=" << base::DoubleToString(start_);
  }
  if (promo.GetString("end", &time_str) &&
      base::Time::FromString(time_str.c_str(), &time)) {
    end_ = time.ToDoubleT();
    DVLOG(1) << "end str =" << time_str
             << ", end_=" << base::DoubleToString(end_);
  }

  promo.GetString("promo_text", &promo_text_);
  DVLOG(1) << "promo_text_=" << promo_text_;

  const base::DictionaryValue* payload = NULL;
  if (promo.GetDictionary("payload", &payload)) {
    promo_payload_.reset(payload->DeepCopy());
  }

  promo.GetInteger("max_views", &max_views_);
  DVLOG(1) << "max_views_ " << max_views_;

  promo.GetInteger("max_seconds", &max_seconds_);
  DVLOG(1) << "max_seconds_ " << max_seconds_;

  promo.GetInteger("promo_id", &promo_id_);
  DVLOG(1) << "promo_id_ " << promo_id_;

  CheckForNewNotification();
}

void NotificationPromo::CheckForNewNotification() {
  NotificationPromo old_promo(local_state_);
  old_promo.InitFromPrefs(promo_type_);

  new_notification_ = old_promo.promo_id_ != promo_id_;
  if (new_notification_)
    OnNewNotification();
}

void NotificationPromo::OnNewNotification() {
  DVLOG(1) << "OnNewNotification";
  WritePrefs();
}

// static
void NotificationPromo::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPrefPromoObject);
}

// static
void NotificationPromo::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kPrefPromoObject);
}

// static
void NotificationPromo::MigrateUserPrefs(PrefService* user_prefs) {
  user_prefs->ClearPref(kPrefPromoObject);
}

void NotificationPromo::WritePrefs() {
  base::DictionaryValue* ntp_promo = new base::DictionaryValue;
  ntp_promo->SetString(kPrefPromoText, promo_text_);
  ntp_promo->Set(kPrefPromoPayload, promo_payload_->DeepCopy());
  ntp_promo->SetDouble(kPrefPromoStart, start_);
  ntp_promo->SetDouble(kPrefPromoEnd, end_);
  ntp_promo->SetInteger(kPrefPromoID, promo_id_);

  ntp_promo->SetInteger(kPrefPromoMaxViews, max_views_);
  ntp_promo->SetInteger(kPrefPromoMaxSeconds, max_seconds_);
  ntp_promo->SetDouble(kPrefPromoFirstViewTime, first_view_time_);

  ntp_promo->SetInteger(kPrefPromoViews, views_);
  ntp_promo->SetBoolean(kPrefPromoClosed, closed_);

  base::ListValue* promo_list = new base::ListValue;
  promo_list->Set(0, ntp_promo);  // Only support 1 promo for now.

  base::DictionaryValue promo_dict;
  promo_dict.MergeDictionary(local_state_->GetDictionary(kPrefPromoObject));
  promo_dict.Set(PromoTypeToString(promo_type_), promo_list);
  local_state_->Set(kPrefPromoObject, promo_dict);
  DVLOG(1) << "WritePrefs " << promo_dict;
}

void NotificationPromo::InitFromPrefs(PromoType promo_type) {
  promo_type_ = promo_type;
  const base::DictionaryValue* promo_dict =
      local_state_->GetDictionary(kPrefPromoObject);
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
  ntp_promo->GetInteger(kPrefPromoID, &promo_id_);

  ntp_promo->GetInteger(kPrefPromoMaxViews, &max_views_);
  ntp_promo->GetInteger(kPrefPromoMaxSeconds, &max_seconds_);
  ntp_promo->GetDouble(kPrefPromoFirstViewTime, &first_view_time_);

  ntp_promo->GetInteger(kPrefPromoViews, &views_);
  ntp_promo->GetBoolean(kPrefPromoClosed, &closed_);
}

bool NotificationPromo::CanShow() const {
  return !closed_ && !promo_text_.empty() && !ExceedsMaxViews() &&
         !ExceedsMaxSeconds() &&
         base::Time::FromDoubleT(StartTime()) < base::Time::Now() &&
         base::Time::FromDoubleT(EndTime()) > base::Time::Now();
}

// static
void NotificationPromo::HandleClosed(PromoType promo_type,
                                     PrefService* local_state) {
  NotificationPromo promo(local_state);
  promo.InitFromPrefs(promo_type);
  if (!promo.closed_) {
    promo.closed_ = true;
    promo.WritePrefs();
  }
}

// static
bool NotificationPromo::HandleViewed(PromoType promo_type,
                                     PrefService* local_state) {
  NotificationPromo promo(local_state);
  promo.InitFromPrefs(promo_type);
  ++promo.views_;
  if (promo.first_view_time_ == 0) {
    promo.first_view_time_ = base::Time::Now().ToDoubleT();
  }
  promo.WritePrefs();
  return promo.ExceedsMaxViews() || promo.ExceedsMaxSeconds();
}

bool NotificationPromo::ExceedsMaxViews() const {
  return (max_views_ == 0) ? false : views_ >= max_views_;
}

bool NotificationPromo::ExceedsMaxSeconds() const {
  if (max_seconds_ == 0 || first_view_time_ == 0)
    return false;

  const base::Time last_view_time = base::Time::FromDoubleT(first_view_time_) +
                                    base::TimeDelta::FromSeconds(max_seconds_);
  return last_view_time < base::Time::Now();
}

bool NotificationPromo::IsPayloadParam(const std::string& param_name) const {
  return param_name != "start" && param_name != "end" &&
         param_name != "promo_text" && param_name != "max_views" &&
         param_name != "max_seconds" && param_name != "promo_id";
}

double NotificationPromo::StartTime() const {
  return start_;
}

double NotificationPromo::EndTime() const {
  return end_;
}

}  // namespace ios
