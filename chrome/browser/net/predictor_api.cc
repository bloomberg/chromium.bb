// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/predictor_api.h"

#include <map>
#include <string>

#include "base/singleton.h"
#include "base/stats_counters.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/url_info.h"
#include "chrome/browser/net/preconnect.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_impl.h"

using base::Time;
using base::TimeDelta;

namespace chrome_browser_net {

static void DnsMotivatedPrefetch(const GURL& url,
    UrlInfo::ResolutionMotivation motivation);

static void DnsPrefetchMotivatedList(const UrlList& urls,
    UrlInfo::ResolutionMotivation motivation);

static UrlList GetPredictedUrlListAtStartup(
    PrefService* user_prefs, PrefService* local_state);

// static
const size_t PredictorInit::kMaxPrefetchConcurrentLookups = 8;

// static
const int PredictorInit::kMaxPrefetchQueueingDelayMs = 500;

// A version number for prefs that are saved. This should be incremented when
// we change the format so that we discard old data.
static const int kPredictorStartupFormatVersion = 1;

//------------------------------------------------------------------------------
// Static helper functions
//------------------------------------------------------------------------------

// Put URL in canonical form, including a scheme, host, and port.
// Returns GURL::EmptyGURL() if the scheme is not http/https or if the url
// cannot be otherwise canonicalized.
static GURL CanonicalizeUrl(const GURL& url) {
  if (!url.has_host())
     return GURL::EmptyGURL();

  std::string scheme;
  if (url.has_scheme()) {
    scheme = url.scheme();
    if (scheme != "http" && scheme != "https")
      return GURL::EmptyGURL();
    if (url.has_port())
      return url.GetWithEmptyPath();
  } else {
    scheme = "http";
  }

  // If we omit a port, it will default to 80 or 443 as appropriate.
  std::string colon_plus_port;
  if (url.has_port())
    colon_plus_port = ":" + url.port();

  return GURL(scheme + "://" + url.host() + colon_plus_port);
}

//------------------------------------------------------------------------------
// This section contains all the globally accessable API entry points for the
// DNS Prefetching feature.
//------------------------------------------------------------------------------

// Status of prefetch feature, controlling whether any prefetching is done.
static bool dns_prefetch_enabled = true;

// Cached inverted copy of the off_the_record pref.
static bool on_the_record_switch = true;

// Enable/disable Dns prefetch activity (either via command line, or via pref).
void EnablePredictor(bool enable) {
  // NOTE: this is invoked on the UI thread.
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
static Predictor* predictor = NULL;

// This API is only used in the browser process.
// It is called from an IPC message originating in the renderer.  It currently
// includes both Page-Scan, and Link-Hover prefetching.
// TODO(jar): Separate out link-hover prefetching, and page-scan results.
void DnsPrefetchList(const NameList& hostnames) {
  // TODO(jar): Push GURL transport further back into renderer.
  UrlList urls;
  for (NameList::const_iterator it = hostnames.begin();
       it < hostnames.end();
       ++it) {
    urls.push_back(GURL("http://" + *it + ":80"));
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DnsPrefetchMotivatedList(urls, UrlInfo::PAGE_SCAN_MOTIVATED);
}

static void DnsPrefetchMotivatedList(
    const UrlList& urls,
    UrlInfo::ResolutionMotivation motivation) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI) ||
         ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!dns_prefetch_enabled || NULL == predictor)
    return;

  if (ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    predictor->ResolveList(urls, motivation);
  } else {
    ChromeThread::PostTask(
        ChromeThread::IO,
        FROM_HERE,
        NewRunnableMethod(predictor,
                          &Predictor::ResolveList, urls, motivation));
  }
}

// This API is used by the autocomplete popup box (where URLs are typed).
void AnticipateUrl(const GURL& url, bool preconnectable) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!dns_prefetch_enabled || NULL == predictor)
    return;
  if (!url.is_valid())
    return;

  static std::string last_host;
  std::string host = url.HostNoBrackets();
  bool is_new_host_request = (host != last_host);
  last_host = host;

  // Omnibox tends to call in pairs (just a few milliseconds apart), and we
  // really don't need to keep resolving a name that often.
  // TODO(jar): A/B tests could check for perf impact of the early returns.
  static base::TimeTicks last_prefetch_for_host;
  base::TimeTicks now = base::TimeTicks::Now();
  if (!is_new_host_request) {
    const int kMinPreresolveSeconds(10);
    if (kMinPreresolveSeconds > (now - last_prefetch_for_host).InSeconds())
      return;
  }
  last_prefetch_for_host = now;

  GURL canonical_url(CanonicalizeUrl(url));

  if (predictor->preconnect_enabled() && preconnectable) {
    static base::TimeTicks last_keepalive;
    // TODO(jar): The wild guess of 30 seconds could be tuned/tested, but it
    // currently is just a guess that most sockets will remain open for at least
    // 30 seconds.
    const int kMaxSearchKeepaliveSeconds(30);
    if ((now - last_keepalive).InSeconds() < kMaxSearchKeepaliveSeconds)
      return;
    last_keepalive = now;

    if (Preconnect::PreconnectOnUIThread(canonical_url))
      return;  // Skip pre-resolution, since we'll open a connection.
  }

  // Perform at least DNS pre-resolution.
  DnsMotivatedPrefetch(canonical_url, UrlInfo::OMNIBOX_MOTIVATED);
}

static void DnsMotivatedPrefetch(const GURL& url,
                                 UrlInfo::ResolutionMotivation motivation) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!dns_prefetch_enabled || NULL == predictor || !url.has_host())
    return;

  ChromeThread::PostTask(
      ChromeThread::IO,
      FROM_HERE,
      NewRunnableMethod(predictor,
                        &Predictor::Resolve, url, motivation));
}

//------------------------------------------------------------------------------
// This section intermingles prefetch results with actual browser HTTP
// network activity.  It supports calculating of the benefit of a prefetch, as
// well as recording what prefetched hostname resolutions might be potentially
// helpful during the next chrome-startup.
//------------------------------------------------------------------------------

// This function determines if there was a saving by prefetching the hostname
// for which the navigation_info is supplied.
static bool AccruePrefetchBenefits(const GURL& referrer_url,
                                   UrlInfo* navigation_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!dns_prefetch_enabled || NULL == predictor)
    return false;
  DCHECK(referrer_url == referrer_url.GetWithEmptyPath());
  return predictor->AccruePrefetchBenefits(referrer_url, navigation_info);
}

void PredictSubresources(const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!dns_prefetch_enabled || NULL == predictor)
    return;
  predictor->PredictSubresources(url);
}

void PredictFrameSubresources(const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!dns_prefetch_enabled || NULL == predictor)
    return;
  predictor->PredictFrameSubresources(url);
}

void LearnFromNavigation(const GURL& referring_url, const GURL& target_url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!dns_prefetch_enabled || NULL == predictor)
    return;
  predictor->LearnFromNavigation(referring_url, target_url);
}

// The observer class needs to connect starts and finishes of HTTP network
// resolutions.  We use the following type for that map.
typedef std::map<int, UrlInfo> ObservedResolutionMap;

// There will only be one instance ever created of the following Observer
// class. The PrefetchObserver lives on the IO thread, and intercepts DNS
// resolutions made by the network stack.
class PrefetchObserver : public net::HostResolver::Observer {
 public:
  typedef std::map<GURL, UrlInfo> FirstResolutionMap;

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
  void StartupListAppend(const UrlInfo& navigation_info);

  // Map of pending resolutions seen by observer.
  ObservedResolutionMap resolutions_;
  // List of the first N hostname resolutions observed in this run.
  FirstResolutionMap first_resolutions_;
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
  if (!request_info.hostname().length())
    return;  // PAC scripts may create queries without a hostname.

  UrlInfo navigation_info;
  // TODO(jar): Remove hack which guestimates ssl via port number, and perhaps
  // have actual URL passed down in request_info instead.
  bool is_ssl(443 == request_info.port());
  std::string url_spec = is_ssl ? "https://" : "http://";
  url_spec += request_info.hostname();
  url_spec += ":";
  url_spec += IntToString(request_info.port());
  navigation_info.SetUrl(GURL(url_spec));
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
  if (!request_info.hostname().length())
    return;  // PAC scripts may create queries without a hostname.
  UrlInfo navigation_info;
  size_t startup_count = 0;
  {
    ObservedResolutionMap::iterator it = resolutions_.find(request_id);
    if (resolutions_.end() == it) {
      NOTREACHED();
      return;
    }
    navigation_info = it->second;
    resolutions_.erase(it);
    startup_count = first_resolutions_.size();
  }

  navigation_info.SetFinishedState(was_resolved);  // Get timing info
  AccruePrefetchBenefits(CanonicalizeUrl(request_info.referrer()),
                         &navigation_info);

  // Handle sub-resource resolutions now that the critical navigational
  // resolution has completed.  This prevents us from in any way delaying that
  // navigational resolution.
  std::string url_spec;
  StringAppendF(&url_spec, "http%s://%s:%d", "",
                request_info.hostname().c_str(), request_info.port());
  PredictSubresources(GURL(url_spec));

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
  if (!request_info.hostname().length())
    return;  // PAC scripts may create queries without a hostname.

  // Remove the entry from |resolutions| that was added by OnStartResolution().
  ObservedResolutionMap::iterator it = resolutions_.find(request_id);
  if (resolutions_.end() == it) {
    NOTREACHED();
    return;
  }
  resolutions_.erase(it);
}

void PrefetchObserver::StartupListAppend(const UrlInfo& navigation_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (!on_the_record_switch || NULL == predictor)
    return;
  if (kStartupResolutionCount <= first_resolutions_.size())
    return;  // Someone just added the last item.
  if (ContainsKey(first_resolutions_, navigation_info.url()))
    return;  // We already have this hostname listed.
  first_resolutions_[navigation_info.url()] = navigation_info;
}

void PrefetchObserver::GetInitialDnsResolutionList(ListValue* startup_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(startup_list);
  startup_list->Clear();
  DCHECK_EQ(0u, startup_list->GetSize());
  startup_list->Append(new FundamentalValue(kPredictorStartupFormatVersion));
  for (FirstResolutionMap::iterator it = first_resolutions_.begin();
       it != first_resolutions_.end();
       ++it) {
    DCHECK(it->first == CanonicalizeUrl(it->first));
    startup_list->Append(new StringValue(it->first.spec()));
  }
}

void PrefetchObserver::DnsGetFirstResolutionsHtml(std::string* output) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  UrlInfo::DnsInfoTable resolution_list;
  {
    for (FirstResolutionMap::iterator it(first_resolutions_.begin());
         it != first_resolutions_.end();
         it++) {
      resolution_list.push_back(it->second);
    }
  }
  UrlInfo::GetHtmlTable(resolution_list,
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
void PredictorGetHtmlInfo(std::string* output) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  output->append("<html><head><title>About DNS</title>"
                 // We'd like the following no-cache... but it doesn't work.
                 // "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
                 "</head><body>");
  if (!dns_prefetch_enabled  || NULL == predictor) {
    output->append("Dns Prefetching is disabled.");
  } else {
    if (!on_the_record_switch) {
      output->append("Incognito mode is active in a window.");
    } else {
      predictor->GetHtmlInfo(output);
      if (g_prefetch_observer)
        g_prefetch_observer->DnsGetFirstResolutionsHtml(output);
      predictor->GetHtmlReferrerLists(output);
    }
  }
  output->append("</body></html>");
}

//------------------------------------------------------------------------------
// This section intializes global DNS prefetch services.
//------------------------------------------------------------------------------

static void InitNetworkPredictor(TimeDelta max_dns_queue_delay,
    size_t max_concurrent, PrefService* user_prefs, PrefService* local_state,
    bool preconnect_enabled) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  bool prefetching_enabled =
      user_prefs->GetBoolean(prefs::kDnsPrefetchingEnabled);

  // Gather the list of hostnames to prefetch on startup.
  UrlList urls =
      GetPredictedUrlListAtStartup(user_prefs, local_state);

  ListValue* referral_list =
      static_cast<ListValue*>(
          local_state->GetMutableList(prefs::kDnsHostReferralList)->DeepCopy());

  g_browser_process->io_thread()->InitNetworkPredictor(
      prefetching_enabled, max_dns_queue_delay, max_concurrent, urls,
      referral_list, preconnect_enabled);
}

void FinalizePredictorInitialization(
    Predictor* global_predictor,
    net::HostResolver::Observer* global_prefetch_observer,
    const UrlList& startup_urls,
    ListValue* referral_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  predictor = global_predictor;
  g_prefetch_observer =
      static_cast<PrefetchObserver*>(global_prefetch_observer);

  DLOG(INFO) << "DNS Prefetch service started";

  // Prefetch these hostnames on startup.
  DnsPrefetchMotivatedList(startup_urls,
                           UrlInfo::STARTUP_LIST_MOTIVATED);
  predictor->DeserializeReferrersThenDelete(referral_list);
}

void FreePredictorResources() {
  predictor = NULL;
  g_prefetch_observer = NULL;
}

//------------------------------------------------------------------------------

net::HostResolver::Observer* CreateResolverObserver() {
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

  if (NULL == predictor) {
    completion->Signal();
    return;
  }

  if (g_prefetch_observer)
    g_prefetch_observer->GetInitialDnsResolutionList(startup_list);

  // TODO(jar): Trimming should be done more regularly, such as every 48 hours
  // of physical time, or perhaps after 48 hours of running (excluding time
  // between sessions possibly).
  // For now, we'll just trim at shutdown.
  predictor->TrimReferrers();
  predictor->SerializeReferrers(referral_list);

  completion->Signal();
}

void SavePredictorStateForNextStartupAndTrim(PrefService* prefs) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (!dns_prefetch_enabled || predictor == NULL)
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

static UrlList GetPredictedUrlListAtStartup(PrefService* user_prefs,
                                            PrefService* local_state) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  UrlList urls;
  // Recall list of URLs we learned about during last session.
  // This may catch secondary hostnames, pulled in by the homepages.  It will
  // also catch more of the "primary" home pages, since that was (presumably)
  // rendered first (and will be rendered first this time too).
  ListValue* startup_list =
      local_state->GetMutableList(prefs::kDnsStartupPrefetchList);
  if (startup_list) {
    ListValue::iterator it = startup_list->begin();
    int format_version = -1;
    if (it != startup_list->end() &&
        (*it)->GetAsInteger(&format_version) &&
        format_version == kPredictorStartupFormatVersion) {
      ++it;
      for (; it != startup_list->end(); ++it) {
        std::string url_spec;
        if (!(*it)->GetAsString(&url_spec)) {
          LOG(DFATAL);
          break;  // Format incompatibility.
        }
        GURL url(url_spec);
        if (!url.has_host() || !url.has_scheme()) {
          LOG(DFATAL);
          break;  // Format incompatibility.
        }

        urls.push_back(url);
      }
    }
  }

  // Prepare for any static home page(s) the user has in prefs.  The user may
  // have a LOT of tab's specified, so we may as well try to warm them all.
  SessionStartupPref tab_start_pref =
      SessionStartupPref::GetStartupPref(user_prefs);
  if (SessionStartupPref::URLS == tab_start_pref.type) {
    for (size_t i = 0; i < tab_start_pref.urls.size(); i++) {
      GURL gurl = tab_start_pref.urls[i];
      if (!gurl.is_valid() || gurl.SchemeIsFile() || gurl.host().empty())
        continue;
      if (gurl.SchemeIs("http") || gurl.SchemeIs("https"))
        urls.push_back(gurl.GetWithEmptyPath());
    }
  }

  if (urls.empty())
    urls.push_back(GURL("http://www.google.com:80"));

  return urls;
}

//------------------------------------------------------------------------------
// Methods for the helper class that is used to startup and teardown the whole
// predictor system (both DNS pre-resolution and TCP/IP connection prewarming).

PredictorInit::PredictorInit(PrefService* user_prefs,
                             PrefService* local_state,
                             bool preconnect_enabled,
                             bool proconnect_despite_proxy) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // Set up a field trial to see what disabling DNS pre-resolution does to
  // latency of page loads.
  FieldTrial::Probability kDivisor = 1000;
  // For each option (i.e., non-default), we have a fixed probability.
  FieldTrial::Probability kProbabilityPerGroup = 1;  // 0.1% probability.

  trial_ = new FieldTrial("DnsImpact", kDivisor);

  // First option is to disable prefetching completely.
  int disabled_prefetch = trial_->AppendGroup("_disabled_prefetch",
                                              kProbabilityPerGroup);


  // We're running two experiments at the same time.  The first set of trials
  // modulates the delay-time until we declare a congestion event (and purge
  // our queue).  The second modulates the number of concurrent resolutions
  // we do at any time.  Users are in exactly one trial (or the default) during
  // any one run, and hence only one experiment at a time.
  // Experiment 1:
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
  // Experiment 2:
  // Set max simultaneous resoultions to 2, 4, or 6, and scale the congestion
  // limit proportionally (so we don't impact average probability of asserting
  // congesion very much).
  int max_2_concurrent_prefetch = trial_->AppendGroup(
        "_max_2 concurrent_prefetch", kProbabilityPerGroup);
  int max_4_concurrent_prefetch = trial_->AppendGroup(
        "_max_4 concurrent_prefetch", kProbabilityPerGroup);
  int max_6_concurrent_prefetch = trial_->AppendGroup(
        "_max_6 concurrent_prefetch", kProbabilityPerGroup);

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
    if (trial_->group() == max_2_concurrent_prefetch)
      max_concurrent = 2;
    else if (trial_->group() == max_4_concurrent_prefetch)
      max_concurrent = 4;
    else if (trial_->group() == max_6_concurrent_prefetch)
      max_concurrent = 6;
    // Scale acceptable delay so we don't cause congestion limits to fire as
    // we modulate max_concurrent (*if* we are modulating it at all).
    max_queueing_delay_ms = (kMaxPrefetchQueueingDelayMs *
                             kMaxPrefetchConcurrentLookups) / max_concurrent;

    TimeDelta max_queueing_delay(
        TimeDelta::FromMilliseconds(max_queueing_delay_ms));

    Preconnect::SetPreconnectDespiteProxy(proconnect_despite_proxy);

    DCHECK(!predictor);
    InitNetworkPredictor(max_queueing_delay, max_concurrent, user_prefs,
                         local_state, preconnect_enabled);
  }
}


}  // namespace chrome_browser_net
