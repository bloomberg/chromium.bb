// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/predictor_api.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/lazy_instance.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/preconnect.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/net/url_info.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_impl.h"

using base::Time;
using base::TimeDelta;

namespace chrome_browser_net {

static void DnsPrefetchMotivatedList(const UrlList& urls,
                                     UrlInfo::ResolutionMotivation motivation);

static UrlList GetPredictedUrlListAtStartup(PrefService* user_prefs,
                                            PrefService* local_state);

// Given that the underlying Chromium resolver defaults to a total maximum of
// 8 paralell resolutions, we will avoid any chance of starving navigational
// resolutions by limiting the number of paralell speculative resolutions.
// TODO(jar): Move this limitation into the resolver.
// static
const size_t PredictorInit::kMaxSpeculativeParallelResolves = 3;

// To control our congestion avoidance system, which discards a queue when
// resolutions are "taking too long," we need an expected resolution time.
// Common average is in the range of 300-500ms.
static const int kExpectedResolutionTimeMs = 500;

// To control the congestion avoidance system, we need an estimate of how many
// speculative requests may arrive at once.  Since we currently only keep 8
// subresource names for each frame, we'll use that as our basis.  Note that
// when scanning search results lists, we might actually get 10 at a time, and
// wikipedia can often supply (during a page scan) upwards of 50.  In those odd
// cases, we may discard some of the later speculative requests mistakenly
// assuming that the resolutions took too long.
static const int kTypicalSpeculativeGroupSize = 8;

// The next constant specifies an amount of queueing delay that is "too large,"
// and indicative of problems with resolutions (perhaps due to an overloaded
// router, or such).  When we exceed this delay, congestion avoidance will kick
// in and all speculations in the queue will be discarded.
// static
const int PredictorInit::kMaxSpeculativeResolveQueueDelayMs =
    (kExpectedResolutionTimeMs * kTypicalSpeculativeGroupSize) /
    kMaxSpeculativeParallelResolves;

// A version number for prefs that are saved. This should be incremented when
// we change the format so that we discard old data.
static const int kPredictorStartupFormatVersion = 1;

// There will only be one instance ever created of the following Observer class.
// The InitialObserver lives on the IO thread, and monitors navigations made by
// the network stack.  This is only used to identify startup time resolutions
// (for re-resolution during our next process startup).
// TODO(jar): Consider preconnecting at startup, which may be faster than
// waiting for render process to start and request a connection.
class InitialObserver {
 public:
  // Recording of when we observed each navigation.
  typedef std::map<GURL, base::TimeTicks> FirstNavigations;

  // Potentially add a new URL to our startup list.
  void Append(const GURL& url);

  // Get an HTML version of our current planned first_navigations_.
  void GetFirstResolutionsHtml(std::string* output);

  // Persist the current first_navigations_ for storage in a list.
  void GetInitialDnsResolutionList(ListValue* startup_list);

 private:
  // List of the first N URL resolutions observed in this run.
  FirstNavigations first_navigations_;

  // The number of URLs we'll save for pre-resolving at next startup.
  static const size_t kStartupResolutionCount = 10;
};

// TODO(willchan): Look at killing this global.
static InitialObserver* g_initial_observer = NULL;

//------------------------------------------------------------------------------
// This section contains all the globally accessable API entry points for the
// DNS Prefetching feature.
//------------------------------------------------------------------------------

// Status of speculative DNS resolution and speculative TCP/IP connection
// feature.
static bool predictor_enabled = true;

// Cached inverted copy of the off_the_record pref.
static bool on_the_record_switch = true;

// Enable/disable Dns prefetch activity (either via command line, or via pref).
void EnablePredictor(bool enable) {
  // NOTE: this is invoked on the UI thread.
  predictor_enabled = enable;
}

void OnTheRecord(bool enable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (on_the_record_switch == enable)
    return;
  on_the_record_switch = enable;
  if (on_the_record_switch)
    g_browser_process->io_thread()->ChangedToOnTheRecord();
}

void RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterListPref(prefs::kDnsPrefetchingStartupList);
  user_prefs->RegisterListPref(prefs::kDnsPrefetchingHostReferralList);
  user_prefs->RegisterBooleanPref(prefs::kDnsPrefetchingEnabled, true);
}

// When enabled, we use the following instance to service all requests in the
// browser process.
// TODO(willchan): Look at killing this.
static Predictor* g_predictor = NULL;

// This API is only used in the browser process.
// It is called from an IPC message originating in the renderer.  It currently
// includes both Page-Scan, and Link-Hover prefetching.
// TODO(jar): Separate out link-hover prefetching, and page-scan results.
void DnsPrefetchList(const NameList& hostnames) {
  // TODO(jar): Push GURL transport further back into renderer, but this will
  // require a Webkit change in the observer :-/.
  UrlList urls;
  for (NameList::const_iterator it = hostnames.begin();
       it < hostnames.end();
       ++it) {
    urls.push_back(GURL("http://" + *it + ":80"));
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DnsPrefetchMotivatedList(urls, UrlInfo::PAGE_SCAN_MOTIVATED);
}

static void DnsPrefetchMotivatedList(
    const UrlList& urls,
    UrlInfo::ResolutionMotivation motivation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!predictor_enabled || NULL == g_predictor)
    return;

  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    g_predictor->ResolveList(urls, motivation);
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        NewRunnableMethod(g_predictor,
                          &Predictor::ResolveList, urls, motivation));
  }
}

// This API is used by the autocomplete popup box (where URLs are typed).
void AnticipateOmniboxUrl(const GURL& url, bool preconnectable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!predictor_enabled || NULL == g_predictor)
    return;
  if (!url.is_valid() || !url.has_host())
    return;

  g_predictor->AnticipateOmniboxUrl(url, preconnectable);
}

void PreconnectUrlAndSubresources(const GURL& url) {
  if (!predictor_enabled || NULL == g_predictor)
    return;
  if (!url.is_valid() || !url.has_host())
    return;

  g_predictor->PreconnectUrlAndSubresources(url);
}


//------------------------------------------------------------------------------
// This section intermingles prefetch results with actual browser HTTP
// network activity.  It supports calculating of the benefit of a prefetch, as
// well as recording what prefetched hostname resolutions might be potentially
// helpful during the next chrome-startup.
//------------------------------------------------------------------------------

void PredictFrameSubresources(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!predictor_enabled || NULL == g_predictor)
    return;
  g_predictor->PredictFrameSubresources(url);
}

void LearnAboutInitialNavigation(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!predictor_enabled || NULL == g_initial_observer )
    return;
  g_initial_observer->Append(url);
}

void LearnFromNavigation(const GURL& referring_url, const GURL& target_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!predictor_enabled || NULL == g_predictor)
    return;
  g_predictor->LearnFromNavigation(referring_url, target_url);
}

// The observer class needs to connect starts and finishes of HTTP network
// resolutions.  We use the following type for that map.
typedef std::map<int, UrlInfo> ObservedResolutionMap;

//------------------------------------------------------------------------------
// Member definitions for InitialObserver class.

void InitialObserver::Append(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!on_the_record_switch || NULL == g_predictor)
    return;
  if (kStartupResolutionCount <= first_navigations_.size())
    return;

  if (url.SchemeIs("http") || url.SchemeIs("https")) {
    const GURL url_without_path(Predictor::CanonicalizeUrl(url));
    if (first_navigations_.find(url_without_path) == first_navigations_.end())
      first_navigations_[url_without_path] = base::TimeTicks::Now();
  }
}

void InitialObserver::GetInitialDnsResolutionList(ListValue* startup_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(startup_list);
  startup_list->Clear();
  DCHECK_EQ(0u, startup_list->GetSize());
  startup_list->Append(new FundamentalValue(kPredictorStartupFormatVersion));
  for (FirstNavigations::iterator it = first_navigations_.begin();
       it != first_navigations_.end();
       ++it) {
    DCHECK(it->first == Predictor::CanonicalizeUrl(it->first));
    startup_list->Append(new StringValue(it->first.spec()));
  }
}

void InitialObserver::GetFirstResolutionsHtml(std::string* output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  UrlInfo::UrlInfoTable resolution_list;
  {
    for (FirstNavigations::iterator it(first_navigations_.begin());
         it != first_navigations_.end();
         it++) {
      UrlInfo info;
      info.SetUrl(it->first);
      info.set_time(it->second);
      resolution_list.push_back(info);
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
  friend struct base::DefaultLazyInstanceTraits<OffTheRecordObserver>;

  OffTheRecordObserver() : count_off_the_record_windows_(0) {}
  ~OffTheRecordObserver() {}

  NotificationRegistrar registrar_;
  int count_off_the_record_windows_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordObserver);
};

static base::LazyInstance<OffTheRecordObserver> g_off_the_record_observer(
    base::LINKER_INITIALIZED);

//------------------------------------------------------------------------------
// This section supports the about:dns page.
//------------------------------------------------------------------------------

// Provide global support for the about:dns page.
void PredictorGetHtmlInfo(std::string* output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  output->append("<html><head><title>About DNS</title>"
                 // We'd like the following no-cache... but it doesn't work.
                 // "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
                 "</head><body>");
  if (!predictor_enabled  || NULL == g_predictor) {
    output->append("DNS pre-resolution and TCP pre-connection is disabled.");
  } else {
    if (!on_the_record_switch) {
      output->append("Incognito mode is active in a window.");
    } else {
      // List items fetched at startup.
      if (g_initial_observer)
        g_initial_observer->GetFirstResolutionsHtml(output);
      // Show list of subresource predictions and stats.
      g_predictor->GetHtmlReferrerLists(output);
      // Show list of prediction results.
      g_predictor->GetHtmlInfo(output);
    }
  }
  output->append("</body></html>");
}

//------------------------------------------------------------------------------
// This section intializes global DNS prefetch services.
//------------------------------------------------------------------------------

static void InitNetworkPredictor(TimeDelta max_dns_queue_delay,
                                 size_t max_parallel_resolves,
                                 PrefService* user_prefs,
                                 PrefService* local_state,
                                 bool preconnect_enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool prefetching_enabled =
      user_prefs->GetBoolean(prefs::kDnsPrefetchingEnabled);

  // Gather the list of hostnames to prefetch on startup.
  UrlList urls =
      GetPredictedUrlListAtStartup(user_prefs, local_state);

  ListValue* referral_list =
      static_cast<ListValue*>(user_prefs->GetMutableList(
          prefs::kDnsPrefetchingHostReferralList)->DeepCopy());

  // Remove obsolete preferences from local state if necessary.
  int dns_prefs_version =
      user_prefs->GetInteger(prefs::kMultipleProfilePrefMigration);
  if (dns_prefs_version < 1) {
    // These prefs only need to be registered if they need to be cleared from
    // local state.
    local_state->RegisterListPref(prefs::kDnsStartupPrefetchList);
    local_state->RegisterListPref(prefs::kDnsHostReferralList);
    local_state->ClearPref(prefs::kDnsStartupPrefetchList);
    local_state->ClearPref(prefs::kDnsHostReferralList);
    user_prefs->SetInteger(prefs::kMultipleProfilePrefMigration, 1);
  }

  g_browser_process->io_thread()->InitNetworkPredictor(
      prefetching_enabled, max_dns_queue_delay, max_parallel_resolves, urls,
      referral_list, preconnect_enabled);
}

void FinalizePredictorInitialization(
    Predictor* global_predictor,
    const UrlList& startup_urls,
    ListValue* referral_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  g_predictor = global_predictor;
  g_initial_observer = new InitialObserver();

  // Prefetch these hostnames on startup.
  DnsPrefetchMotivatedList(startup_urls,
                           UrlInfo::STARTUP_LIST_MOTIVATED);
  g_predictor->DeserializeReferrersThenDelete(referral_list);
}

void FreePredictorResources() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  g_predictor = NULL;  // Owned and released by io_thread.cc.
  delete g_initial_observer;
  g_initial_observer = NULL;
}

//------------------------------------------------------------------------------
// Functions to handle saving of hostnames from one session to the next, to
// expedite startup times.

static void SaveDnsPrefetchStateForNextStartupAndTrimOnIOThread(
    ListValue* startup_list,
    ListValue* referral_list,
    base::WaitableEvent* completion) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (NULL == g_predictor) {
    completion->Signal();
    return;
  }

  if (g_initial_observer)
    g_initial_observer->GetInitialDnsResolutionList(startup_list);

  // TODO(jar): Trimming should be done more regularly, such as every 48 hours
  // of physical time, or perhaps after 48 hours of running (excluding time
  // between sessions possibly).
  // For now, we'll just trim at shutdown.
  g_predictor->TrimReferrers();
  g_predictor->SerializeReferrers(referral_list);

  completion->Signal();
}

void SavePredictorStateForNextStartupAndTrim(PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!predictor_enabled || g_predictor == NULL)
    return;

  base::WaitableEvent completion(true, false);

  bool posted = BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      NewRunnableFunction(SaveDnsPrefetchStateForNextStartupAndTrimOnIOThread,
          prefs->GetMutableList(prefs::kDnsPrefetchingStartupList),
          prefs->GetMutableList(prefs::kDnsPrefetchingHostReferralList),
          &completion));

  DCHECK(posted);
  if (posted)
    completion.Wait();
}

static UrlList GetPredictedUrlListAtStartup(PrefService* user_prefs,
                                            PrefService* local_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UrlList urls;
  // Recall list of URLs we learned about during last session.
  // This may catch secondary hostnames, pulled in by the homepages.  It will
  // also catch more of the "primary" home pages, since that was (presumably)
  // rendered first (and will be rendered first this time too).
  ListValue* startup_list =
      user_prefs->GetMutableList(prefs::kDnsPrefetchingStartupList);

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
// g_predictor system (both DNS pre-resolution and TCP/IP connection
// prewarming).

PredictorInit::PredictorInit(PrefService* user_prefs,
                             PrefService* local_state,
                             bool preconnect_enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Set up a field trial to see what disabling DNS pre-resolution does to
  // latency of page loads.
  base::FieldTrial::Probability kDivisor = 1000;
  // For each option (i.e., non-default), we have a fixed probability.
  base::FieldTrial::Probability kProbabilityPerGroup = 100;  // 10% probability.

  // After June 30, 2011 builds, it will always be in default group
  // (default_enabled_prefetch).
  trial_ = new base::FieldTrial("DnsImpact", kDivisor,
                                "default_enabled_prefetch", 2011, 6, 30);

  // First option is to disable prefetching completely.
  int disabled_prefetch = trial_->AppendGroup("disabled_prefetch",
                                              kProbabilityPerGroup);

  // We're running two experiments at the same time.  The first set of trials
  // modulates the delay-time until we declare a congestion event (and purge
  // our queue).  The second modulates the number of concurrent resolutions
  // we do at any time.  Users are in exactly one trial (or the default) during
  // any one run, and hence only one experiment at a time.
  // Experiment 1:
  // Set congestion detection at 250, 500, or 750ms, rather than the 1 second
  // default.
  int max_250ms_prefetch = trial_->AppendGroup("max_250ms_queue_prefetch",
                                               kProbabilityPerGroup);
  int max_500ms_prefetch = trial_->AppendGroup("max_500ms_queue_prefetch",
                                               kProbabilityPerGroup);
  int max_750ms_prefetch = trial_->AppendGroup("max_750ms_queue_prefetch",
                                               kProbabilityPerGroup);
  // Set congestion detection at 2 seconds instead of the 1 second default.
  int max_2s_prefetch = trial_->AppendGroup("max_2s_queue_prefetch",
                                            kProbabilityPerGroup);
  // Experiment 2:
  // Set max simultaneous resoultions to 2, 4, or 6, and scale the congestion
  // limit proportionally (so we don't impact average probability of asserting
  // congesion very much).
  int max_2_concurrent_prefetch = trial_->AppendGroup(
      "max_2 concurrent_prefetch", kProbabilityPerGroup);
  int max_4_concurrent_prefetch = trial_->AppendGroup(
      "max_4 concurrent_prefetch", kProbabilityPerGroup);
  int max_6_concurrent_prefetch = trial_->AppendGroup(
      "max_6 concurrent_prefetch", kProbabilityPerGroup);

  // We will register the incognito observer regardless of whether prefetching
  // is enabled, as it is also used to clear the host cache.
  g_off_the_record_observer.Get().Register();

  if (trial_->group() != disabled_prefetch) {
    // Initialize the DNS prefetch system.
    size_t max_parallel_resolves = kMaxSpeculativeParallelResolves;
    int max_queueing_delay_ms = kMaxSpeculativeResolveQueueDelayMs;

    if (trial_->group() == max_2_concurrent_prefetch)
      max_parallel_resolves = 2;
    else if (trial_->group() == max_4_concurrent_prefetch)
      max_parallel_resolves = 4;
    else if (trial_->group() == max_6_concurrent_prefetch)
      max_parallel_resolves = 6;

    if (trial_->group() == max_250ms_prefetch) {
      max_queueing_delay_ms =
         (250 * kTypicalSpeculativeGroupSize) /  max_parallel_resolves;
    } else if (trial_->group() == max_500ms_prefetch) {
      max_queueing_delay_ms =
          (500 * kTypicalSpeculativeGroupSize) /  max_parallel_resolves;
    } else if (trial_->group() == max_750ms_prefetch) {
      max_queueing_delay_ms =
          (750 * kTypicalSpeculativeGroupSize) /  max_parallel_resolves;
    } else if (trial_->group() == max_2s_prefetch) {
      max_queueing_delay_ms =
          (2000 * kTypicalSpeculativeGroupSize) /  max_parallel_resolves;
    }

    TimeDelta max_queueing_delay(
        TimeDelta::FromMilliseconds(max_queueing_delay_ms));

    DCHECK(!g_predictor);
    InitNetworkPredictor(max_queueing_delay, max_parallel_resolves, user_prefs,
                         local_state, preconnect_enabled);
  }
}

PredictorInit::~PredictorInit() {
}

}  // namespace chrome_browser_net
