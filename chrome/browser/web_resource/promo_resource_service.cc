// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/promo_resource_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_resource/notification_promo.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "googleurl/src/gurl.h"

namespace {

// Delay on first fetch so we don't interfere with startup.
const int kStartResourceFetchDelay = 5000;

// Delay between calls to update the cache (12 hours), and 3 min in debug mode.
const int kCacheUpdateDelay = 12 * 60 * 60 * 1000;
const int kTestCacheUpdateDelay = 3 * 60 * 1000;

// The version of the service (used to expire the cache when upgrading Chrome
// to versions with different types of promos).
const int kPromoServiceVersion = 7;

// The promotion type used for Unpack() and ScheduleNotificationOnInit.
const NotificationPromo::PromoType kValidPromoTypes[] = {
#if defined(OS_ANDROID) || defined(OS_IOS)
    NotificationPromo::MOBILE_NTP_SYNC_PROMO,
#else
    NotificationPromo::NTP_NOTIFICATION_PROMO,
    NotificationPromo::NTP_BUBBLE_PROMO,
#endif
};

GURL GetPromoResourceURL() {
  const std::string promo_server_url = CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kPromoServerURL);
  return promo_server_url.empty() ?
      NotificationPromo::PromoServerURL() : GURL(promo_server_url);
}

bool IsTest() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kPromoServerURL);
}

int GetCacheUpdateDelay() {
  return IsTest() ? kTestCacheUpdateDelay : kCacheUpdateDelay;
}

}  // namespace

// static
void PromoResourceService::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterIntegerPref(prefs::kNtpPromoVersion, 0);
  local_state->RegisterStringPref(prefs::kNtpPromoLocale, std::string());
}

// static
void PromoResourceService::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kNtpPromoResourceCacheUpdate,
                            "0",
                            PrefService::UNSYNCABLE_PREF);
  NotificationPromo::RegisterUserPrefs(prefs);

  // TODO(achuith): Delete this in M22.
  prefs->RegisterDoublePref(prefs::kNtpCustomLogoStart,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kNtpCustomLogoEnd,
                            0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->ClearPref(prefs::kNtpCustomLogoStart);
  prefs->ClearPref(prefs::kNtpCustomLogoEnd);
}

PromoResourceService::PromoResourceService(Profile* profile)
    : WebResourceService(profile->GetPrefs(),
                         GetPromoResourceURL(),
                         true,  // append locale to URL
                         prefs::kNtpPromoResourceCacheUpdate,
                         kStartResourceFetchDelay,
                         GetCacheUpdateDelay()),
                         profile_(profile),
                         ALLOW_THIS_IN_INITIALIZER_LIST(
                             weak_ptr_factory_(this)) {
  ScheduleNotificationOnInit();
}

PromoResourceService::~PromoResourceService() {
}

void PromoResourceService::ScheduleNotification(
    const NotificationPromo& notification_promo) {
  const double promo_start = notification_promo.StartTimeForGroup();
  const double promo_end = notification_promo.EndTime();

  if (promo_start > 0 && promo_end > 0) {
    const int64 ms_until_start =
        static_cast<int64>((base::Time::FromDoubleT(
            promo_start) - base::Time::Now()).InMilliseconds());
    const int64 ms_until_end =
        static_cast<int64>((base::Time::FromDoubleT(
            promo_end) - base::Time::Now()).InMilliseconds());
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
  std::string locale = g_browser_process->GetApplicationLocale();
  if (GetPromoServiceVersion() != kPromoServiceVersion ||
      GetPromoLocale() != locale) {
    // If the promo service has been upgraded or Chrome switched locales,
    // refresh the promos.
    // TODO(achuith): Mixing local_state and prefs does not work for
    // multi-profile case. We should probably store version/locale in prefs_
    // as well.
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetInteger(prefs::kNtpPromoVersion, kPromoServiceVersion);
    local_state->SetString(prefs::kNtpPromoLocale, locale);
    prefs_->ClearPref(prefs::kNtpPromoResourceCacheUpdate);
    PostNotification(0);
  } else {
    // If the promo start is in the future, set a notification task to
    // invalidate the NTP cache at the time of the promo start.
    for (size_t i = 0; i < arraysize(kValidPromoTypes); ++i) {
      NotificationPromo notification_promo(profile_);
      notification_promo.InitFromPrefs(kValidPromoTypes[i]);
      ScheduleNotification(notification_promo);
    }
  }
}

void PromoResourceService::PostNotification(int64 delay_ms) {
  // Note that this could cause re-issuing a notification every time
  // we receive an update from a server if something goes wrong.
  // Given that this couldn't happen more frequently than every
  // kCacheUpdateDelay milliseconds, we should be fine.
  // TODO(achuith): This crashes if we post delay_ms = 0 to the message loop.
  // during startup.
  if (delay_ms > 0) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PromoResourceService::PromoResourceStateChange,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay_ms));
  } else if (delay_ms == 0) {
    PromoResourceStateChange();
  }
}

void PromoResourceService::PromoResourceStateChange() {
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_PROMO_RESOURCE_STATE_CHANGED,
                  content::Source<WebResourceService>(this),
                  content::NotificationService::NoDetails());
}

int PromoResourceService::GetPromoServiceVersion() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetInteger(prefs::kNtpPromoVersion);
}

std::string PromoResourceService::GetPromoLocale() {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetString(prefs::kNtpPromoLocale);
}

void PromoResourceService::Unpack(const DictionaryValue& parsed_json) {
  for (size_t i = 0; i < arraysize(kValidPromoTypes); ++i) {
    NotificationPromo notification_promo(profile_);
    notification_promo.InitFromJson(parsed_json, kValidPromoTypes[i]);
    if (notification_promo.new_notification())
      ScheduleNotification(notification_promo);
  }
}
