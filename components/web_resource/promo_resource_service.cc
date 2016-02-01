// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_resource/promo_resource_service.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/web_resource/notification_promo.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "components/web_resource/web_resource_switches.h"
#include "url/gurl.h"

namespace web_resource {

namespace {

// Delay on first fetch so we don't interfere with startup.
const int kStartResourceFetchDelay = 5000;

// Delay between calls to fetch the promo json: 6 hours in production, and 3 min
// in debug.
const int kCacheUpdateDelay = 6 * 60 * 60 * 1000;
const int kTestCacheUpdateDelay = 3 * 60 * 1000;

// The promotion type used for Unpack() and ScheduleNotificationOnInit().
const NotificationPromo::PromoType kValidPromoTypes[] = {
#if defined(OS_ANDROID) || defined(OS_IOS)
    NotificationPromo::MOBILE_NTP_SYNC_PROMO,
#if defined(OS_IOS)
    NotificationPromo::MOBILE_NTP_WHATS_NEW_PROMO,
#endif  // defined(OS_IOS)
#else
    NotificationPromo::NTP_NOTIFICATION_PROMO,
    NotificationPromo::NTP_BUBBLE_PROMO,
#endif
};

GURL GetPromoResourceURL(version_info::Channel channel) {
  const std::string promo_server_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPromoServerURL);
  return promo_server_url.empty() ? NotificationPromo::PromoServerURL(channel)
                                  : GURL(promo_server_url);
}

bool IsTest() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPromoServerURL);
}

int GetCacheUpdateDelay() {
  return IsTest() ? kTestCacheUpdateDelay : kCacheUpdateDelay;
}

}  // namespace

// static
void PromoResourceService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kNtpPromoResourceCacheUpdate, "0");
  NotificationPromo::RegisterPrefs(registry);
}

// static
void PromoResourceService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // TODO(dbeam): This is registered only for migration; remove in M28
  // when all prefs have been cleared.  http://crbug.com/168887
  registry->RegisterStringPref(prefs::kNtpPromoResourceCacheUpdate, "0");
  NotificationPromo::RegisterProfilePrefs(registry);
}

// static
void PromoResourceService::MigrateUserPrefs(PrefService* user_prefs) {
  user_prefs->ClearPref(prefs::kNtpPromoResourceCacheUpdate);
  NotificationPromo::MigrateUserPrefs(user_prefs);
}

PromoResourceService::PromoResourceService(
    PrefService* local_state,
    version_info::Channel channel,
    const std::string& application_locale,
    net::URLRequestContextGetter* request_context,
    const char* disable_network_switch,
    const ParseJSONCallback& parse_json_callback)
    : WebResourceService(local_state,
                         GetPromoResourceURL(channel),
                         application_locale,  // append locale to URL
                         prefs::kNtpPromoResourceCacheUpdate,
                         kStartResourceFetchDelay,
                         GetCacheUpdateDelay(),
                         request_context,
                         disable_network_switch,
                         parse_json_callback),
      weak_ptr_factory_(this) {
  ScheduleNotificationOnInit();
}

PromoResourceService::~PromoResourceService() {}

scoped_ptr<PromoResourceService::StateChangedSubscription>
PromoResourceService::RegisterStateChangedCallback(
    const base::Closure& closure) {
  return callback_list_.Add(closure);
}

void PromoResourceService::ScheduleNotification(
    const NotificationPromo& notification_promo) {
  const double promo_start = notification_promo.StartTimeForGroup();
  const double promo_end = notification_promo.EndTime();

  if (promo_start > 0 && promo_end > 0) {
    const int64_t ms_until_start = static_cast<int64_t>(
        (base::Time::FromDoubleT(promo_start) - base::Time::Now())
            .InMilliseconds());
    const int64_t ms_until_end = static_cast<int64_t>(
        (base::Time::FromDoubleT(promo_end) - base::Time::Now())
            .InMilliseconds());
    if (ms_until_start > 0) {
      // Schedule the next notification to happen at the start of promotion.
      PostNotification(ms_until_start);
    } else if (ms_until_end > 0) {
      if (ms_until_start <= 0) {
        // The promo is active.  Notify immediately.
        PostNotification(0);
      }
      // Schedule the next notification to happen at the end of promotion.
      PostNotification(ms_until_end);
    } else {
      // The promo (if any) has finished.  Notify immediately.
      PostNotification(0);
    }
  } else {
    // The promo (if any) was apparently cancelled.  Notify immediately.
    PostNotification(0);
  }
}

void PromoResourceService::ScheduleNotificationOnInit() {
  // If the promo start is in the future, set a notification task to
  // invalidate the NTP cache at the time of the promo start.
  for (size_t i = 0; i < arraysize(kValidPromoTypes); ++i) {
    NotificationPromo notification_promo(prefs_);
    notification_promo.InitFromPrefs(kValidPromoTypes[i]);
    ScheduleNotification(notification_promo);
  }
}

void PromoResourceService::PostNotification(int64_t delay_ms) {
  // Note that this could cause re-issuing a notification every time
  // we receive an update from a server if something goes wrong.
  // Given that this couldn't happen more frequently than every
  // kCacheUpdateDelay milliseconds, we should be fine.
  // TODO(achuith): This crashes if we post delay_ms = 0 to the message loop.
  // during startup.
  if (delay_ms > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&PromoResourceService::PromoResourceStateChange,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay_ms));
  } else if (delay_ms == 0) {
    PromoResourceStateChange();
  }
}

void PromoResourceService::PromoResourceStateChange() {
  callback_list_.Notify();
}

void PromoResourceService::Unpack(const base::DictionaryValue& parsed_json) {
  for (size_t i = 0; i < arraysize(kValidPromoTypes); ++i) {
    NotificationPromo notification_promo(prefs_);
    notification_promo.InitFromJson(parsed_json, kValidPromoTypes[i]);
    if (notification_promo.new_notification())
      ScheduleNotification(notification_promo);
  }
}

}  // namespace web_resource
