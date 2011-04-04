// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/promo_resource_service.h"

#include "base/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace {

// Delay on first fetch so we don't interfere with startup.
static const int kStartResourceFetchDelay = 5000;

// Delay between calls to update the cache (48 hours).
static const int kCacheUpdateDelay = 48 * 60 * 60 * 1000;

// Users are randomly assigned to one of kNTPPromoGroupSize buckets, in order
// to be able to roll out promos slowly, or display different promos to
// different groups.
static const int kNTPPromoGroupSize = 16;

// Maximum number of hours for each time slice (4 weeks).
static const int kMaxTimeSliceHours = 24 * 7 * 4;

// Used to determine which build type should be shown a given promo.
enum BuildType {
  NO_BUILD = 0,
  DEV_BUILD = 1,
  BETA_BUILD = 1 << 1,
  STABLE_BUILD = 1 << 2,
};

}  // namespace

namespace web_resource {

bool CanShowPromo(Profile* profile) {
  bool promo_closed = false;
  PrefService* prefs = profile->GetPrefs();
  if (prefs->HasPrefPath(prefs::kNTPPromoClosed))
    promo_closed = prefs->GetBoolean(prefs::kNTPPromoClosed);

  // Only show if not synced.
  bool is_synced =
      (profile->HasProfileSyncService() &&
          sync_ui_util::GetStatus(
              profile->GetProfileSyncService()) == sync_ui_util::SYNCED);

  // GetVersionStringModifier hits the registry. See http://crbug.com/70898.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  const std::string channel = platform_util::GetVersionStringModifier();
  bool is_promo_build = false;
  PrefService* local_state = g_browser_process->local_state();
  if (local_state->HasPrefPath(prefs::kNTPPromoBuild)) {
    int builds_allowed = local_state->GetInteger(prefs::kNTPPromoBuild);
    if (builds_allowed == NO_BUILD)
      return false;
    if (channel == "dev" || channel == "dev-m") {
      is_promo_build = (DEV_BUILD & builds_allowed) != 0;
    } else if (channel == "beta" || channel == "beta-m") {
      is_promo_build = (BETA_BUILD & builds_allowed) != 0;
    } else if (channel == "" || channel == "m") {
      is_promo_build = (STABLE_BUILD & builds_allowed) != 0;
    } else {
      is_promo_build = false;
    }
  }

  return !promo_closed && !is_synced && is_promo_build;
}

}  // namespace web_resource

// Server for dynamically loaded NTP HTML elements. TODO(mirandac): append
// locale for future usage, when we're serving localizable strings.
const char* PromoResourceService::kDefaultPromoResourceServer =
    "https://www.google.com/support/chrome/bin/topic/1142433/inproduct?hl=";

PromoResourceService::PromoResourceService()
    : WebResourceService(PromoResourceService::kDefaultPromoResourceServer,
                         true,  // append locale to URL
                         NotificationType::PROMO_RESOURCE_STATE_CHANGED,
                         prefs::kNTPPromoResourceCacheUpdate,
                         kStartResourceFetchDelay,
                         kCacheUpdateDelay),
      web_resource_cache_(NULL) {
  Init();
}

PromoResourceService::~PromoResourceService() { }

void PromoResourceService::Init() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->RegisterDoublePref(prefs::kNTPCustomLogoStart, 0);
  local_state->RegisterDoublePref(prefs::kNTPCustomLogoEnd, 0);
  local_state->RegisterDoublePref(prefs::kNTPPromoStart, 0);
  local_state->RegisterDoublePref(prefs::kNTPPromoEnd, 0);
  local_state->RegisterStringPref(prefs::kNTPPromoLine, std::string());
  local_state->RegisterIntegerPref(prefs::kNTPPromoBuild, NO_BUILD);

  // If the promo start is in the future, set a notification task to
  // invalidate the NTP cache at the time of the promo start.
  double promo_start = local_state->GetDouble(prefs::kNTPPromoStart);
  double promo_end = local_state->GetDouble(prefs::kNTPPromoEnd);
  ScheduleNotification(promo_start, promo_end);
}

void PromoResourceService::Unpack(const DictionaryValue& parsed_json) {
  UnpackLogoSignal(parsed_json);
  UnpackPromoSignal(parsed_json);
}

void PromoResourceService::ScheduleNotification(double promo_start,
                                                double promo_end) {
  if (promo_start > 0 && promo_end > 0) {
    int64 ms_until_start =
        static_cast<int64>((base::Time::FromDoubleT(
            promo_start) - base::Time::Now()).InMilliseconds());
    int64 ms_until_end =
        static_cast<int64>((base::Time::FromDoubleT(
            promo_end) - base::Time::Now()).InMilliseconds());
    if (ms_until_start > 0)
      PostNotification(ms_until_start);
    if (ms_until_end > 0) {
      PostNotification(ms_until_end);
      if (ms_until_start <= 0) {
        // Notify immediately if time is between start and end.
        PostNotification(0);
      }
    }
  }
}

void PromoResourceService::UnpackPromoSignal(
    const DictionaryValue& parsed_json) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryValue* topic_dict;
  ListValue* answer_list;
  double old_promo_start = 0;
  double old_promo_end = 0;
  double promo_start = 0;
  double promo_end = 0;

  // Check for preexisting start and end values.
  if (local_state->HasPrefPath(prefs::kNTPPromoStart) &&
      local_state->HasPrefPath(prefs::kNTPPromoEnd)) {
    old_promo_start = local_state->GetDouble(prefs::kNTPPromoStart);
    old_promo_end = local_state->GetDouble(prefs::kNTPPromoEnd);
  }

  // Check for newly received start and end values.
  if (parsed_json.GetDictionary("topic", &topic_dict)) {
    if (topic_dict->GetList("answers", &answer_list)) {
      std::string promo_start_string = "";
      std::string promo_end_string = "";
      std::string promo_string = "";
      std::string promo_build = "";
      int promo_build_type = 0;
      int time_slice_hrs = 0;
      for (ListValue::const_iterator tip_iter = answer_list->begin();
           tip_iter != answer_list->end(); ++tip_iter) {
        if (!(*tip_iter)->IsType(Value::TYPE_DICTIONARY))
          continue;
        DictionaryValue* a_dic =
            static_cast<DictionaryValue*>(*tip_iter);
        std::string promo_signal;
        if (a_dic->GetString("name", &promo_signal)) {
          if (promo_signal == "promo_start") {
            a_dic->GetString("question", &promo_build);
            size_t split = promo_build.find(":");
            if (split != std::string::npos &&
                base::StringToInt(promo_build.substr(0, split),
                                  &promo_build_type) &&
                base::StringToInt(promo_build.substr(split+1),
                                  &time_slice_hrs) &&
                promo_build_type >= 0 &&
                promo_build_type <= (DEV_BUILD | BETA_BUILD | STABLE_BUILD) &&
                time_slice_hrs >= 0 &&
                time_slice_hrs <= kMaxTimeSliceHours) {
              local_state->SetInteger(prefs::kNTPPromoBuild, promo_build_type);
            } else {
              // If no time data or bad time data are set, do not show promo.
              local_state->SetInteger(prefs::kNTPPromoBuild, NO_BUILD);
            }
            a_dic->GetString("inproduct", &promo_start_string);
            a_dic->GetString("tooltip", &promo_string);
            local_state->SetString(prefs::kNTPPromoLine, promo_string);
          } else if (promo_signal == "promo_end") {
            a_dic->GetString("inproduct", &promo_end_string);
          }
        }
      }
      if (!promo_start_string.empty() &&
          promo_start_string.length() > 0 &&
          !promo_end_string.empty() &&
          promo_end_string.length() > 0) {
        base::Time start_time;
        base::Time end_time;
        if (base::Time::FromString(
                ASCIIToWide(promo_start_string).c_str(), &start_time) &&
            base::Time::FromString(
                ASCIIToWide(promo_end_string).c_str(), &end_time)) {
          // Add group time slice, adjusted from hours to seconds.
          srand(static_cast<uint32>(time(NULL)));
          promo_group_ = rand() % kNTPPromoGroupSize;
          promo_start = start_time.ToDoubleT() +
              promo_group_ * time_slice_hrs * 60 * 60;
          promo_end = end_time.ToDoubleT();
        }
      }
    }
  }

  // If start or end times have changed, trigger a new web resource
  // notification, so that the logo on the NTP is updated. This check is
  // outside the reading of the web resource data, because the absence of
  // dates counts as a triggering change if there were dates before.
  // Also reset the promo closed preference, to signal a new promo.
  if (!(old_promo_start == promo_start) ||
      !(old_promo_end == promo_end)) {
    local_state->SetDouble(prefs::kNTPPromoStart, promo_start);
    local_state->SetDouble(prefs::kNTPPromoEnd, promo_end);
    ScheduleNotification(promo_start, promo_end);
  }
}

void PromoResourceService::UnpackLogoSignal(
    const DictionaryValue& parsed_json) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryValue* topic_dict;
  ListValue* answer_list;
  double old_logo_start = 0;
  double old_logo_end = 0;
  double logo_start = 0;
  double logo_end = 0;

  // Check for preexisting start and end values.
  if (local_state->HasPrefPath(prefs::kNTPCustomLogoStart) &&
      local_state->HasPrefPath(prefs::kNTPCustomLogoEnd)) {
    old_logo_start = local_state->GetDouble(prefs::kNTPCustomLogoStart);
    old_logo_end = local_state->GetDouble(prefs::kNTPCustomLogoEnd);
  }

  // Check for newly received start and end values.
  if (parsed_json.GetDictionary("topic", &topic_dict)) {
    if (topic_dict->GetList("answers", &answer_list)) {
      std::string logo_start_string = "";
      std::string logo_end_string = "";
      for (ListValue::const_iterator tip_iter = answer_list->begin();
           tip_iter != answer_list->end(); ++tip_iter) {
        if (!(*tip_iter)->IsType(Value::TYPE_DICTIONARY))
          continue;
        DictionaryValue* a_dic =
            static_cast<DictionaryValue*>(*tip_iter);
        std::string logo_signal;
        if (a_dic->GetString("name", &logo_signal)) {
          if (logo_signal == "custom_logo_start") {
            a_dic->GetString("inproduct", &logo_start_string);
          } else if (logo_signal == "custom_logo_end") {
            a_dic->GetString("inproduct", &logo_end_string);
          }
        }
      }
      if (!logo_start_string.empty() &&
          logo_start_string.length() > 0 &&
          !logo_end_string.empty() &&
          logo_end_string.length() > 0) {
        base::Time start_time;
        base::Time end_time;
        if (base::Time::FromString(
                ASCIIToWide(logo_start_string).c_str(), &start_time) &&
            base::Time::FromString(
                ASCIIToWide(logo_end_string).c_str(), &end_time)) {
          logo_start = start_time.ToDoubleT();
          logo_end = end_time.ToDoubleT();
        }
      }
    }
  }

  // If logo start or end times have changed, trigger a new web resource
  // notification, so that the logo on the NTP is updated. This check is
  // outside the reading of the web resource data, because the absence of
  // dates counts as a triggering change if there were dates before.
  if (!(old_logo_start == logo_start) ||
      !(old_logo_end == logo_end)) {
    local_state->SetDouble(prefs::kNTPCustomLogoStart, logo_start);
    local_state->SetDouble(prefs::kNTPCustomLogoEnd, logo_end);
    NotificationService* service = NotificationService::current();
    service->Notify(NotificationType::PROMO_RESOURCE_STATE_CHANGED,
                    Source<WebResourceService>(this),
                    NotificationService::NoDetails());
  }
}

