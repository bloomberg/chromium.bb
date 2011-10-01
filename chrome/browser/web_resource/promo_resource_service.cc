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

// Delay between calls to update the cache (48 hours), and 3 min in debug mode.
static const int kCacheUpdateDelay = 48 * 60 * 60 * 1000;
static const int kTestCacheUpdateDelay = 3 * 60 * 1000;

// The version of the service (used to expire the cache when upgrading Chrome
// to versions with different types of promos).
static const int kPromoServiceVersion = 2;

// Properties used by the server.
static const char kAnswerIdProperty[] = "answer_id";
static const char kWebStoreHeaderProperty[] = "question";
static const char kWebStoreButtonProperty[] = "inproduct_target";
static const char kWebStoreLinkProperty[] = "inproduct";
static const char kWebStoreExpireProperty[] = "tooltip";

const char* GetPromoResourceURL() {
  std::string promo_server_url = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kPromoServerURL);
  return promo_server_url.empty() ?
      PromoResourceService::kDefaultPromoResourceServer :
      promo_server_url.c_str();
}

bool IsTest() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kPromoServerURL);
}

int GetCacheUpdateDelay() {
  return IsTest() ? kTestCacheUpdateDelay : kCacheUpdateDelay;
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
  NotificationPromo::RegisterUserPrefs(prefs);
}

// static
chrome::VersionInfo::Channel PromoResourceService::GetChannel() {
  // GetChannel hits the registry on Windows. See http://crbug.com/70898.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  return chrome::VersionInfo::GetChannel();
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
                         GetCacheUpdateDelay()),
                         profile_(profile),
                         channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
  Init();
}

PromoResourceService::~PromoResourceService() { }

void PromoResourceService::Init() {
  ScheduleNotificationOnInit();
}

bool PromoResourceService::IsBuildTargeted(int builds_targeted) {
  if (channel_ == chrome::VersionInfo::CHANNEL_UNKNOWN)
    channel_ = GetChannel();

  return IsBuildTargeted(channel_, builds_targeted);
}

void PromoResourceService::Unpack(const DictionaryValue& parsed_json) {
  UnpackLogoSignal(parsed_json);
  UnpackNotificationSignal(parsed_json);
  UnpackWebStoreSignal(parsed_json);
}

void PromoResourceService::OnNewNotification(double start, double end) {
  ScheduleNotification(start, end);
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
  NotificationPromo notification_promo(prefs_, this);
  notification_promo.InitFromJson(parsed_json);
}

bool PromoResourceService::CanShowNotificationPromo(Profile* profile) {
  NotificationPromo notification_promo(profile->GetPrefs(), NULL);
  notification_promo.InitFromPrefs();
  return notification_promo.CanShow();
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

    if (IsBuildTargeted(target_builds)) {
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
