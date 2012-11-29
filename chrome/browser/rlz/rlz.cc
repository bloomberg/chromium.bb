// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code glues the RLZ library DLL with Chrome. It allows Chrome to work
// with or without the DLL being present. If the DLL is not present the
// functions do nothing and just return false.

#include "chrome/browser/rlz/rlz.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "net/http/http_util.h"

#if defined(OS_WIN)
#include "chrome/installer/util/google_update_settings.h"
#else
namespace GoogleUpdateSettings {
static bool GetLanguage(string16* language) {
  // TODO(thakis): Implement.
  NOTIMPLEMENTED();
  return false;
}

// The referral program is defunct and not used. No need to implement these
// functions on non-Win platforms.
static bool GetReferral(string16* referral) {
  return true;
}
static bool ClearReferral() {
  return true;
}
}  // namespace GoogleUpdateSettings
#endif

// There is no corresponding header for content/browser/browser_main_runner.cc
// to export this global but it is used in other places as well (also defined
// as "extern").
extern bool g_exited_main_message_loop;

using content::BrowserThread;
using content::NavigationEntry;

namespace {

bool IsBrandOrganic(const std::string& brand) {
  return brand.empty() || google_util::IsOrganic(brand);
}

void RecordProductEvents(bool first_run,
                         bool is_google_default_search,
                         bool is_google_homepage,
                         bool is_google_in_startpages,
                         bool already_ran,
                         bool omnibox_used,
                         bool homepage_used) {
  TRACE_EVENT0("RLZ", "RecordProductEvents");
  // Record the installation of chrome. We call this all the time but the rlz
  // lib should ignore all but the first one.
  rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                              RLZTracker::CHROME_OMNIBOX,
                              rlz_lib::INSTALL);
  rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                              RLZTracker::CHROME_HOME_PAGE,
                              rlz_lib::INSTALL);

  if (!already_ran) {
    // Do the initial event recording if is the first run or if we have an
    // empty rlz which means we haven't got a chance to do it.
    char omnibox_rlz[rlz_lib::kMaxRlzLength + 1];
    if (!rlz_lib::GetAccessPointRlz(RLZTracker::CHROME_OMNIBOX, omnibox_rlz,
                                    rlz_lib::kMaxRlzLength)) {
      omnibox_rlz[0] = 0;
    }

    // Record if google is the initial search provider and/or home page.
    if ((first_run || omnibox_rlz[0] == 0) && is_google_default_search) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  RLZTracker::CHROME_OMNIBOX,
                                  rlz_lib::SET_TO_GOOGLE);
    }

    char homepage_rlz[rlz_lib::kMaxRlzLength + 1];
    if (!rlz_lib::GetAccessPointRlz(RLZTracker::CHROME_HOME_PAGE, homepage_rlz,
                                    rlz_lib::kMaxRlzLength)) {
      homepage_rlz[0] = 0;
    }

    if ((first_run || homepage_rlz[0] == 0) &&
        (is_google_homepage || is_google_in_startpages)) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  RLZTracker::CHROME_HOME_PAGE,
                                  rlz_lib::SET_TO_GOOGLE);
    }
  }

  // Record first user interaction with the omnibox. We call this all the
  // time but the rlz lib should ingore all but the first one.
  if (omnibox_used) {
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                RLZTracker::CHROME_OMNIBOX,
                                rlz_lib::FIRST_SEARCH);
  }

  // Record first user interaction with the home page. We call this all the
  // time but the rlz lib should ingore all but the first one.
  if (homepage_used || is_google_in_startpages) {
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                RLZTracker::CHROME_HOME_PAGE,
                                rlz_lib::FIRST_SEARCH);
  }
}

bool SendFinancialPing(const std::string& brand,
                       const string16& lang,
                       const string16& referral) {
  // It's possible for the user to close Chrome just before the time that this
  // ping is due to go out.  Detect that condition and abort.
  if (g_exited_main_message_loop)
    return false;

  rlz_lib::AccessPoint points[] = {RLZTracker::CHROME_OMNIBOX,
                                   RLZTracker::CHROME_HOME_PAGE,
                                   rlz_lib::NO_ACCESS_POINT};
  std::string lang_ascii(UTF16ToASCII(lang));
  std::string referral_ascii(UTF16ToASCII(referral));
  return rlz_lib::SendFinancialPing(rlz_lib::CHROME, points, "chrome",
                                    brand.c_str(), referral_ascii.c_str(),
                                    lang_ascii.c_str(), false, true);
}

}  // namespace

#if defined(OS_WIN)
// static
const rlz_lib::AccessPoint RLZTracker::CHROME_OMNIBOX =
    rlz_lib::CHROME_OMNIBOX;
// static
const rlz_lib::AccessPoint RLZTracker::CHROME_HOME_PAGE =
    rlz_lib::CHROME_HOME_PAGE;
#elif defined(OS_MACOSX)
// static
const rlz_lib::AccessPoint RLZTracker::CHROME_OMNIBOX =
    rlz_lib::CHROME_MAC_OMNIBOX;
// static
const rlz_lib::AccessPoint RLZTracker::CHROME_HOME_PAGE =
    rlz_lib::CHROME_MAC_HOME_PAGE;
#elif defined(OS_CHROMEOS)
// static
const rlz_lib::AccessPoint RLZTracker::CHROME_OMNIBOX =
    rlz_lib::CHROMEOS_OMNIBOX;
// static
const rlz_lib::AccessPoint RLZTracker::CHROME_HOME_PAGE =
    rlz_lib::CHROMEOS_HOME_PAGE;
#endif

RLZTracker* RLZTracker::tracker_ = NULL;

// static
RLZTracker* RLZTracker::GetInstance() {
  return tracker_ ? tracker_ : Singleton<RLZTracker>::get();
}

RLZTracker::RLZTracker()
    : first_run_(false),
      send_ping_immediately_(false),
      is_google_default_search_(false),
      is_google_homepage_(false),
      is_google_in_startpages_(false),
      already_ran_(false),
      omnibox_used_(false),
      homepage_used_(false) {
}

RLZTracker::~RLZTracker() {
}

// static
bool RLZTracker::InitRlzDelayed(bool first_run,
                                int delay,
                                bool is_google_default_search,
                                bool is_google_homepage,
                                bool is_google_in_startpages) {
  return GetInstance()->Init(first_run, delay, is_google_default_search,
                             is_google_homepage, is_google_in_startpages);
}

bool RLZTracker::Init(bool first_run,
                      int delay,
                      bool is_google_default_search,
                      bool is_google_homepage,
                      bool is_google_in_startpages) {
  first_run_ = first_run;
  is_google_default_search_ = is_google_default_search;
  is_google_homepage_ = is_google_homepage;
  is_google_in_startpages_ = is_google_in_startpages;

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

  std::string brand;
  if (google_util::GetBrand(&brand) && !IsBrandOrganic(brand)) {
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
  }

  rlz_lib::SetURLRequestContext(g_browser_process->system_request_context());
  ScheduleDelayedInit(delay);

  return true;
}

void RLZTracker::ScheduleDelayedInit(int delay) {
  // The RLZTracker is a singleton object that outlives any runnable tasks
  // that will be queued up.
  BrowserThread::GetBlockingPool()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&RLZTracker::DelayedInit, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(delay));
}

void RLZTracker::DelayedInit() {
  worker_pool_token_ = BrowserThread::GetBlockingPool()->GetSequenceToken();

  bool schedule_ping = false;

  // For organic brandcodes do not use rlz at all. Empty brandcode usually
  // means a chromium install. This is ok.
  std::string brand;
  if (google_util::GetBrand(&brand) && !IsBrandOrganic(brand)) {
    RecordProductEvents(first_run_, is_google_default_search_,
                        is_google_homepage_, is_google_in_startpages_,
                        already_ran_, omnibox_used_, homepage_used_);
    schedule_ping = true;
  }

  // If chrome has been reactivated, record the events for this brand
  // as well.
  std::string reactivation_brand;
  if (google_util::GetReactivationBrand(&reactivation_brand) &&
      !IsBrandOrganic(reactivation_brand)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
    RecordProductEvents(first_run_, is_google_default_search_,
                        is_google_homepage_, is_google_in_startpages_,
                        already_ran_, omnibox_used_, homepage_used_);
    schedule_ping = true;
  }

  already_ran_ = true;

  if (schedule_ping)
    ScheduleFinancialPing();
}

void RLZTracker::ScheduleFinancialPing() {
  BrowserThread::GetBlockingPool()->PostSequencedWorkerTask(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&RLZTracker::PingNowImpl, base::Unretained(this)));
}

void RLZTracker::PingNowImpl() {
  TRACE_EVENT0("RLZ", "RLZTracker::PingNowImpl");
  string16 lang;
  GoogleUpdateSettings::GetLanguage(&lang);
  if (lang.empty())
    lang = ASCIIToUTF16("en");
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
    GetAccessPointRlz(RLZTracker::CHROME_OMNIBOX, NULL);
    GetAccessPointRlz(RLZTracker::CHROME_HOME_PAGE, NULL);
  }

  std::string reactivation_brand;
  if (google_util::GetReactivationBrand(&reactivation_brand) &&
      !IsBrandOrganic(reactivation_brand)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
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
  switch (type) {
    case chrome::NOTIFICATION_OMNIBOX_OPENED_URL:
    case chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED:
      RecordFirstSearch(CHROME_OMNIBOX);
      registrar_.Remove(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                        content::NotificationService::AllSources());
      registrar_.Remove(this, chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
                        content::NotificationService::AllSources());
      break;
    case content::NOTIFICATION_NAV_ENTRY_PENDING: {
      const NavigationEntry* entry =
          content::Details<content::NavigationEntry>(details).ptr();
      if (entry != NULL &&
          ((entry->GetTransitionType() &
            content::PAGE_TRANSITION_HOME_PAGE) != 0)) {
        RecordFirstSearch(CHROME_HOME_PAGE);
        registrar_.Remove(this, content::NOTIFICATION_NAV_ENTRY_PENDING,
                          content::NotificationService::AllSources());
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

// static
bool RLZTracker::RecordProductEvent(rlz_lib::Product product,
                                    rlz_lib::AccessPoint point,
                                    rlz_lib::Event event_id) {
  return GetInstance()->RecordProductEventImpl(product, point, event_id);
}

bool RLZTracker::RecordProductEventImpl(rlz_lib::Product product,
                                        rlz_lib::AccessPoint point,
                                        rlz_lib::Event event_id) {
  // Make sure we don't access disk outside of the I/O thread.
  // In such case we repost the task on the right thread and return error.
  if (ScheduleRecordProductEvent(product, point, event_id))
    return true;

  bool ret = rlz_lib::RecordProductEvent(product, point, event_id);

  // If chrome has been reactivated, record the event for this brand as well.
  std::string reactivation_brand;
  if (google_util::GetReactivationBrand(&reactivation_brand)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
    ret &= rlz_lib::RecordProductEvent(product, point, event_id);
  }

  return ret;
}

bool RLZTracker::ScheduleRecordProductEvent(rlz_lib::Product product,
                                            rlz_lib::AccessPoint point,
                                            rlz_lib::Event event_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return false;
  if (!already_ran_) {
    LOG(ERROR) << "Attempted recording RLZ event before RLZ init.";
    return true;
  }

  BrowserThread::GetBlockingPool()->PostSequencedWorkerTask(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&RLZTracker::RecordProductEvent),
                 product, point, event_id));

  return true;
}

void RLZTracker::RecordFirstSearch(rlz_lib::AccessPoint point) {
  // Make sure we don't access disk outside of the I/O thread.
  // In such case we repost the task on the right thread and return error.
  if (ScheduleRecordFirstSearch(point))
    return;

  bool* record_used = point == CHROME_OMNIBOX ?
      &omnibox_used_ : &homepage_used_;

  // Try to record event now, else set the flag to try later when we
  // attempt the ping.
  if (!RecordProductEvent(rlz_lib::CHROME, point, rlz_lib::FIRST_SEARCH))
    *record_used = true;
  else if (send_ping_immediately_ && point == CHROME_OMNIBOX)
    ScheduleDelayedInit(0);
}

bool RLZTracker::ScheduleRecordFirstSearch(rlz_lib::AccessPoint point) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return false;
  BrowserThread::GetBlockingPool()->PostSequencedWorkerTask(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&RLZTracker::RecordFirstSearch,
                 base::Unretained(this), point));
  return true;
}

// static
std::string RLZTracker::GetAccessPointHttpHeader(rlz_lib::AccessPoint point) {
  TRACE_EVENT0("RLZ", "RLZTracker::GetAccessPointHttpHeader");
  std::string extra_headers;
  string16 rlz_string;
  RLZTracker::GetAccessPointRlz(point, &rlz_string);
  if (!rlz_string.empty()) {
    net::HttpUtil::AppendHeaderIfMissing("X-Rlz-String",
                                         UTF16ToUTF8(rlz_string),
                                         &extra_headers);
  }

  return extra_headers;
}

// GetAccessPointRlz() caches RLZ strings for all access points. If we had
// a successful ping, then we update the cached value.
bool RLZTracker::GetAccessPointRlz(rlz_lib::AccessPoint point,
                                   string16* rlz) {
  TRACE_EVENT0("RLZ", "RLZTracker::GetAccessPointRlz");
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
  if (!rlz_lib::GetAccessPointRlz(point, str_rlz, rlz_lib::kMaxRlzLength))
    return false;

  string16 rlz_local(ASCIIToUTF16(std::string(str_rlz)));
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
  BrowserThread::GetBlockingPool()->PostSequencedWorkerTask(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&RLZTracker::GetAccessPointRlz), point,
                 not_used));
  return true;
}

// static
void RLZTracker::CleanupRlz() {
  GetInstance()->rlz_cache_.clear();
  GetInstance()->registrar_.RemoveAll();
}
