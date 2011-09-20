// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/promo_resource_service.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/apps_promo.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"

namespace {

// Delay on first fetch so we don't interfere with startup.
static const int kStartResourceFetchDelay = 5000;

// Delay between calls to update the cache (48 hours).
static const int kCacheUpdateDelay = 48 * 60 * 60 * 1000;

// Users are randomly assigned to one of kNTPPromoGroupSize buckets, in order
// to be able to roll out promos slowly, or display different promos to
// different groups.
static const int kNTPPromoGroupSize = 100;

// Maximum number of hours for each time slice (4 weeks).
static const int kMaxTimeSliceHours = 24 * 7 * 4;

// The version of the service (used to expire the cache when upgrading Chrome
// to versions with different types of promos).
static const int kPromoServiceVersion = 2;

// Properties used by the server.
static const char kAnswerIdProperty[] = "answer_id";
static const char kWebStoreHeaderProperty[] = "question";
static const char kWebStoreButtonProperty[] = "inproduct_target";
static const char kWebStoreLinkProperty[] = "inproduct";
static const char kWebStoreExpireProperty[] = "tooltip";

chrome::VersionInfo::Channel GetChannel() {
  // GetChannel hits the registry on Windows. See http://crbug.com/70898.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  return chrome::VersionInfo::GetChannel();
}

int GetNextQuestionValue(const std::string& question,
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
  return value;
}

const char* GetPromoResourceURL() {
  std::string promo_server_url = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kPromoServerURL);
  return promo_server_url.empty() ?
      PromoResourceService::kDefaultPromoResourceServer :
      promo_server_url.c_str();
}

}  // namespace

// Server for dynamically loaded NTP HTML elements.
const char* PromoResourceService::kDefaultPromoResourceServer =
    "https://www.google.com/support/chrome/bin/topic/1142433/inproduct?hl=";

// static
void PromoResourceService::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterIntegerPref(prefs::kNTPPromoVersion, 0);
  local_state->RegisterStringPref(prefs::kNTPPromoLocale, std::string());
}

// static
void PromoResourceService::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDoublePref(prefs::kNTPCustomLogoStart,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kNTPCustomLogoEnd,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kNTPPromoStart,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kNTPPromoEnd,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kNTPPromoLine,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kNTPPromoClosed,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoGroup,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(
      prefs::kNTPPromoBuild,
      ALL_BUILDS,
      PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoGroupTimeSlice,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kNTPPromoGroupMax,
                             0,
                             PrefService::UNSYNCABLE_PREF);
}

// static
bool PromoResourceService::IsBuildTargeted(chrome::VersionInfo::Channel channel,
                                           int builds_allowed) {
  if (builds_allowed == NO_BUILD)
    return false;
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_CANARY:
      return (CANARY_BUILD & builds_allowed) != 0;
    case chrome::VersionInfo::CHANNEL_DEV:
      return (DEV_BUILD & builds_allowed) != 0;
    case chrome::VersionInfo::CHANNEL_BETA:
      return (BETA_BUILD & builds_allowed) != 0;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return (STABLE_BUILD & builds_allowed) != 0;
    default:
      // Show promos for local builds when using a custom promo URL.
      return CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPromoServerURL);
  }
}

PromoResourceService::PromoResourceService(Profile* profile)
    : WebResourceService(profile->GetPrefs(),
                         GetPromoResourceURL(),
                         true,  // append locale to URL
                         chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                         prefs::kNTPPromoResourceCacheUpdate,
                         kStartResourceFetchDelay,
                         kCacheUpdateDelay),
                         profile_(profile),
                         channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
  Init();
}

PromoResourceService::~PromoResourceService() { }

void PromoResourceService::Init() {
  ScheduleNotificationOnInit();
}

bool PromoResourceService::IsThisBuildTargeted(int builds_targeted) {
  if (channel_ == chrome::VersionInfo::CHANNEL_UNKNOWN)
    channel_ = GetChannel();

  return IsBuildTargeted(channel_, builds_targeted);
}

void PromoResourceService::Unpack(const DictionaryValue& parsed_json) {
  UnpackLogoSignal(parsed_json);
  UnpackNotificationSignal(parsed_json);
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

void PromoResourceService::UnpackNotificationSignal(
    const DictionaryValue& parsed_json) {
  // Check for newly received start and end values.
  std::string promo_start_string, promo_end_string;

  DictionaryValue* topic_dict;
  if (parsed_json.GetDictionary("topic", &topic_dict)) {
    ListValue* answer_list;
    if (topic_dict->GetList("answers", &answer_list)) {
      for (ListValue::const_iterator answer_iter = answer_list->begin();
           answer_iter != answer_list->end(); ++answer_iter) {
        if (!(*answer_iter)->IsType(Value::TYPE_DICTIONARY))
          continue;

        ParseNotification(static_cast<DictionaryValue*>(*answer_iter),
            &promo_start_string, &promo_end_string);
      }
    }
  }

  CheckForNewNotification(promo_start_string, promo_end_string);
}

void PromoResourceService::ParseNotification(
    DictionaryValue* a_dic,
    std::string* promo_start_string,
    std::string* promo_end_string) {
  std::string promo_signal;
  if (a_dic->GetString("name", &promo_signal)) {
    if (promo_signal == "promo_start") {
      SetNotificationParams(a_dic);
      SetNotificationLine(a_dic);

      a_dic->GetString("inproduct", promo_start_string);
    } else if (promo_signal == "promo_end") {
      a_dic->GetString("inproduct", promo_end_string);
    }
  }
}

void PromoResourceService::SetNotificationParams(DictionaryValue* a_dic) {
  std::string question;
  a_dic->GetString("question", &question);

  size_t index(0);
  bool err(false);
  int promo_build = GetNextQuestionValue(question, &index, &err);
  int time_slice = GetNextQuestionValue(question, &index, &err);
  int max_group = GetNextQuestionValue(question, &index, &err);

  if (err ||
      promo_build < 0 ||
      promo_build > ALL_BUILDS ||
      time_slice < 0 ||
      time_slice > kMaxTimeSliceHours ||
      max_group < 0 ||
      max_group >= kNTPPromoGroupSize) {
    // If values are not valid, do not show promo.
    NOTREACHED() << "Invalid server data, question=" << question <<
        ", build=" << promo_build <<
        ", time_slice=" << time_slice <<
        ", max_group=" << max_group;
    promo_build = NO_BUILD;
    time_slice = 0;
    max_group = 0;
  }

  prefs_->SetInteger(prefs::kNTPPromoBuild, promo_build);
  prefs_->SetInteger(prefs::kNTPPromoGroupTimeSlice, time_slice);
  prefs_->SetInteger(prefs::kNTPPromoGroupMax, max_group);
}

void PromoResourceService::SetNotificationLine(DictionaryValue* a_dic) {
  std::string promo_line;
  a_dic->GetString("tooltip", &promo_line);
  if (!promo_line.empty())
    prefs_->SetString(prefs::kNTPPromoLine, promo_line);
}

void PromoResourceService::CheckForNewNotification(
    const std::string& promo_start_string,
    const std::string& promo_end_string) {
  double promo_start = 0.0, promo_end = 0.0;
  ParseNewNotificationTimes(promo_start_string, promo_end_string,
      &promo_start, &promo_end);

  double old_promo_start = 0.0, old_promo_end = 0.0;
  GetCurrentNotificationTimes(&old_promo_start, &old_promo_end);

  // If start or end times have changed, trigger a new web resource
  // notification, so that the logo on the NTP is updated. This check is
  // outside the reading of the web resource data, because the absence of
  // dates counts as a triggering change if there were dates before.
  // Also create new promo groups, and reset the promo closed preference,
  // to signal a new promo.
  if (old_promo_start != promo_start || old_promo_end != promo_end)
    OnNewNotification(promo_start, promo_end);
}

void PromoResourceService::ParseNewNotificationTimes(
    const std::string& promo_start_string,
    const std::string& promo_end_string,
    double* promo_start,
    double* promo_end) {
  *promo_start = *promo_end = 0.0;

  if (promo_start_string.empty() && !promo_end_string.empty())
    return;

  base::Time start_time, end_time;
  if (!base::Time::FromString(promo_start_string.c_str(), &start_time) ||
      !base::Time::FromString(promo_end_string.c_str(), &end_time))
    return;

  *promo_start = start_time.ToDoubleT();
  *promo_end = end_time.ToDoubleT();
}

void PromoResourceService::GetCurrentNotificationTimes(double* old_promo_start,
                                                       double* old_promo_end) {
  *old_promo_start = *old_promo_end = 0.0;
  if (prefs_->HasPrefPath(prefs::kNTPPromoStart) &&
      prefs_->HasPrefPath(prefs::kNTPPromoEnd)) {
    *old_promo_start = prefs_->GetDouble(prefs::kNTPPromoStart);
    *old_promo_end = prefs_->GetDouble(prefs::kNTPPromoEnd);
  }
}

int PromoResourceService::ResetNotificationGroup() {
  srand(static_cast<uint32>(time(NULL)));
  const int promo_group = rand() % kNTPPromoGroupSize;
  prefs_->SetInteger(prefs::kNTPPromoGroup, promo_group);
  return promo_group;
}

// static
double PromoResourceService::GetNotificationStartTime(PrefService* prefs) {
  if (!prefs->HasPrefPath(prefs::kNTPPromoStart))
    return 0.0;

  const double promo_start = prefs->GetDouble(prefs::kNTPPromoStart);

  if (!prefs->HasPrefPath(prefs::kNTPPromoGroup) ||
      !prefs->HasPrefPath(prefs::kNTPPromoGroupTimeSlice))
    return promo_start;

  const int promo_group = prefs->GetInteger(prefs::kNTPPromoGroup);
  const int time_slice = prefs->GetInteger(prefs::kNTPPromoGroupTimeSlice);
  // Adjust promo_start using group time slice, adjusted from hours to seconds.
  static const double kSecondsInHour = 60.0 * 60.0;
  return promo_start + promo_group * time_slice * kSecondsInHour;
}

void PromoResourceService::OnNewNotification(double promo_start,
                                             double promo_end) {
  ResetNotificationGroup();

  prefs_->SetBoolean(prefs::kNTPPromoClosed, false);

  prefs_->SetDouble(prefs::kNTPPromoStart, promo_start);
  prefs_->SetDouble(prefs::kNTPPromoEnd, promo_end);

  ScheduleNotification(GetNotificationStartTime(prefs_), promo_end);
}

bool PromoResourceService::CanShowNotificationPromo(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();

  // Check if promo has been closed by the user.
  if (prefs->HasPrefPath(prefs::kNTPPromoClosed) &&
      prefs->GetBoolean(prefs::kNTPPromoClosed))
    return false;

  // Check if our build is appropriate for this promo.
  if (!prefs->HasPrefPath(prefs::kNTPPromoBuild) ||
      !IsBuildTargeted(GetChannel(), prefs->GetInteger(prefs::kNTPPromoBuild)))
    return false;

  // Check if we are in the right group for this promo.
  if (!prefs->FindPreference(prefs::kNTPPromoGroup) ||
      !prefs->FindPreference(prefs::kNTPPromoGroupMax) ||
      (prefs->GetInteger(prefs::kNTPPromoGroup) >=
      prefs->GetInteger(prefs::kNTPPromoGroupMax)))
    return false;

  // Check if we are in the right time window for this promo.
  if (!prefs->FindPreference(prefs::kNTPPromoStart) ||
      !prefs->FindPreference(prefs::kNTPPromoEnd) ||
      base::Time::FromDoubleT(GetNotificationStartTime(prefs)) >
          base::Time::Now() ||
      base::Time::FromDoubleT(prefs->GetDouble(prefs::kNTPPromoEnd)) <
          base::Time::Now())
    return false;

  return prefs->HasPrefPath(prefs::kNTPPromoLine);
}

void PromoResourceService::UnpackWebStoreSignal(
    const DictionaryValue& parsed_json) {
  DictionaryValue* topic_dict;
  ListValue* answer_list;

  bool is_webstore_active = false;
  bool signal_found = false;
  AppsPromo::PromoData promo_data;
  std::string promo_link = "";
  std::string promo_logo = "";
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

    // The "name" field has three different values packed into it, each
    // separated by a ':'.
    std::string name;
    if (!a_dic->GetString("name", &name))
      continue;

    // (1) the string "webstore_promo"
    size_t split = name.find(":");
    if (split == std::string::npos || name.substr(0, split) != "webstore_promo")
      continue;

    // If the "webstore_promo" string was found, that's enough to activate the
    // apps section even if the rest of the promo fails parsing.
    is_webstore_active = true;

    // (2) an integer specifying which builds the promo targets
    name = name.substr(split+1);
    split = name.find(':');
    if (split == std::string::npos ||
        !base::StringToInt(name.substr(0, split), &target_builds))
      continue;

    // (3) an integer specifying what users should maximize the promo
    name = name.substr(split+1);
    split = name.find(':');
    if (split == std::string::npos ||
        !base::StringToInt(name.substr(0, split), &promo_data.user_group))
      continue;

    // (4) optional text that specifies a URL of a logo image
    promo_logo = name.substr(split+1);

    if (!a_dic->GetString(kAnswerIdProperty, &promo_data.id) ||
        !a_dic->GetString(kWebStoreHeaderProperty, &promo_data.header) ||
        !a_dic->GetString(kWebStoreButtonProperty, &promo_data.button) ||
        !a_dic->GetString(kWebStoreLinkProperty, &promo_link) ||
        !a_dic->GetString(kWebStoreExpireProperty, &promo_data.expire))
      continue;

    if (IsThisBuildTargeted(target_builds)) {
      // The downloader will set the promo prefs and send the
      // NOTIFICATION_WEB_STORE_PROMO_LOADED notification.
      promo_data.link = GURL(promo_link);
      promo_data.logo = GURL(promo_logo);
      apps_promo_logo_fetcher_.reset(
          new AppsPromoLogoFetcher(profile_, promo_data));
      signal_found = true;
      break;
    }
  }

  if (!signal_found) {
    // If no web store promos target this build, then clear all the prefs.
    AppsPromo::ClearPromo();
  }

  AppsPromo::SetWebStoreSupportedForLocale(is_webstore_active);

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
        if (base::Time::FromString(logo_start_string.c_str(), &start_time) &&
            base::Time::FromString(logo_end_string.c_str(), &end_time)) {
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
    service->Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                    Source<WebResourceService>(this),
                    NotificationService::NoDetails());
  }
}
