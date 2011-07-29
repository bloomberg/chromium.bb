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
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"

namespace {

enum {
  ACCESS_VALUES_STALE,      // Possibly new values available.
  ACCESS_VALUES_FRESH       // The cached values are current.
};

// Tracks if we have tried and succeeded sending the ping. This helps us
// decide if we need to refresh the cached RLZ string.
volatile int access_values_state = ACCESS_VALUES_STALE;
base::Lock rlz_lock;

bool SendFinancialPing(const std::wstring& brand, const std::wstring& lang,
                       const std::wstring& referral, bool exclude_id) {
  rlz_lib::AccessPoint points[] = {rlz_lib::CHROME_OMNIBOX,
                                   rlz_lib::CHROME_HOME_PAGE,
                                   rlz_lib::NO_ACCESS_POINT};
  std::string brand_ascii(WideToASCII(brand));
  std::string lang_ascii(WideToASCII(lang));
  std::string referral_ascii(WideToASCII(referral));

  // If chrome has been reactivated, send a ping for this brand as well.
  // We ignore the return value of SendFinancialPing() since we'll try again
  // later anyway.  Callers of this function are only interested in whether
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

// This class leverages the AutocompleteEditModel notification to know when
// the user first interacted with the omnibox and set a global accordingly.
class OmniBoxUsageObserver : public NotificationObserver {
 public:
  OmniBoxUsageObserver(bool first_run, bool send_ping_immediately,
                       bool google_default_search)
    : first_run_(first_run),
      send_ping_immediately_(send_ping_immediately),
      google_default_search_(google_default_search) {
    registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                   NotificationService::AllSources());
    // If instant is enabled we'll start searching as soon as the user starts
    // typing in the omnibox (which triggers INSTANT_CONTROLLER_UPDATED).
    registrar_.Add(this, chrome::NOTIFICATION_INSTANT_CONTROLLER_UPDATED,
                   NotificationService::AllSources());
    omnibox_used_ = false;
    DCHECK(!instance_);
    instance_ = this;
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  static bool used() {
    return omnibox_used_;
  }

  // Deletes the single instance of OmniBoxUsageObserver.
  static void DeleteInstance() {
    delete instance_;
  }

 private:
  // Dtor is private so the object cannot be created on the stack.
  ~OmniBoxUsageObserver() {
    instance_ = NULL;
  }

  static bool omnibox_used_;

  // There should only be one instance created at a time, and instance_ points
  // to that instance.
  // NOTE: this is only non-null for the amount of time it is needed. Once the
  // instance_ is no longer needed (or Chrome is exiting), this is null.
  static OmniBoxUsageObserver* instance_;

  NotificationRegistrar registrar_;
  bool first_run_;
  bool send_ping_immediately_;
  bool google_default_search_;
};

bool OmniBoxUsageObserver::omnibox_used_ = false;
OmniBoxUsageObserver* OmniBoxUsageObserver::instance_ = NULL;

// This task is run in the file thread, so to not block it for a long time
// we use a throwaway thread to do the blocking url request.
class DailyPingTask : public Task {
 public:
  virtual ~DailyPingTask() {
  }
  virtual void Run() {
    // We use a transient thread because we have no guarantees about
    // how long the RLZ lib can block us.
    _beginthread(PingNow, 0, NULL);
  }

 private:
  // Causes a ping to the server using WinInet.
  static void _cdecl PingNow(void*) {
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
      base::AutoLock lock(rlz_lock);
      access_values_state = ACCESS_VALUES_STALE;
      GoogleUpdateSettings::ClearReferral();
    }
  }

  // Organic brands all start with GG, such as GGCM.
  static bool is_organic(const std::wstring& brand) {
    return (brand.size() < 2) ? false : (brand.substr(0, 2) == L"GG");
  }
};

// Performs late RLZ initialization and RLZ event recording for chrome.
// This task needs to run on the UI thread.
class DelayedInitTask : public Task {
 public:
  DelayedInitTask(bool first_run, bool google_default_search)
      : first_run_(first_run),
        google_default_search_(google_default_search) {
  }
  virtual ~DelayedInitTask() {
  }
  virtual void Run() {
    // For non-interactive tests we don't do the rest of the initialization
    // because sometimes the very act of loading the dll causes QEMU to crash.
    if (::GetEnvironmentVariableW(ASCIIToWide(env_vars::kHeadless).c_str(),
                                  NULL, 0)) {
      return;
    }
    // For organic brandcodes do not use rlz at all. Empty brandcode usually
    // means a chromium install. This is ok.
    std::wstring brand;
    if (!GoogleUpdateSettings::GetBrand(&brand) || brand.empty() ||
        GoogleUpdateSettings::IsOrganic(brand))
      return;

    RecordProductEvents(first_run_, google_default_search_, already_ran_);

    // If chrome has been reactivated, record the events for this brand
    // as well.
    std::wstring reactivation_brand;
    if (GoogleUpdateSettings::GetReactivationBrand(&reactivation_brand)) {
      rlz_lib::SupplementaryBranding branding(reactivation_brand.c_str());
      RecordProductEvents(first_run_, google_default_search_, already_ran_);
    }

    already_ran_ = true;

    // Schedule the daily RLZ ping.
    MessageLoop::current()->PostTask(FROM_HERE, new DailyPingTask());
  }

 private:
  static void RecordProductEvents(bool first_run, bool google_default_search,
                                  bool already_ran) {
    // Record the installation of chrome. We call this all the time but the rlz
    // lib should ingore all but the first one.
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                rlz_lib::CHROME_OMNIBOX,
                                rlz_lib::INSTALL);
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                rlz_lib::CHROME_HOME_PAGE,
                                rlz_lib::INSTALL);

    // Do the initial event recording if is the first run or if we have an
    // empty rlz which means we haven't got a chance to do it.
    char omnibox_rlz[rlz_lib::kMaxRlzLength + 1];
    if (!rlz_lib::GetAccessPointRlz(rlz_lib::CHROME_OMNIBOX, omnibox_rlz,
                                    rlz_lib::kMaxRlzLength, NULL)) {
      omnibox_rlz[0] = 0;
    }

    // Record if google is the initial search provider.
    if ((first_run || omnibox_rlz[0] == 0) && google_default_search &&
        !already_ran) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  rlz_lib::CHROME_OMNIBOX,
                                  rlz_lib::SET_TO_GOOGLE);
    }

    // Record first user interaction with the omnibox. We call this all the
    // time but the rlz lib should ingore all but the first one.
    if (OmniBoxUsageObserver::used()) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  rlz_lib::CHROME_OMNIBOX,
                                  rlz_lib::FIRST_SEARCH);
    }
  }

  // Flag that remembers if the delayed task already ran or not.  This is
  // needed only in the first_run case, since we don't want to record the
  // set-to-google event more than once.  We need to worry about this event
  // (and not the others) because it is not a stateful RLZ event.
  static bool already_ran_;

  bool first_run_;

  // True if Google is the default search engine for the first profile starting
  // in a browser during first run.
  bool google_default_search_;

};

bool DelayedInitTask::already_ran_ = false;

void OmniBoxUsageObserver::Observe(int type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  // Needs to be evaluated. See http://crbug.com/62328.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Try to record event now, else set the flag to try later when we
  // attempt the ping.
  if (!RLZTracker::RecordProductEvent(rlz_lib::CHROME,
                                      rlz_lib::CHROME_OMNIBOX,
                                      rlz_lib::FIRST_SEARCH))
    omnibox_used_ = true;
  else if (send_ping_immediately_) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE, new DelayedInitTask(first_run_,
            google_default_search_));
  }

  delete this;
}

}  // namespace

bool RLZTracker::InitRlzDelayed(bool first_run, int delay,
                                bool google_default_search) {
  // A negative delay means that a financial ping should be sent immediately
  // after a first search is recorded, without waiting for the next restart
  // of chrome.  However, we only want this behaviour on first run.
  bool send_ping_immediately = false;
  if (delay < 0) {
    send_ping_immediately = true;
    delay = -delay;
  }

  // Maximum and minimum delay we would allow to be set through master
  // preferences. Somewhat arbitrary, may need to be adjusted in future.
  const int kMaxDelay = 200 * 1000;
  const int kMinDelay = 20 * 1000;

  delay *= 1000;
  delay = (delay < kMinDelay) ? kMinDelay : delay;
  delay = (delay > kMaxDelay) ? kMaxDelay : delay;

  if (!OmniBoxUsageObserver::used())
    new OmniBoxUsageObserver(first_run, send_ping_immediately,
                             google_default_search);

  // Schedule the delayed init items.
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE,
      FROM_HERE,
      new DelayedInitTask(first_run, google_default_search),
      delay);
  return true;
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

// We implement caching of the answer of get_access_point() if the request
// is for CHROME_OMNIBOX. If we had a successful ping, then we update the
// cached value.

bool RLZTracker::GetAccessPointRlz(rlz_lib::AccessPoint point,
                                   std::wstring* rlz) {
  static std::wstring cached_ommibox_rlz;
  if (rlz_lib::CHROME_OMNIBOX == point) {
    base::AutoLock lock(rlz_lock);
    if (access_values_state == ACCESS_VALUES_FRESH) {
      *rlz = cached_ommibox_rlz;
      return true;
    }
  }

  // Make sure we don't access disk outside of the file context.
  // In such case we repost the task on the right thread and return error.
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    // Caching of access points is now only implemented for the CHROME_OMNIBOX.
    // Thus it is not possible to call this function on another thread for
    // other access points until proper caching for these has been implemented
    // and the code that calls this function can handle synchronous fetching
    // of the access point.
    DCHECK_EQ(rlz_lib::CHROME_OMNIBOX, point);

    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(&RLZTracker::GetAccessPointRlz,
                            point, &cached_ommibox_rlz));
      rlz->erase();
      return false;
  }

  char str_rlz[rlz_lib::kMaxRlzLength + 1];
  if (!rlz_lib::GetAccessPointRlz(point, str_rlz, rlz_lib::kMaxRlzLength, NULL))
    return false;
  *rlz = ASCIIToWide(std::string(str_rlz));
  if (rlz_lib::CHROME_OMNIBOX == point) {
    base::AutoLock lock(rlz_lock);
    cached_ommibox_rlz.assign(*rlz);
    access_values_state = ACCESS_VALUES_FRESH;
  }
  return true;
}

// static
void RLZTracker::CleanupRlz() {
  OmniBoxUsageObserver::DeleteInstance();
}
