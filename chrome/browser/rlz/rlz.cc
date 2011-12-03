// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code glues the RLZ library DLL with Chrome. It allows Chrome to work
// with or without the DLL being present. If the DLL is not present the
// functions do nothing and just return false.

#include "chrome/browser/rlz/rlz.h"

#include <windows.h>
#include <process.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace {

bool IsBrandOrganic(const std::string& brand) {
  return brand.empty() || google_util::IsOrganic(brand);
}

void RecordProductEvents(bool first_run, bool google_default_search,
                         bool google_default_homepage, bool already_ran,
                         bool omnibox_used, bool homepage_used) {
  // Record the installation of chrome. We call this all the time but the rlz
  // lib should ingore all but the first one.
  rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                              rlz_lib::CHROME_OMNIBOX,
                              rlz_lib::INSTALL);
  rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                              rlz_lib::CHROME_HOME_PAGE,
                              rlz_lib::INSTALL);

  if (!already_ran) {
    // Do the initial event recording if is the first run or if we have an
    // empty rlz which means we haven't got a chance to do it.
    char omnibox_rlz[rlz_lib::kMaxRlzLength + 1];
    if (!rlz_lib::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, omnibox_rlz,
                                    rlz_lib::kMaxRlzLength, NULL)) {
      omnibox_rlz[0] = 0;
    }

    // Record if google is the initial search provider and/or home page.
    if ((first_run || omnibox_rlz[0] == 0) && google_default_search) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  rlz_lib::CHROME_OMNIBOX,
                                  rlz_lib::SET_TO_GOOGLE);
    }

    char homepage_rlz[rlz_lib::kMaxRlzLength + 1];
    if (!rlz_lib::GetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, homepage_rlz,
                                    rlz_lib::kMaxRlzLength, NULL)) {
      homepage_rlz[0] = 0;
    }

    if ((first_run || homepage_rlz[0] == 0) && google_default_homepage) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  rlz_lib::CHROME_HOME_PAGE,
                                  rlz_lib::SET_TO_GOOGLE);
    }
  }

  // Record first user interaction with the omnibox. We call this all the
  // time but the rlz lib should ingore all but the first one.
  if (omnibox_used) {
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                rlz_lib::CHROME_OMNIBOX,
                                rlz_lib::FIRST_SEARCH);
  }

  // Record first user interaction with the home page. We call this all the
  // time but the rlz lib should ingore all but the first one.
  if (homepage_used) {
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                rlz_lib::CHROME_HOME_PAGE,
                                rlz_lib::FIRST_SEARCH);
  }
}

bool SendFinancialPing(const std::string& brand,
                       const string16& lang,
                       const string16& referral) {
  rlz_lib::AccessPoint points[] = {rlz_lib::CHROME_OMNIBOX,
                                   rlz_lib::CHROME_HOME_PAGE,
                                   rlz_lib::NO_ACCESS_POINT};
  std::string lang_ascii(WideToASCII(lang));
  std::string referral_ascii(WideToASCII(referral));
  return rlz_lib::SendFinancialPing(rlz_lib::CHROME, points, "chrome",
                                    brand.c_str(), referral_ascii.c_str(),
                                    lang_ascii.c_str(), false, NULL, true);
}

}  // namespace

RLZTracker* RLZTracker::tracker_ = NULL;

// static
RLZTracker* RLZTracker::GetInstance() {
  return tracker_ ? tracker_ : Singleton<RLZTracker>::get();
}

RLZTracker::RLZTracker()
    : first_run_(false),
      send_ping_immediately_(false),
      google_default_search_(false),
      google_default_homepage_(false),
      already_ran_(false),
      omnibox_used_(false),
      homepage_used_(false) {
}

RLZTracker::~RLZTracker() {
}

bool RLZTracker::InitRlzDelayed(bool first_run, int delay,
                                bool google_default_search,
                                bool google_default_homepage) {
  return GetInstance()->Init(first_run, delay, google_default_search,
                             google_default_homepage);
}

bool RLZTracker::Init(bool first_run, int delay, bool google_default_search,
                      bool google_default_homepage) {
  first_run_ = first_run;
  google_default_search_ = google_default_search;
  google_default_homepage_ = google_default_homepage;

  // A negative delay means that a financial ping should be sent immediately
  // after a first search is recorded, without waiting for the next restart
  // of chrome. However, we only want this behaviour on first run.
  send_ping_immediately_ = false;
  if (delay < 0) {
    send_ping_immediately_ = true;
    delay = -delay;
  }

  // Maximum and minimum delay we would allow to be set through master
  // preferences. Somewhat arbitrary, may need to be adjusted in future.
  const int kMaxDelay = 200 * 1000;
  const int kMinDelay = 20 * 1000;

  delay *= 1000;
  delay = (delay < kMinDelay) ? kMinDelay : delay;
  delay = (delay > kMaxDelay) ? kMaxDelay : delay;

  // Register for notifications from the omnibox so that we can record when
  // the user performs a first search.
  registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                 content::NotificationService::AllSources());
  // If instant is enabled we'll start searching as soon as the user starts
  // typing in the omnibox (which triggers INSTANT_CONTROLLER_UPDATED).
  registrar_.Add(this, chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
                 content::NotificationService::AllSources());

  // Register for notifications from navigations, to see if the user has used
  // the home page.
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                 content::NotificationService::AllSources());

  ScheduleDelayedInit(delay);

  return true;
}

void RLZTracker::ScheduleDelayedInit(int delay) {
  // The RLZTracker is a singleton object that outlives any runnable tasks
  // that will be queued up.
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&RLZTracker::DelayedInit, base::Unretained(this)),
      delay);
}

void RLZTracker::DelayedInit() {
  bool schedule_ping = false;

  // For organic brandcodes do not use rlz at all. Empty brandcode usually
  // means a chromium install. This is ok.
  std::string brand;
  if (google_util::GetBrand(&brand) && !IsBrandOrganic(brand)) {
    RecordProductEvents(first_run_, google_default_search_,
                        google_default_homepage_, already_ran_,
                        omnibox_used_, homepage_used_);
    schedule_ping = true;
  }

  // If chrome has been reactivated, record the events for this brand
  // as well.
  std::string reactivation_brand;
  if (google_util::GetReactivationBrand(&reactivation_brand) &&
      !IsBrandOrganic(reactivation_brand)) {
    string16 reactivation_brand16 = UTF8ToUTF16(reactivation_brand);
    rlz_lib::SupplementaryBranding branding(reactivation_brand16.c_str());
    RecordProductEvents(first_run_, google_default_search_,
                        google_default_homepage_, already_ran_,
                        omnibox_used_, homepage_used_);
    schedule_ping = true;
  }

  already_ran_ = true;

  if (schedule_ping)
    ScheduleFinancialPing();
}

void RLZTracker::ScheduleFinancialPing() {
  // Investigate why _beginthread() is used here, and not chrome's threading
  // API.  Tracked in bug http://crbug.com/106213
  _beginthread(PingNow, 0, this);
}

// static
void _cdecl RLZTracker::PingNow(void* arg) {
  RLZTracker* tracker = reinterpret_cast<RLZTracker*>(arg);
  tracker->PingNowImpl();
}

void RLZTracker::PingNowImpl() {
  // This is the entry point of a background thread, so I/O is allowed.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  string16 lang;
  GoogleUpdateSettings::GetLanguage(&lang);
  if (lang.empty())
    lang = L"en";
  string16 referral;
  GoogleUpdateSettings::GetReferral(&referral);

  std::string brand;
  if (google_util::GetBrand(&brand) && !IsBrandOrganic(brand) &&
      SendFinancialPing(brand, lang, referral)) {
    GoogleUpdateSettings::ClearReferral();

    {
      base::AutoLock lock(cache_lock_);
      rlz_cache_.clear();
    }

    // Prime the RLZ cache for the access points we are interested in.
    GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, NULL);
    GetAccessPointRlz(rlz_lib::CHROME_HOME_PAGE, NULL);
  }

  std::string reactivation_brand;
  if (google_util::GetReactivationBrand(&reactivation_brand) &&
      !IsBrandOrganic(reactivation_brand)) {
    string16 reactivation_brand16 = UTF8ToUTF16(reactivation_brand);
    rlz_lib::SupplementaryBranding branding(reactivation_brand16.c_str());
    SendFinancialPing(reactivation_brand, lang, referral);
  }
}

bool RLZTracker::SendFinancialPing(const std::string& brand,
                                   const string16& lang,
                                   const string16& referral) {
  return ::SendFinancialPing(brand, lang, referral);
}

void RLZTracker::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  // Needs to be evaluated. See http://crbug.com/62328.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  rlz_lib::AccessPoint point;
  bool* record_used = NULL;
  bool call_record = false;

  switch (type) {
    case chrome::NOTIFICATION_OMNIBOX_OPENED_URL:
    case chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED:
      point = rlz_lib::CHROME_OMNIBOX;
      record_used = &omnibox_used_;
      call_record = true;

      registrar_.Remove(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                        content::NotificationService::AllSources());
      registrar_.Remove(this, chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
                        content::NotificationService::AllSources());
      break;
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      const NavigationEntry* entry =
          content::Details<NavigationEntry>(details).ptr();
      if (entry != NULL &&
          ((entry->transition_type() &
            content::PAGE_TRANSITION_HOME_PAGE) != 0)) {
        point = rlz_lib::CHROME_HOME_PAGE;
        record_used = &homepage_used_;
        call_record = true;

        registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                          content::NotificationService::AllSources());
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  if (call_record) {
    // Try to record event now, else set the flag to try later when we
    // attempt the ping.
    if (!RecordProductEvent(rlz_lib::CHROME, point, rlz_lib::FIRST_SEARCH))
      *record_used = true;
    else if (send_ping_immediately_ && point == rlz_lib::CHROME_OMNIBOX) {
      ScheduleDelayedInit(0);
    }
  }
}

bool RLZTracker::RecordProductEvent(rlz_lib::Product product,
                                    rlz_lib::AccessPoint point,
                                    rlz_lib::Event event_id) {
  bool ret = rlz_lib::RecordProductEvent(product, point, event_id);

  // If chrome has been reactivated, record the event for this brand as well.
  std::string reactivation_brand;
  if (google_util::GetReactivationBrand(&reactivation_brand)) {
    string16 reactivation_brand16 = UTF8ToUTF16(reactivation_brand);
    rlz_lib::SupplementaryBranding branding(reactivation_brand16.c_str());
    ret &= rlz_lib::RecordProductEvent(product, point, event_id);
  }

  return ret;
}

// GetAccessPointRlz() caches RLZ strings for all access points. If we had
// a successful ping, then we update the cached value.
bool RLZTracker::GetAccessPointRlz(rlz_lib::AccessPoint point,
                                   string16* rlz) {
  return GetInstance()->GetAccessPointRlzImpl(point, rlz);
}

// GetAccessPointRlz() caches RLZ strings for all access points. If we had
// a successful ping, then we update the cached value.
bool RLZTracker::GetAccessPointRlzImpl(rlz_lib::AccessPoint point,
                                       string16* rlz) {
  // If the RLZ string for the specified access point is already cached,
  // simply return its value.
  {
    base::AutoLock lock(cache_lock_);
    if (rlz_cache_.find(point) != rlz_cache_.end()) {
      if (rlz)
        *rlz = rlz_cache_[point];
      return true;
    }
  }

  // Make sure we don't access disk outside of the I/O thread.
  // In such case we repost the task on the right thread and return error.
  if (ScheduleGetAccessPointRlz(point))
    return false;

  char str_rlz[rlz_lib::kMaxRlzLength + 1];
  if (!rlz_lib::GetAccessPointRlz(point, str_rlz, rlz_lib::kMaxRlzLength, NULL))
    return false;

  string16 rlz_local(ASCIIToWide(std::string(str_rlz)));
  if (rlz)
    *rlz = rlz_local;

  base::AutoLock lock(cache_lock_);
  rlz_cache_[point] = rlz_local;
  return true;
}

bool RLZTracker::ScheduleGetAccessPointRlz(rlz_lib::AccessPoint point) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return false;

  string16* not_used = NULL;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::IgnoreReturn<bool>(
          base::Bind(&RLZTracker::GetAccessPointRlz, point, not_used)));
  return true;
}

// static
void RLZTracker::CleanupRlz() {
  GetInstance()->rlz_cache_.clear();
  GetInstance()->registrar_.RemoveAll();
}
