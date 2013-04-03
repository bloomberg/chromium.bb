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
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
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

using content::BrowserThread;
using content::NavigationEntry;

namespace {

// Maximum and minimum delay for financial ping we would allow to be set through
// master preferences. Somewhat arbitrary, may need to be adjusted in future.
const base::TimeDelta kMaxInitDelay = base::TimeDelta::FromSeconds(200);
const base::TimeDelta kMinInitDelay = base::TimeDelta::FromSeconds(20);

bool IsGoogleUrl(const GURL& url) {
  return google_util::IsGoogleHomePageUrl(url.possibly_invalid_spec());
}

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
  rlz_lib::AccessPoint points[] = {RLZTracker::CHROME_OMNIBOX,
                                   RLZTracker::CHROME_HOME_PAGE,
                                   rlz_lib::NO_ACCESS_POINT};
  std::string lang_ascii(UTF16ToASCII(lang));
  std::string referral_ascii(UTF16ToASCII(referral));
  std::string product_signature;
#if defined(OS_CHROMEOS)
  product_signature = "chromeos";
#else
  product_signature = "chrome";
#endif
  return rlz_lib::SendFinancialPing(rlz_lib::CHROME, points,
                                    product_signature.c_str(),
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
      worker_pool_token_(BrowserThread::GetBlockingPool()->GetSequenceToken()),
      already_ran_(false),
      omnibox_used_(false),
      homepage_used_(false),
      min_init_delay_(kMinInitDelay) {
}

RLZTracker::~RLZTracker() {
}

// static
bool RLZTracker::InitRlzDelayed(bool first_run,
                                bool send_ping_immediately,
                                base::TimeDelta delay,
                                bool is_google_default_search,
                                bool is_google_homepage,
                                bool is_google_in_startpages) {
  return GetInstance()->Init(first_run, send_ping_immediately, delay,
                             is_google_default_search, is_google_homepage,
                             is_google_in_startpages);
}

// static
bool RLZTracker::InitRlzFromProfileDelayed(Profile* profile,
                                           bool first_run,
                                           bool send_ping_immediately,
                                           base::TimeDelta delay) {
  bool is_google_default_search = false;
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service) {
    const TemplateURL* url_template =
        template_url_service->GetDefaultSearchProvider();
    is_google_default_search =
        url_template && url_template->url_ref().HasGoogleBaseURLs();
  }

  PrefService* pref_service = profile->GetPrefs();
  bool is_google_homepage = google_util::IsGoogleHomePageUrl(
      pref_service->GetString(prefs::kHomePage));

  bool is_google_in_startpages = false;
  SessionStartupPref session_startup_prefs =
      StartupBrowserCreator::GetSessionStartupPref(
          *CommandLine::ForCurrentProcess(), profile);
  if (session_startup_prefs.type == SessionStartupPref::URLS) {
    is_google_in_startpages = std::count_if(session_startup_prefs.urls.begin(),
                                            session_startup_prefs.urls.end(),
                                            IsGoogleUrl) > 0;
  }

  if (!InitRlzDelayed(first_run, send_ping_immediately, delay,
                      is_google_default_search, is_google_homepage,
                      is_google_in_startpages)) {
    return false;
  }

  // Prime the RLZ cache for the home page access point so that its avaiable
  // for the startup page if needed (i.e., when the startup page is set to
  // the home page).
  GetAccessPointRlz(CHROME_HOME_PAGE, NULL);

  return true;
}

bool RLZTracker::Init(bool first_run,
                      bool send_ping_immediately,
                      base::TimeDelta delay,
                      bool is_google_default_search,
                      bool is_google_homepage,
                      bool is_google_in_startpages) {
  first_run_ = first_run;
  is_google_default_search_ = is_google_default_search;
  is_google_homepage_ = is_google_homepage;
  is_google_in_startpages_ = is_google_in_startpages;
  send_ping_immediately_ = send_ping_immediately;

  // Enable zero delays for testing.
  if (CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType))
    EnableZeroDelayForTesting();

  delay = std::min(kMaxInitDelay, std::max(min_init_delay_, delay));

  if (google_util::GetBrand(&brand_) && !IsBrandOrganic(brand_)) {
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
  google_util::GetReactivationBrand(&reactivation_brand_);

  rlz_lib::SetURLRequestContext(g_browser_process->system_request_context());
  ScheduleDelayedInit(delay);

  return true;
}

void RLZTracker::ScheduleDelayedInit(base::TimeDelta delay) {
  // The RLZTracker is a singleton object that outlives any runnable tasks
  // that will be queued up.
  BrowserThread::GetBlockingPool()->PostDelayedSequencedWorkerTask(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&RLZTracker::DelayedInit, base::Unretained(this)),
      delay);
}

void RLZTracker::DelayedInit() {
  bool schedule_ping = false;

  // For organic brandcodes do not use rlz at all. Empty brandcode usually
  // means a chromium install. This is ok.
  if (!IsBrandOrganic(brand_)) {
    RecordProductEvents(first_run_, is_google_default_search_,
                        is_google_homepage_, is_google_in_startpages_,
                        already_ran_, omnibox_used_, homepage_used_);
    schedule_ping = true;
  }

  // If chrome has been reactivated, record the events for this brand
  // as well.
  if (!IsBrandOrganic(reactivation_brand_)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand_.c_str());
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

  if (!IsBrandOrganic(brand_) && SendFinancialPing(brand_, lang, referral)) {
    GoogleUpdateSettings::ClearReferral();

    {
      base::AutoLock lock(cache_lock_);
      rlz_cache_.clear();
    }

    // Prime the RLZ cache for the access points we are interested in.
    GetAccessPointRlz(RLZTracker::CHROME_OMNIBOX, NULL);
    GetAccessPointRlz(RLZTracker::CHROME_HOME_PAGE, NULL);
  }

  if (!IsBrandOrganic(reactivation_brand_)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand_.c_str());
    SendFinancialPing(reactivation_brand_, lang, referral);
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
  if (!reactivation_brand_.empty()) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand_.c_str());
    ret &= rlz_lib::RecordProductEvent(product, point, event_id);
  }

  return ret;
}

bool RLZTracker::ScheduleRecordProductEvent(rlz_lib::Product product,
                                            rlz_lib::AccessPoint point,
                                            rlz_lib::Event event_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return false;

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
    ScheduleDelayedInit(base::TimeDelta());
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

#if defined(OS_CHROMEOS)
// static
void RLZTracker::ClearRlzState() {
  GetInstance()->ClearRlzStateImpl();
}

void RLZTracker::ClearRlzStateImpl() {
  if (ScheduleClearRlzState())
    return;
  rlz_lib::ClearAllProductEvents(rlz_lib::CHROME);
}

bool RLZTracker::ScheduleClearRlzState() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return false;

  BrowserThread::GetBlockingPool()->PostSequencedWorkerTask(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&RLZTracker::ClearRlzStateImpl,
                 base::Unretained(this)));
  return true;
}
#endif

// static
void RLZTracker::CleanupRlz() {
  GetInstance()->rlz_cache_.clear();
  GetInstance()->registrar_.RemoveAll();
}

// static
void RLZTracker::EnableZeroDelayForTesting() {
  GetInstance()->min_init_delay_ = base::TimeDelta();
}
