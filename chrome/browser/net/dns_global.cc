// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_global.h"

#include <map>
#include <string>

#include "base/singleton.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/dns_host_info.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_impl.h"

using base::Time;
using base::TimeDelta;

namespace chrome_browser_net {

static void DnsMotivatedPrefetch(const std::string& hostname,
                                 DnsHostInfo::ResolutionMotivation motivation);
static void DnsPrefetchMotivatedList(
    const NameList& hostnames,
    DnsHostInfo::ResolutionMotivation motivation);

static NameList GetDnsPrefetchHostNamesAtStartup(
    PrefService* user_prefs, PrefService* local_state);

// static
const size_t DnsGlobalInit::kMaxPrefetchConcurrentLookups = 8;

// static
const int DnsGlobalInit::kMaxPrefetchQueueingDelayMs = 500;

//------------------------------------------------------------------------------
// This section contains all the globally accessable API entry points for the
// DNS Prefetching feature.
//------------------------------------------------------------------------------

// Status of prefetch feature, controlling whether any prefetching is done.
static bool dns_prefetch_enabled = true;

// Cached inverted copy of the off_the_record pref.
static bool on_the_record_switch = true;

// Enable/disable Dns prefetch activity (either via command line, or via pref).
void EnableDnsPrefetch(bool enable) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  dns_prefetch_enabled = enable;
}

void OnTheRecord(bool enable) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (on_the_record_switch == enable)
    return;
  on_the_record_switch = enable;
  if (on_the_record_switch)
    g_browser_process->io_thread()->ChangedToOnTheRecord();
}

void RegisterPrefs(PrefService* local_state) {
  local_state->RegisterListPref(prefs::kDnsStartupPrefetchList);
  local_state->RegisterListPref(prefs::kDnsHostReferralList);
}

void RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kDnsPrefetchingEnabled, true);
}

// When enabled, we use the following instance to service all requests in the
// browser process.
// TODO(willchan): Look at killing this.
static DnsMaster* dns_master = NULL;

// This API is only used in the browser process.
// It is called from an IPC message originating in the renderer.  It currently
// includes both Page-Scan, and Link-Hover prefetching.
// TODO(jar): Separate out link-hover prefetching, and page-scan results.
void DnsPrefetchList(const NameList& hostnames) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DnsPrefetchMotivatedList(hostnames, DnsHostInfo::PAGE_SCAN_MOTIVATED);
}

static void DnsPrefetchMotivatedList(
    const NameList& hostnames,
    DnsHostInfo::ResolutionMotivation motivation) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI) ||
         ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!dns_prefetch_enabled || NULL == dns_master)
    return;

  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    dns_master->ResolveList(hostnames, motivation);
  } else {
    ChromeThread::PostTask(
        ChromeThread::IO,
        FROM_HERE,
        NewRunnableMethod(dns_master,
                          &DnsMaster::ResolveList, hostnames, motivation));
  }
}

// This API is used by the autocomplete popup box (where URLs are typed).
void DnsPrefetchUrl(const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!dns_prefetch_enabled || NULL == dns_master)
    return;
  if (url.is_valid())
    DnsMotivatedPrefetch(url.host(), DnsHostInfo::OMNIBOX_MOTIVATED);
}

static void DnsMotivatedPrefetch(const std::string& hostname,
                                 DnsHostInfo::ResolutionMotivation motivation) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!dns_prefetch_enabled || NULL == dns_master || !hostname.size())
    return;

  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableMethod(dns_master,
                        &DnsMaster::Resolve, hostname, motivation));
}

//------------------------------------------------------------------------------
// This section intermingles prefetch results with actual browser HTTP
// network activity.  It supports calculating of the benefit of a prefetch, as
// well as recording what prefetched hostname resolutions might be potentially
// helpful during the next chrome-startup.
//------------------------------------------------------------------------------

// This function determines if there was a saving by prefetching the hostname
// for which the navigation_info is supplied.
static bool AccruePrefetchBenefits(const GURL& referrer,
                                   DnsHostInfo* navigation_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!dns_prefetch_enabled || NULL == dns_master)
    return false;
  return dns_master->AccruePrefetchBenefits(referrer, navigation_info);
}

// When we navigate, we may know in advance some other domains that will need to
// be resolved.  This function initiates those side effects.
static void NavigatingTo(const std::string& host_name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!dns_prefetch_enabled || NULL == dns_master)
    return;
  dns_master->NavigatingTo(host_name);
}

// The observer class needs to connect starts and finishes of HTTP network
// resolutions.  We use the following type for that map.
typedef std::map<int, DnsHostInfo> ObservedResolutionMap;

// There will only be one instance ever created of the following Observer
// class. The PrefetchObserver lives on the IO thread, and intercepts DNS
// resolutions made by the network stack.
class PrefetchObserver : public net::HostResolver::Observer {
 public:
  // net::HostResolver::Observer implementation:
  virtual void OnStartResolution(
      int request_id,
      const net::HostResolver::RequestInfo& request_info);
  virtual void OnFinishResolutionWithStatus(
      int request_id,
      bool was_resolved,
      const net::HostResolver::RequestInfo& request_info);
  virtual void OnCancelResolution(
      int request_id,
      const net::HostResolver::RequestInfo& request_info);

  void DnsGetFirstResolutionsHtml(std::string* output);
  void GetInitialDnsResolutionList(ListValue* startup_list);

 private:
  void StartupListAppend(const DnsHostInfo& navigation_info);

  // Map of pending resolutions seen by observer.
  ObservedResolutionMap resolutions_;
  // List of the first N hostname resolutions observed in this run.
  Results first_resolutions_;
  // The number of hostnames we'll save for prefetching at next startup.
  static const size_t kStartupResolutionCount = 10;
};

// TODO(willchan): Look at killing this global.
static PrefetchObserver* g_prefetch_observer = NULL;

//------------------------------------------------------------------------------
// Member definitions for above Observer class.

void PrefetchObserver::OnStartResolution(
    int request_id,
    const net::HostResolver::RequestInfo& request_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (request_info.is_speculative())
    return;  // One of our own requests.
  DCHECK_NE(0U, request_info.hostname().length());
  DnsHostInfo navigation_info;
  navigation_info.SetHostname(request_info.hostname());
  navigation_info.SetStartedState();

  // This entry will be deleted either by OnFinishResolutionWithStatus(), or
  // by  OnCancelResolution().
  resolutions_[request_id] = navigation_info;
}

void PrefetchObserver::OnFinishResolutionWithStatus(
    int request_id,
    bool was_resolved,
    const net::HostResolver::RequestInfo& request_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (request_info.is_speculative())
    return;  // One of our own requests.
  DnsHostInfo navigation_info;
  size_t startup_count = 0;
  {
    ObservedResolutionMap::iterator it = resolutions_.find(request_id);
    if (resolutions_.end() == it) {
      DCHECK(false);
      return;
    }
    navigation_info = it->second;
    resolutions_.erase(it);
    startup_count = first_resolutions_.size();
  }

  navigation_info.SetFinishedState(was_resolved);  // Get timing info
  AccruePrefetchBenefits(request_info.referrer(), &navigation_info);

  // Handle sub-resource resolutions now that the critical navigational
  // resolution has completed.  This prevents us from in any way delaying that
  // navigational resolution.
  NavigatingTo(request_info.hostname());

  if (kStartupResolutionCount <= startup_count || !was_resolved)
    return;
  // TODO(jar): Don't add host to our list if it is a non-linked lookup, and
  // instead rely on Referrers to pull this in automatically with the enclosing
  // page load (once we start to persist elements of our referrer tree).
  StartupListAppend(navigation_info);
}

void PrefetchObserver::OnCancelResolution(
    int request_id,
    const net::HostResolver::RequestInfo& request_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (request_info.is_speculative())
    return;  // One of our own requests.

  // Remove the entry from |resolutions| that was added by OnStartResolution().
  ObservedResolutionMap::iterator it = resolutions_.find(request_id);
  if (resolutions_.end() == it) {
    DCHECK(false);
    return;
  }
  resolutions_.erase(it);
}

void PrefetchObserver::StartupListAppend(const DnsHostInfo& navigation_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (!on_the_record_switch || NULL == dns_master)
    return;
  if (kStartupResolutionCount <= first_resolutions_.size())
    return;  // Someone just added the last item.
  std::string host_name = navigation_info.hostname();
  if (first_resolutions_.find(host_name) != first_resolutions_.end())
    return;  // We already have this hostname listed.
  first_resolutions_[host_name] = navigation_info;
}

void PrefetchObserver::GetInitialDnsResolutionList(ListValue* startup_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(startup_list);
  startup_list->Clear();
  DCHECK_EQ(0u, startup_list->GetSize());
  for (Results::iterator it = first_resolutions_.begin();
       it != first_resolutions_.end();
       it++) {
    const std::string hostname = it->first;
    startup_list->Append(Value::CreateStringValue(hostname));
  }
}

void PrefetchObserver::DnsGetFirstResolutionsHtml(std::string* output) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  DnsHostInfo::DnsInfoTable resolution_list;
  {
    for (Results::iterator it(first_resolutions_.begin());
         it != first_resolutions_.end();
         it++) {
      resolution_list.push_back(it->second);
    }
  }
  DnsHostInfo::GetHtmlTable(resolution_list,
      "Future startups will prefetch DNS records for ", false, output);
}

//------------------------------------------------------------------------------
// Support observer to detect opening and closing of OffTheRecord windows.
// This object lives on the UI thread.

class OffTheRecordObserver : public NotificationObserver {
 public:
  void Register() {
    // TODO(pkasting): This test should not be necessary.  See crbug.com/12475.
    if (registrar_.IsEmpty()) {
      registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                     NotificationService::AllSources());
      registrar_.Add(this, NotificationType::BROWSER_OPENED,
                     NotificationService::AllSources());
    }
  }

  void Observe(NotificationType type, const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::BROWSER_OPENED:
        if (!Source<Browser>(source)->profile()->IsOffTheRecord())
          break;
        ++count_off_the_record_windows_;
        OnTheRecord(false);
        break;

      case NotificationType::BROWSER_CLOSED:
        if (!Source<Browser>(source)->profile()->IsOffTheRecord())
        break;  // Ignore ordinary windows.
        DCHECK_LT(0, count_off_the_record_windows_);
        if (0 >= count_off_the_record_windows_)  // Defensive coding.
          break;
        if (--count_off_the_record_windows_)
          break;  // Still some windows are incognito.
        OnTheRecord(true);
        break;

      default:
        break;
    }
  }

 private:
  friend struct DefaultSingletonTraits<OffTheRecordObserver>;
  OffTheRecordObserver() : count_off_the_record_windows_(0) {}
  ~OffTheRecordObserver() {}

  NotificationRegistrar registrar_;
  int count_off_the_record_windows_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordObserver);
};

//------------------------------------------------------------------------------
// This section supports the about:dns page.
//------------------------------------------------------------------------------

// Provide global support for the about:dns page.
void DnsPrefetchGetHtmlInfo(std::string* output) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  output->append("<html><head><title>About DNS</title>"
                 // We'd like the following no-cache... but it doesn't work.
                 // "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
                 "</head><body>");
  if (!dns_prefetch_enabled  || NULL == dns_master) {
    output->append("Dns Prefetching is disabled.");
  } else {
    if (!on_the_record_switch) {
      output->append("Incognito mode is active in a window.");
    } else {
      dns_master->GetHtmlInfo(output);
      g_prefetch_observer->DnsGetFirstResolutionsHtml(output);
      dns_master->GetHtmlReferrerLists(output);
    }
  }
  output->append("</body></html>");
}

//------------------------------------------------------------------------------
// This section intializes global DNS prefetch services.
//------------------------------------------------------------------------------

static void InitDnsPrefetch(TimeDelta max_queue_delay, size_t max_concurrent,
                            PrefService* user_prefs, PrefService* local_state) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  bool prefetching_enabled =
      user_prefs->GetBoolean(prefs::kDnsPrefetchingEnabled);

  // Gather the list of hostnames to prefetch on startup.
  NameList hostnames =
      GetDnsPrefetchHostNamesAtStartup(user_prefs, local_state);

  ListValue* referral_list =
      static_cast<ListValue*>(
          local_state->GetMutableList(prefs::kDnsHostReferralList)->DeepCopy());

  g_browser_process->io_thread()->InitDnsMaster(
      prefetching_enabled, max_queue_delay, max_concurrent, hostnames,
      referral_list);
}

void FinalizeDnsPrefetchInitialization(
    DnsMaster* global_dns_master,
    net::HostResolver::Observer* global_prefetch_observer,
    const NameList& hostnames_to_prefetch,
    ListValue* referral_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  dns_master = global_dns_master;
  g_prefetch_observer =
      static_cast<PrefetchObserver*>(global_prefetch_observer);

  DLOG(INFO) << "DNS Prefetch service started";

  // Prefetch these hostnames on startup. 
  DnsPrefetchMotivatedList(hostnames_to_prefetch,
                           DnsHostInfo::STARTUP_LIST_MOTIVATED);
  dns_master->DeserializeReferrersThenDelete(referral_list);
}

void FreeDnsPrefetchResources() {
  dns_master = NULL;
  g_prefetch_observer = NULL;
}

//------------------------------------------------------------------------------

net::HostResolver::Observer* CreatePrefetchObserver() {
  return new PrefetchObserver();
}

//------------------------------------------------------------------------------
// Functions to handle saving of hostnames from one session to the next, to
// expedite startup times.

static void SaveDnsPrefetchStateForNextStartupAndTrimOnIOThread(
    ListValue* startup_list,
    ListValue* referral_list,
    base::WaitableEvent* completion) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (NULL == dns_master) {
    completion->Signal();
    return;
  }

  g_prefetch_observer->GetInitialDnsResolutionList(startup_list);

  // TODO(jar): Trimming should be done more regularly, such as every 48 hours
  // of physical time, or perhaps after 48 hours of running (excluding time
  // between sessions possibly).
  // For now, we'll just trim at shutdown.
  dns_master->TrimReferrers();
  dns_master->SerializeReferrers(referral_list);

  completion->Signal();
}

void SaveDnsPrefetchStateForNextStartupAndTrim(PrefService* prefs) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (!dns_prefetch_enabled || dns_master == NULL)
    return;

  base::WaitableEvent completion(true, false);

  bool posted = ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableFunction(SaveDnsPrefetchStateForNextStartupAndTrimOnIOThread,
                          prefs->GetMutableList(prefs::kDnsStartupPrefetchList),
                          prefs->GetMutableList(prefs::kDnsHostReferralList),
                          &completion));

  DCHECK(posted);
  if (posted)
    completion.Wait();
}

static NameList GetDnsPrefetchHostNamesAtStartup(PrefService* user_prefs,
                                                 PrefService* local_state) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  NameList hostnames;
  // Prefetch DNS for hostnames we learned about during last session.
  // This may catch secondary hostnames, pulled in by the homepages.  It will
  // also catch more of the "primary" home pages, since that was (presumably)
  // rendered first (and will be rendered first this time too).
  ListValue* startup_list =
      local_state->GetMutableList(prefs::kDnsStartupPrefetchList);
  if (startup_list) {
    for (ListValue::iterator it = startup_list->begin();
         it != startup_list->end();
         it++) {
      std::string hostname;
      (*it)->GetAsString(&hostname);
      hostnames.push_back(hostname);
    }
  }

  // Prepare for any static home page(s) the user has in prefs.  The user may
  // have a LOT of tab's specified, so we may as well try to warm them all.
  SessionStartupPref tab_start_pref =
      SessionStartupPref::GetStartupPref(user_prefs);
  if (SessionStartupPref::URLS == tab_start_pref.type) {
    for (size_t i = 0; i < tab_start_pref.urls.size(); i++) {
      GURL gurl = tab_start_pref.urls[i];
      if (gurl.is_valid() && !gurl.host().empty())
        hostnames.push_back(gurl.host());
    }
  }

  if (hostnames.empty())
    hostnames.push_back("www.google.com");

  return hostnames;
}

//------------------------------------------------------------------------------
// Methods for the helper class that is used to startup and teardown the whole
// DNS prefetch system.

DnsGlobalInit::DnsGlobalInit(PrefService* user_prefs,
                             PrefService* local_state) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // Set up a field trial to see what disabling DNS pre-resolution does to
  // latency of page loads.
  FieldTrial::Probability kDivisor = 100;
  // For each option (i.e., non-default), we have a fixed probability.
  FieldTrial::Probability kProbabilityPerGroup = 3;  // 3% probability.

  trial_ = new FieldTrial("DnsImpact", kDivisor);

  // First option is to disable prefetching completely.
  int disabled_prefetch = trial_->AppendGroup("_disabled_prefetch",
                                              kProbabilityPerGroup);
  // Set congestion detection at 250, 500, or 750ms, rather than the 1 second
  // default.
  int max_250ms_prefetch = trial_->AppendGroup("_max_250ms_queue_prefetch",
                                               kProbabilityPerGroup);
  int max_500ms_prefetch = trial_->AppendGroup("_max_500ms_queue_prefetch",
                                               kProbabilityPerGroup);
  int max_750ms_prefetch = trial_->AppendGroup("_max_750ms_queue_prefetch",
                                               kProbabilityPerGroup);
  // Set congestion detection at 2 seconds instead of the 1 second default.
  int max_2s_prefetch = trial_->AppendGroup("_max_2s_queue_prefetch",
                                            kProbabilityPerGroup);

  trial_->AppendGroup("_default_enabled_prefetch",
      FieldTrial::kAllRemainingProbability);

  // We will register the incognito observer regardless of whether prefetching
  // is enabled, as it is also used to clear the host cache.
  Singleton<OffTheRecordObserver>::get()->Register();

  if (trial_->group() != disabled_prefetch) {
    // Initialize the DNS prefetch system.

    size_t max_concurrent = kMaxPrefetchConcurrentLookups;

    int max_queueing_delay_ms = kMaxPrefetchQueueingDelayMs;

    if (trial_->group() == max_250ms_prefetch)
      max_queueing_delay_ms = 250;
    else if (trial_->group() == max_500ms_prefetch)
      max_queueing_delay_ms = 500;
    else if (trial_->group() == max_750ms_prefetch)
      max_queueing_delay_ms = 750;
    else if (trial_->group() == max_2s_prefetch)
      max_queueing_delay_ms = 2000;

    TimeDelta max_queueing_delay(
        TimeDelta::FromMilliseconds(max_queueing_delay_ms));

    DCHECK(!dns_master);
    InitDnsPrefetch(max_queueing_delay, max_concurrent, user_prefs,
                    local_state);
  }
}

}  // namespace chrome_browser_net
