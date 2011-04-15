// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/promo_resource_service.h"

#include "base/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/apps_promo.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "googleurl/src/gurl.h"

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

// The version of the service (used to expire the cache when upgrading Chrome
// to versions with different types of promos).
static const int kPromoServiceVersion = 1;

// Properties used by the server.
static const char kAnswerIdProperty[] = "answer_id";
static const char kWebStoreHeaderProperty[] = "question";
static const char kWebStoreButtonProperty[] = "inproduct_target";
static const char kWebStoreLinkProperty[] = "inproduct";
static const char kWebStoreExpireProperty[] = "tooltip";

}  // namespace

// Server for dynamically loaded NTP HTML elements. TODO(mirandac): append
// locale for future usage, when we're serving localizable strings.
const char* PromoResourceService::kDefaultPromoResourceServer =
    "https://www.google.com/support/chrome/bin/topic/1142433/inproduct?hl=";

// static
void PromoResourceService::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterIntegerPref(prefs::kNTPPromoVersion, 0);
  local_state->RegisterStringPref(prefs::kNTPPromoLocale, std::string());
}

// static
void PromoResourceService::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDoublePref(prefs::kNTPCustomLogoStart, 0);
  prefs->RegisterDoublePref(prefs::kNTPCustomLogoEnd, 0);
  prefs->RegisterDoublePref(prefs::kNTPPromoStart, 0);
  prefs->RegisterDoublePref(prefs::kNTPPromoEnd, 0);
  prefs->RegisterStringPref(prefs::kNTPPromoLine, std::string());
  prefs->RegisterBooleanPref(prefs::kNTPPromoClosed, false);
  prefs->RegisterIntegerPref(prefs::kNTPPromoGroup, -1);
  prefs->RegisterIntegerPref(prefs::kNTPPromoBuild,
       CANARY_BUILD | DEV_BUILD | BETA_BUILD | STABLE_BUILD);
  prefs->RegisterIntegerPref(prefs::kNTPPromoGroupTimeSlice, 0);
}

// static
bool PromoResourceService::IsBuildTargeted(const std::string& channel,
                                           int builds_allowed) {
  if (builds_allowed == NO_BUILD)
    return false;
  if (channel == "canary" || channel == "canary-m") {
    return (CANARY_BUILD & builds_allowed) != 0;
  } else if (channel == "dev" || channel == "dev-m") {
    return (DEV_BUILD & builds_allowed) != 0;
  } else if (channel == "beta" || channel == "beta-m") {
    return (BETA_BUILD & builds_allowed) != 0;
  } else if (channel == "" || channel == "m") {
    return (STABLE_BUILD & builds_allowed) != 0;
  } else {
    return false;
  }
}

PromoResourceService::PromoResourceService(Profile* profile)
    : WebResourceService(profile,
                         profile->GetPrefs(),
                         PromoResourceService::kDefaultPromoResourceServer,
                         true,  // append locale to URL
                         NotificationType::PROMO_RESOURCE_STATE_CHANGED,
                         prefs::kNTPPromoResourceCacheUpdate,
                         kStartResourceFetchDelay,
                         kCacheUpdateDelay),
      web_resource_cache_(NULL),
      channel_(NULL) {
  Init();
}

PromoResourceService::~PromoResourceService() { }

void PromoResourceService::Init() {
  ScheduleNotificationOnInit();
}

bool PromoResourceService::IsThisBuildTargeted(int builds_targeted) {
  if (channel_ == NULL) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    channel_ = platform_util::GetVersionStringModifier().c_str();
  }

  return IsBuildTargeted(channel_, builds_targeted);
}

void PromoResourceService::Unpack(const DictionaryValue& parsed_json) {
  UnpackLogoSignal(parsed_json);
  UnpackPromoSignal(parsed_json);
  UnpackWebStoreSignal(parsed_json);
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

void PromoResourceService::ScheduleNotificationOnInit() {
  std::string locale = g_browser_process->GetApplicationLocale();
  if ((GetPromoServiceVersion() != kPromoServiceVersion) ||
      (GetPromoLocale() != locale)) {
    // If the promo service has been upgraded or Chrome switched locales,
    // refresh the promos.
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetInteger(prefs::kNTPPromoVersion, kPromoServiceVersion);
    local_state->SetString(prefs::kNTPPromoLocale, locale);
    prefs_->ClearPref(prefs::kNTPPromoResourceCacheUpdate);
    AppsPromo::ClearPromo();
    PostNotification(0);
  } else {
    // If the promo start is in the future, set a notification task to
    // invalidate the NTP cache at the time of the promo start.
    double promo_start = prefs_->GetDouble(prefs::kNTPPromoStart);
    double promo_end = prefs_->GetDouble(prefs::kNTPPromoEnd);
    ScheduleNotification(promo_start, promo_end);
  }
}

int PromoResourceService::GetPromoServiceVersion() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetInteger(prefs::kNTPPromoVersion);
}

std::string PromoResourceService::GetPromoLocale() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetString(prefs::kNTPPromoLocale);
}

void PromoResourceService::UnpackPromoSignal(
    const DictionaryValue& parsed_json) {
  DictionaryValue* topic_dict;
  ListValue* answer_list;
  double old_promo_start = 0;
  double old_promo_end = 0;
  double promo_start = 0;
  double promo_end = 0;

  // Check for preexisting start and end values.
  if (prefs_->HasPrefPath(prefs::kNTPPromoStart) &&
      prefs_->HasPrefPath(prefs::kNTPPromoEnd)) {
    old_promo_start = prefs_->GetDouble(prefs::kNTPPromoStart);
    old_promo_end = prefs_->GetDouble(prefs::kNTPPromoEnd);
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
      for (ListValue::const_iterator answer_iter = answer_list->begin();
           answer_iter != answer_list->end(); ++answer_iter) {
        if (!(*answer_iter)->IsType(Value::TYPE_DICTIONARY))
          continue;
        DictionaryValue* a_dic =
            static_cast<DictionaryValue*>(*answer_iter);
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
              prefs_->SetInteger(prefs::kNTPPromoBuild, promo_build_type);
              prefs_->SetInteger(prefs::kNTPPromoGroupTimeSlice,
                                 time_slice_hrs);
            } else {
              // If no time data or bad time data are set, do not show promo.
              prefs_->SetInteger(prefs::kNTPPromoBuild, NO_BUILD);
              prefs_->SetInteger(prefs::kNTPPromoGroupTimeSlice, 0);
            }
            a_dic->GetString("inproduct", &promo_start_string);
            a_dic->GetString("tooltip", &promo_string);
            prefs_->SetString(prefs::kNTPPromoLine, promo_string);
            srand(static_cast<uint32>(time(NULL)));
            prefs_->SetInteger(prefs::kNTPPromoGroup,
                               rand() % kNTPPromoGroupSize);
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
          promo_start = start_time.ToDoubleT() +
              (prefs_->FindPreference(prefs::kNTPPromoGroup) ?
                  prefs_->GetInteger(prefs::kNTPPromoGroup) *
                      time_slice_hrs * 60 * 60 : 0);
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
    prefs_->SetDouble(prefs::kNTPPromoStart, promo_start);
    prefs_->SetDouble(prefs::kNTPPromoEnd, promo_end);
    prefs_->SetBoolean(prefs::kNTPPromoClosed, false);
    ScheduleNotification(promo_start, promo_end);
  }
}

void PromoResourceService::UnpackWebStoreSignal(
    const DictionaryValue& parsed_json) {
  DictionaryValue* topic_dict;
  ListValue* answer_list;

  bool signal_found = false;
  std::string promo_id = "";
  std::string promo_header = "";
  std::string promo_button = "";
  std::string promo_link = "";
  std::string promo_expire = "";
  int target_builds = 0;

  if (!parsed_json.GetDictionary("topic", &topic_dict) ||
      !topic_dict->GetList("answers", &answer_list))
    return;

  for (ListValue::const_iterator answer_iter = answer_list->begin();
       answer_iter != answer_list->end(); ++answer_iter) {
    if (!(*answer_iter)->IsType(Value::TYPE_DICTIONARY))
      continue;
    DictionaryValue* a_dic =
        static_cast<DictionaryValue*>(*answer_iter);
    std::string name;
    if (!a_dic->GetString("name", &name))
      continue;

    size_t split = name.find(":");
    if (split == std::string::npos)
      continue;

    std::string promo_signal = name.substr(0, split);

    if (promo_signal != "webstore_promo" ||
        !base::StringToInt(name.substr(split+1), &target_builds))
      continue;

    if (!a_dic->GetString(kAnswerIdProperty, &promo_id) ||
        !a_dic->GetString(kWebStoreHeaderProperty, &promo_header) ||
        !a_dic->GetString(kWebStoreButtonProperty, &promo_button) ||
        !a_dic->GetString(kWebStoreLinkProperty, &promo_link) ||
        !a_dic->GetString(kWebStoreExpireProperty, &promo_expire))
      continue;

    if (IsThisBuildTargeted(target_builds)) {
      // Store the first web store promo that targets the current build.
      AppsPromo::SetPromo(
          promo_id, promo_header, promo_button, GURL(promo_link), promo_expire);
      signal_found = true;
      break;
    }
  }

  if (!signal_found) {
    // If no web store promos target this build, then clear all the prefs.
    AppsPromo::ClearPromo();
  }

  NotificationService::current()->Notify(
      NotificationType::WEB_STORE_PROMO_LOADED,
      Source<PromoResourceService>(this),
      NotificationService::NoDetails());

  return;
}

void PromoResourceService::UnpackLogoSignal(
    const DictionaryValue& parsed_json) {
  DictionaryValue* topic_dict;
  ListValue* answer_list;
  double old_logo_start = 0;
  double old_logo_end = 0;
  double logo_start = 0;
  double logo_end = 0;

  // Check for preexisting start and end values.
  if (prefs_->HasPrefPath(prefs::kNTPCustomLogoStart) &&
      prefs_->HasPrefPath(prefs::kNTPCustomLogoEnd)) {
    old_logo_start = prefs_->GetDouble(prefs::kNTPCustomLogoStart);
    old_logo_end = prefs_->GetDouble(prefs::kNTPCustomLogoEnd);
  }

  // Check for newly received start and end values.
  if (parsed_json.GetDictionary("topic", &topic_dict)) {
    if (topic_dict->GetList("answers", &answer_list)) {
      std::string logo_start_string = "";
      std::string logo_end_string = "";
      for (ListValue::const_iterator answer_iter = answer_list->begin();
           answer_iter != answer_list->end(); ++answer_iter) {
        if (!(*answer_iter)->IsType(Value::TYPE_DICTIONARY))
          continue;
        DictionaryValue* a_dic =
            static_cast<DictionaryValue*>(*answer_iter);
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
    prefs_->SetDouble(prefs::kNTPCustomLogoStart, logo_start);
    prefs_->SetDouble(prefs::kNTPCustomLogoEnd, logo_end);
    NotificationService* service = NotificationService::current();
    service->Notify(NotificationType::PROMO_RESOURCE_STATE_CHANGED,
                    Source<WebResourceService>(this),
                    NotificationService::NoDetails());
  }
}

namespace PromoResourceServiceUtil {

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

  bool is_promo_build = false;
  if (prefs->HasPrefPath(prefs::kNTPPromoBuild)) {
    // GetVersionStringModifier hits the registry. See http://crbug.com/70898.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    const std::string channel = platform_util::GetVersionStringModifier();
    is_promo_build = PromoResourceService::IsBuildTargeted(
        channel, prefs->GetInteger(prefs::kNTPPromoBuild));
  }

  return !promo_closed && !is_synced && is_promo_build;
}

}  // namespace PromoResourceServiceUtil
