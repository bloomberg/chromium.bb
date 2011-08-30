// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code glues the RLZ library DLL with Chrome. It allows Chrome to work
// with or without the DLL being present. If the DLL is not present the
// functions do nothing and just return false.

#include "chrome/browser/rlz/rlz.h"

#include <process.h>
#include <windows.h>

#include <algorithm>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/common/notification_service.h"

namespace {

// Organic brands all start with GG, such as GGCM.
static bool is_organic(const std::wstring& brand) {
  return (brand.size() < 2) ? false : (brand.substr(0, 2) == L"GG");
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

bool SendFinancialPing(const std::wstring& brand,
                       const std::wstring& lang,
                       const std::wstring& referral,
                       bool exclude_id) {
  rlz_lib::AccessPoint points[] = {rlz_lib::CHROME_OMNIBOX,
                                   rlz_lib::CHROME_HOME_PAGE,
                                   rlz_lib::NO_ACCESS_POINT};
  std::string brand_ascii(WideToASCII(brand));
  std::string lang_ascii(WideToASCII(lang));
  std::string referral_ascii(WideToASCII(referral));

  // If chrome has been reactivated, send a ping for this brand as well.
  // We ignore the return value of SendFinancialPing() since we'll try again
  // later anyway. Callers of this function are only interested in whether
  // the ping for the main brand succeeded or not.
  std::wstring reactivation_brand;
  if (GoogleUpdateSettings::GetReactivationBrand(&reactivation_brand)) {
    std::string reactivation_brand_ascii(WideToASCII(reactivation_brand));
    rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
    rlz_lib::SendFinancialPing(rlz_lib::CHROME, points, "chrome",
                               reactivation_brand_ascii.c_str(),
                               referral_ascii.c_str(), lang_ascii.c_str(),
                               exclude_id, NULL, true);
  }

  return rlz_lib::SendFinancialPing(rlz_lib::CHROME, points, "chrome",
                                    brand_ascii.c_str(), referral_ascii.c_str(),
                                    lang_ascii.c_str(), exclude_id, NULL, true);
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
                 NotificationService::AllSources());
  // If instant is enabled we'll start searching as soon as the user starts
  // typing in the omnibox (which triggers INSTANT_CONTROLLER_UPDATED).
  registrar_.Add(this, chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
                 NotificationService::AllSources());

  // Register for notifications from navigations, to see if the user has used
  // the home page.
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                 NotificationService::AllSources());

  ScheduleDelayedInit(delay);
  return true;
}

void RLZTracker::ScheduleDelayedInit(int delay) {
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableMethod(this, &RLZTracker::DelayedInit),
      delay);
}

void RLZTracker::DelayedInit() {
  // For organic brandcodes do not use rlz at all. Empty brandcode usually
  // means a chromium install. This is ok.
  std::wstring brand;
  if (!GoogleUpdateSettings::GetBrand(&brand) || brand.empty() ||
      GoogleUpdateSettings::IsOrganic(brand))
    return;

  RecordProductEvents(first_run_, google_default_search_,
                      google_default_homepage_, already_ran_,
                      omnibox_used_, homepage_used_);

  // If chrome has been reactivated, record the events for this brand
  // as well.
  std::wstring reactivation_brand;
  if (GoogleUpdateSettings::GetReactivationBrand(&reactivation_brand)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
    RecordProductEvents(first_run_, google_default_search_,
                        google_default_homepage_, already_ran_,
                        omnibox_used_, homepage_used_);
  }

  already_ran_ = true;

  ScheduleFinancialPing();
}

void RLZTracker::ScheduleFinancialPing() {
  _beginthread(PingNow, 0, this);
}

// static
void _cdecl RLZTracker::PingNow(void* arg) {
  RLZTracker* tracker = reinterpret_cast<RLZTracker*>(arg);
  tracker->PingNowImpl();
}

void RLZTracker::PingNowImpl() {
  // Needs to be evaluated. See http://crbug.com/62328.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  std::wstring lang;
  GoogleUpdateSettings::GetLanguage(&lang);
  if (lang.empty())
    lang = L"en";
  std::wstring brand;
  GoogleUpdateSettings::GetBrand(&brand);
  std::wstring referral;
  GoogleUpdateSettings::GetReferral(&referral);
  if (SendFinancialPing(brand, lang, referral, is_organic(brand))) {
    GoogleUpdateSettings::ClearReferral();
    base::AutoLock lock(cache_lock_);
    rlz_cache_.clear();
  }
}

bool RLZTracker::SendFinancialPing(const std::wstring& brand,
                                   const std::wstring& lang,
                                   const std::wstring& referral,
                                   bool exclude_id) {
  return ::SendFinancialPing(brand, lang, referral, exclude_id);
}

void RLZTracker::Observe(int type,
                         const NotificationSource& source,
                         const NotificationDetails& details) {
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
                        NotificationService::AllSources());
      registrar_.Remove(this, chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
                        NotificationService::AllSources());
      break;
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      const NavigationEntry* entry = Details<NavigationEntry>(details).ptr();
      if (entry != NULL &&
          ((entry->transition_type() & RLZ_PAGETRANSITION_HOME_PAGE) != 0)) {
        point = rlz_lib::CHROME_HOME_PAGE;
        record_used = &homepage_used_;
        call_record = true;

        registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                          NotificationService::AllSources());
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
  std::wstring reactivation_brand;
  if (GoogleUpdateSettings::GetReactivationBrand(&reactivation_brand)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
    ret &= rlz_lib::RecordProductEvent(product, point, event_id);
  }

  return ret;
}

// GetAccessPointRlz() caches RLZ strings for all access points. If we had
// a successful ping, then we update the cached value.
bool RLZTracker::GetAccessPointRlz(rlz_lib::AccessPoint point,
                                   std::wstring* rlz) {
  return GetInstance()->GetAccessPointRlzImpl(point, rlz);
}

// GetAccessPointRlz() caches RLZ strings for all access points. If we had
// a successful ping, then we update the cached value.
bool RLZTracker::GetAccessPointRlzImpl(rlz_lib::AccessPoint point,
                                       std::wstring* rlz) {
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

  std::wstring rlz_local(ASCIIToWide(std::string(str_rlz)));
  if (rlz)
    *rlz = rlz_local;

  base::AutoLock lock(cache_lock_);
  rlz_cache_[point] = rlz_local;
  return true;
}

bool RLZTracker::ScheduleGetAccessPointRlz(rlz_lib::AccessPoint point) {
  if (BrowserThread::CurrentlyOn(BrowserThread::FILE))
    return false;

  std::wstring* not_used = NULL;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(&RLZTracker::GetAccessPointRlz, point, not_used));
  return true;
}

// static
void RLZTracker::CleanupRlz() {
  GetInstance()->rlz_cache_.clear();
  GetInstance()->registrar_.RemoveAll();
}
