// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/predictor.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/preconnect.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/single_request_host_resolver.h"

using base::TimeDelta;
using content::BrowserThread;

namespace chrome_browser_net {

// static
const int Predictor::kPredictorReferrerVersion = 2;
const double Predictor::kPreconnectWorthyExpectedValue = 0.8;
const double Predictor::kDNSPreresolutionWorthyExpectedValue = 0.1;
const double Predictor::kDiscardableExpectedValue = 0.05;
// The goal is of trimming is to to reduce the importance (number of expected
// subresources needed) by a factor of 2 after about 24 hours of uptime. We will
// trim roughly once-an-hour of uptime.  The ratio to use in each trim operation
// is then the 24th root of 0.5.  If a user only surfs for 4 hours a day, then
// after about 6 days they will have halved all their estimates of subresource
// connections.  Once this falls below kDiscardableExpectedValue the referrer
// will be discarded.
// TODO(jar): Measure size of referrer lists in the field.  Consider an adaptive
// system that uses a higher trim ratio when the list is large.
// static
const double Predictor::kReferrerTrimRatio = 0.97153;
const TimeDelta Predictor::kDurationBetweenTrimmings = TimeDelta::FromHours(1);
const TimeDelta Predictor::kDurationBetweenTrimmingIncrements =
    TimeDelta::FromSeconds(15);
const size_t Predictor::kUrlsTrimmedPerIncrement = 5u;
const size_t Predictor::kMaxSpeculativeParallelResolves = 3;
// To control our congestion avoidance system, which discards a queue when
// resolutions are "taking too long," we need an expected resolution time.
// Common average is in the range of 300-500ms.
const int kExpectedResolutionTimeMs = 500;
const int Predictor::kTypicalSpeculativeGroupSize = 8;
const int Predictor::kMaxSpeculativeResolveQueueDelayMs =
    (kExpectedResolutionTimeMs * Predictor::kTypicalSpeculativeGroupSize) /
    Predictor::kMaxSpeculativeParallelResolves;

static int g_max_queueing_delay_ms = 0;
static size_t g_max_parallel_resolves = 0u;

// A version number for prefs that are saved. This should be incremented when
// we change the format so that we discard old data.
static const int kPredictorStartupFormatVersion = 1;

class Predictor::LookupRequest {
 public:
  LookupRequest(Predictor* predictor,
                net::HostResolver* host_resolver,
                const GURL& url)
      : predictor_(predictor),
        url_(url),
        resolver_(host_resolver) {
  }

  // Return underlying network resolver status.
  // net::OK ==> Host was found synchronously.
  // net:ERR_IO_PENDING ==> Network will callback later with result.
  // anything else ==> Host was not found synchronously.
  int Start() {
    net::HostResolver::RequestInfo resolve_info(
        net::HostPortPair::FromURL(url_));

    // Make a note that this is a speculative resolve request. This allows us
    // to separate it from real navigations in the observer's callback, and
    // lets the HostResolver know it can de-prioritize it.
    resolve_info.set_is_speculative(true);
    return resolver_.Resolve(
        resolve_info, &addresses_,
        base::Bind(&LookupRequest::OnLookupFinished, base::Unretained(this)),
        net::BoundNetLog());
  }

 private:
  void OnLookupFinished(int result) {
    predictor_->OnLookupFinished(this, url_, result == net::OK);
  }

  Predictor* predictor_;  // The predictor which started us.

  const GURL url_;  // Hostname to resolve.
  net::SingleRequestHostResolver resolver_;
  net::AddressList addresses_;

  DISALLOW_COPY_AND_ASSIGN(LookupRequest);
};

Predictor::Predictor(bool preconnect_enabled)
    : initial_observer_(NULL),
      predictor_enabled_(true),
      peak_pending_lookups_(0),
      shutdown_(false),
      max_concurrent_dns_lookups_(g_max_parallel_resolves),
      max_dns_queue_delay_(
          TimeDelta::FromMilliseconds(g_max_queueing_delay_ms)),
      host_resolver_(NULL),
      preconnect_enabled_(preconnect_enabled),
      consecutive_omnibox_preconnect_count_(0),
      next_trim_time_(base::TimeTicks::Now() + kDurationBetweenTrimmings) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

Predictor::~Predictor() {
  // TODO(rlp): Add DCHECK for CurrentlyOn(BrowserThread::IO) when the
  // ProfileManagerTest has been updated with a mock profile.
  DCHECK(shutdown_);
}

// static
Predictor* Predictor::CreatePredictor(
    bool preconnect_enabled, bool simple_shutdown) {
  if (simple_shutdown)
    return new SimplePredictor(preconnect_enabled);
  return new Predictor(preconnect_enabled);
}

void Predictor::RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterListPref(prefs::kDnsPrefetchingStartupList,
                               PrefService::UNSYNCABLE_PREF);
  user_prefs->RegisterListPref(prefs::kDnsPrefetchingHostReferralList,
                               PrefService::UNSYNCABLE_PREF);
}

// --------------------- Start UI methods. ------------------------------------

void Predictor::InitNetworkPredictor(PrefService* user_prefs,
                                     PrefService* local_state,
                                     IOThread* io_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  bool predictor_enabled =
      user_prefs->GetBoolean(prefs::kNetworkPredictionEnabled);

  // Gather the list of hostnames to prefetch on startup.
  UrlList urls = GetPredictedUrlListAtStartup(user_prefs, local_state);

  base::ListValue* referral_list =
      static_cast<base::ListValue*>(user_prefs->GetList(
          prefs::kDnsPrefetchingHostReferralList)->DeepCopy());

  // Remove obsolete preferences from local state if necessary.
  int current_version =
      local_state->GetInteger(prefs::kMultipleProfilePrefMigration);
  if ((current_version & browser::DNS_PREFS) == 0) {
    local_state->RegisterListPref(prefs::kDnsStartupPrefetchList,
                                  PrefService::UNSYNCABLE_PREF);
    local_state->RegisterListPref(prefs::kDnsHostReferralList,
                                  PrefService::UNSYNCABLE_PREF);
    local_state->ClearPref(prefs::kDnsStartupPrefetchList);
    local_state->ClearPref(prefs::kDnsHostReferralList);
    local_state->SetInteger(prefs::kMultipleProfilePrefMigration,
        current_version | browser::DNS_PREFS);
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &Predictor::FinalizeInitializationOnIOThread,
          base::Unretained(this),
          urls, referral_list,
          io_thread, predictor_enabled));
}

void Predictor::AnticipateOmniboxUrl(const GURL& url, bool preconnectable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!predictor_enabled_)
    return;
  if (!url.is_valid() || !url.has_host())
    return;
  std::string host = url.HostNoBrackets();
  bool is_new_host_request = (host != last_omnibox_host_);
  last_omnibox_host_ = host;

  UrlInfo::ResolutionMotivation motivation(UrlInfo::OMNIBOX_MOTIVATED);
  base::TimeTicks now = base::TimeTicks::Now();

  if (preconnect_enabled()) {
    if (preconnectable && !is_new_host_request) {
      ++consecutive_omnibox_preconnect_count_;
      // The omnibox suggests a search URL (for which we can preconnect) after
      // one or two characters are typed, even though such typing often (1 in
      // 3?) becomes a real URL.  This code waits till is has more evidence of a
      // preconnectable URL (search URL) before forming a preconnection, so as
      // to reduce the useless preconnect rate.
      // Perchance this logic should be pushed back into the omnibox, where the
      // actual characters typed, such as a space, can better forcast whether
      // we need to search/preconnect or not.  By waiting for at least 4
      // characters in a row that have lead to a search proposal, we avoid
      // preconnections for a prefix like "www." and we also wait until we have
      // at least a 4 letter word to search for.
      // Each character typed appears to induce 2 calls to
      // AnticipateOmniboxUrl(), so we double 4 characters and limit at 8
      // requests.
      // TODO(jar): Use an A/B test to optimize this.
      const int kMinConsecutiveRequests = 8;
      if (consecutive_omnibox_preconnect_count_ >= kMinConsecutiveRequests) {
        // TODO(jar): Perhaps we should do a GET to leave the socket open in the
        // pool.  Currently, we just do a connect, which MAY be reset if we
        // don't use it in 10 secondes!!!  As a result, we may do more
        // connections, and actually cost the server more than if we did a real
        // get with a fake request (/gen_204 might be the good path on Google).
        const int kMaxSearchKeepaliveSeconds(10);
        if ((now - last_omnibox_preconnect_).InSeconds() <
             kMaxSearchKeepaliveSeconds)
          return;  // We've done a preconnect recently.
        last_omnibox_preconnect_ = now;
        const int kConnectionsNeeded = 1;
        PreconnectOnUIThread(CanonicalizeUrl(url), motivation,
                             kConnectionsNeeded);
        return;  // Skip pre-resolution, since we'll open a connection.
      }
    } else {
      consecutive_omnibox_preconnect_count_ = 0;
    }
  }

  // Fall through and consider pre-resolution.

  // Omnibox tends to call in pairs (just a few milliseconds apart), and we
  // really don't need to keep resolving a name that often.
  // TODO(jar): A/B tests could check for perf impact of the early returns.
  if (!is_new_host_request) {
    const int kMinPreresolveSeconds(10);
    if (kMinPreresolveSeconds > (now - last_omnibox_preresolve_).InSeconds())
      return;
  }
  last_omnibox_preresolve_ = now;

  // Perform at least DNS pre-resolution.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&Predictor::Resolve, base::Unretained(this),
                 CanonicalizeUrl(url), motivation));
}

void Predictor::PreconnectUrlAndSubresources(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!predictor_enabled_)
    return;
  if (!url.is_valid() || !url.has_host())
    return;
  if (preconnect_enabled()) {
    std::string host = url.HostNoBrackets();
    UrlInfo::ResolutionMotivation motivation(UrlInfo::EARLY_LOAD_MOTIVATED);
    const int kConnectionsNeeded = 1;
    PreconnectOnUIThread(CanonicalizeUrl(url), motivation,
                         kConnectionsNeeded);
    PredictFrameSubresources(url.GetWithEmptyPath());
  }
}

UrlList Predictor::GetPredictedUrlListAtStartup(
    PrefService* user_prefs,
    PrefService* local_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UrlList urls;
  // Recall list of URLs we learned about during last session.
  // This may catch secondary hostnames, pulled in by the homepages.  It will
  // also catch more of the "primary" home pages, since that was (presumably)
  // rendered first (and will be rendered first this time too).
  const ListValue* startup_list =
      user_prefs->GetList(prefs::kDnsPrefetchingStartupList);

  if (startup_list) {
    base::ListValue::const_iterator it = startup_list->begin();
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

void Predictor::set_max_queueing_delay(int max_queueing_delay_ms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_max_queueing_delay_ms = max_queueing_delay_ms;
}

void Predictor::set_max_parallel_resolves(size_t max_parallel_resolves) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_max_parallel_resolves = max_parallel_resolves;
}

void Predictor::ShutdownOnUIThread(PrefService* user_prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SaveStateForNextStartupAndTrim(user_prefs);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&Predictor::Shutdown, base::Unretained(this)));
}

// ---------------------- End UI methods. -------------------------------------

// --------------------- Start IO methods. ------------------------------------

void Predictor::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shutdown_);
  shutdown_ = true;

  STLDeleteElements(&pending_lookups_);
}

void Predictor::DiscardAllResults() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Delete anything listed so far in this session that shows in about:dns.
  referrers_.clear();


  // Try to delete anything in our work queue.
  while (!work_queue_.IsEmpty()) {
    // Emulate processing cycle as though host was not found.
    GURL url = work_queue_.Pop();
    UrlInfo* info = &results_[url];
    DCHECK(info->HasUrl(url));
    info->SetAssignedState();
    info->SetNoSuchNameState();
  }
  // Now every result_ is either resolved, or is being resolved
  // (see LookupRequest).

  // Step through result_, recording names of all hosts that can't be erased.
  // We can't erase anything being worked on.
  Results assignees;
  for (Results::iterator it = results_.begin(); results_.end() != it; ++it) {
    GURL url(it->first);
    UrlInfo* info = &it->second;
    DCHECK(info->HasUrl(url));
    if (info->is_assigned()) {
      info->SetPendingDeleteState();
      assignees[url] = *info;
    }
  }
  DCHECK_LE(assignees.size(), max_concurrent_dns_lookups_);
  results_.clear();
  // Put back in the names being worked on.
  for (Results::iterator it = assignees.begin(); assignees.end() != it; ++it) {
    DCHECK(it->second.is_marked_to_delete());
    results_[it->first] = it->second;
  }
}

// Overloaded Resolve() to take a vector of names.
void Predictor::ResolveList(const UrlList& urls,
                            UrlInfo::ResolutionMotivation motivation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (UrlList::const_iterator it = urls.begin(); it < urls.end(); ++it) {
    AppendToResolutionQueue(*it, motivation);
  }
}

// Basic Resolve() takes an invidual name, and adds it
// to the queue.
void Predictor::Resolve(const GURL& url,
                        UrlInfo::ResolutionMotivation motivation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!url.has_host())
    return;
  AppendToResolutionQueue(url, motivation);
}

void Predictor::LearnFromNavigation(const GURL& referring_url,
                                    const GURL& target_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!predictor_enabled_)
    return;
  DCHECK_EQ(referring_url, Predictor::CanonicalizeUrl(referring_url));
  DCHECK_NE(referring_url, GURL::EmptyGURL());
  DCHECK_EQ(target_url, Predictor::CanonicalizeUrl(target_url));
  DCHECK_NE(target_url, GURL::EmptyGURL());

  referrers_[referring_url].SuggestHost(target_url);
  // Possibly do some referrer trimming.
  TrimReferrers();
}

//-----------------------------------------------------------------------------
// This section supports the about:dns page.

void Predictor::PredictorGetHtmlInfo(Predictor* predictor,
                                     std::string* output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  output->append("<html><head><title>About DNS</title>"
                 // We'd like the following no-cache... but it doesn't work.
                 // "<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">"
                 "</head><body>");
  if (predictor && predictor->predictor_enabled()) {
    predictor->GetHtmlInfo(output);
  } else {
    output->append("DNS pre-resolution and TCP pre-connection is disabled.");
  }
  output->append("</body></html>");
}

// Provide sort order so all .com's are together, etc.
struct RightToLeftStringSorter {
  bool operator()(const GURL& left,
                  const GURL& right) const {
    return string_compare(left.host(), right.host());
  }

  static bool string_compare(const std::string& left_host,
                             const std::string& right_host) {
    if (left_host == right_host) return true;
    size_t left_already_matched = left_host.size();
    size_t right_already_matched = right_host.size();

    // Ensure both strings have characters.
    if (!left_already_matched) return true;
    if (!right_already_matched) return false;

    // Watch for trailing dot, so we'll always be safe to go one beyond dot.
    if ('.' == left_host[left_already_matched - 1]) {
      if ('.' != right_host[right_already_matched - 1])
        return true;
      // Both have dots at end of string.
      --left_already_matched;
      --right_already_matched;
    } else {
      if ('.' == right_host[right_already_matched - 1])
        return false;
    }

    while (1) {
      if (!left_already_matched) return true;
      if (!right_already_matched) return false;

      size_t left_start = left_host.find_last_of('.', left_already_matched - 1);
      if (std::string::npos == left_start) {
        left_already_matched = left_start = 0;
      } else {
        left_already_matched = left_start;
        ++left_start;  // Don't compare the dot.
      }
      size_t right_start = right_host.find_last_of('.',
                                                   right_already_matched - 1);
      if (std::string::npos == right_start) {
        right_already_matched = right_start = 0;
      } else {
        right_already_matched = right_start;
        ++right_start;  // Don't compare the dot.
      }

      int diff = left_host.compare(left_start, left_host.size(),
                                   right_host, right_start, right_host.size());
      if (diff > 0) return false;
      if (diff < 0) return true;
    }
  }
};

void Predictor::GetHtmlReferrerLists(std::string* output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (referrers_.empty())
    return;

  // TODO(jar): Remove any plausible JavaScript from names before displaying.

  typedef std::set<GURL, struct RightToLeftStringSorter>
      SortedNames;
  SortedNames sorted_names;

  for (Referrers::iterator it = referrers_.begin();
       referrers_.end() != it; ++it)
    sorted_names.insert(it->first);

  output->append("<br><table border>");
  output->append(
      "<tr><th>Host for Page</th>"
      "<th>Page Load<br>Count</th>"
      "<th>Subresource<br>Navigations</th>"
      "<th>Subresource<br>PreConnects</th>"
      "<th>Subresource<br>PreResolves</th>"
      "<th>Expected<br>Connects</th>"
      "<th>Subresource Spec</th></tr>");

  for (SortedNames::iterator it = sorted_names.begin();
       sorted_names.end() != it; ++it) {
    Referrer* referrer = &(referrers_[*it]);
    bool first_set_of_futures = true;
    for (Referrer::iterator future_url = referrer->begin();
         future_url != referrer->end(); ++future_url) {
      output->append("<tr align=right>");
      if (first_set_of_futures) {
        base::StringAppendF(output,
                            "<td rowspan=%d>%s</td><td rowspan=%d>%d</td>",
                            static_cast<int>(referrer->size()),
                            it->spec().c_str(),
                            static_cast<int>(referrer->size()),
                            static_cast<int>(referrer->use_count()));
      }
      first_set_of_futures = false;
      base::StringAppendF(output,
          "<td>%d</td><td>%d</td><td>%d</td><td>%2.3f</td><td>%s</td></tr>",
          static_cast<int>(future_url->second.navigation_count()),
          static_cast<int>(future_url->second.preconnection_count()),
          static_cast<int>(future_url->second.preresolution_count()),
          static_cast<double>(future_url->second.subresource_use_rate()),
          future_url->first.spec().c_str());
    }
  }
  output->append("</table>");
}

void Predictor::GetHtmlInfo(std::string* output) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (initial_observer_.get())
    initial_observer_->GetFirstResolutionsHtml(output);
  // Show list of subresource predictions and stats.
  GetHtmlReferrerLists(output);

  // Local lists for calling UrlInfo
  UrlInfo::UrlInfoTable name_not_found;
  UrlInfo::UrlInfoTable name_preresolved;

  // Get copies of all useful data.
  typedef std::map<GURL, UrlInfo, RightToLeftStringSorter> SortedUrlInfo;
  SortedUrlInfo snapshot;
  // UrlInfo supports value semantics, so we can do a shallow copy.
  for (Results::iterator it(results_.begin()); it != results_.end(); it++)
    snapshot[it->first] = it->second;

  // Partition the UrlInfo's into categories.
  for (SortedUrlInfo::iterator it(snapshot.begin());
       it != snapshot.end(); it++) {
    if (it->second.was_nonexistent()) {
      name_not_found.push_back(it->second);
      continue;
    }
    if (!it->second.was_found())
      continue;  // Still being processed.
    name_preresolved.push_back(it->second);
  }

  bool brief = false;
#ifdef NDEBUG
  brief = true;
#endif  // NDEBUG

  // Call for display of each table, along with title.
  UrlInfo::GetHtmlTable(name_preresolved,
      "Preresolution DNS records performed for ", brief, output);
  UrlInfo::GetHtmlTable(name_not_found,
      "Preresolving DNS records revealed non-existence for ", brief, output);
}

void Predictor::TrimReferrersNow() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Just finish up work if an incremental trim is in progress.
  if (urls_being_trimmed_.empty())
    LoadUrlsForTrimming();
  IncrementalTrimReferrers(true);  // Do everything now.
}

void Predictor::SerializeReferrers(base::ListValue* referral_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  referral_list->Clear();
  referral_list->Append(new base::FundamentalValue(kPredictorReferrerVersion));
  for (Referrers::const_iterator it = referrers_.begin();
       it != referrers_.end(); ++it) {
    // Serialize the list of subresource names.
    Value* subresource_list(it->second.Serialize());

    // Create a list for each referer.
    ListValue* motivator(new ListValue);
    motivator->Append(new StringValue(it->first.spec()));
    motivator->Append(subresource_list);

    referral_list->Append(motivator);
  }
}

void Predictor::DeserializeReferrers(const base::ListValue& referral_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  int format_version = -1;
  if (referral_list.GetSize() > 0 &&
      referral_list.GetInteger(0, &format_version) &&
      format_version == kPredictorReferrerVersion) {
    for (size_t i = 1; i < referral_list.GetSize(); ++i) {
      base::ListValue* motivator;
      if (!referral_list.GetList(i, &motivator)) {
        NOTREACHED();
        return;
      }
      std::string motivating_url_spec;
      if (!motivator->GetString(0, &motivating_url_spec)) {
        NOTREACHED();
        return;
      }

      Value* subresource_list;
      if (!motivator->Get(1, &subresource_list)) {
        NOTREACHED();
        return;
      }

      referrers_[GURL(motivating_url_spec)].Deserialize(*subresource_list);
    }
  }
}

void Predictor::DeserializeReferrersThenDelete(
    base::ListValue* referral_list) {
  DeserializeReferrers(*referral_list);
  delete referral_list;
}

void Predictor::DiscardInitialNavigationHistory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (initial_observer_.get())
    initial_observer_->DiscardInitialNavigationHistory();
}

void Predictor::FinalizeInitializationOnIOThread(
    const UrlList& startup_urls,
    base::ListValue* referral_list,
    IOThread* io_thread,
    bool predictor_enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  predictor_enabled_ = predictor_enabled;
  initial_observer_.reset(new InitialObserver());
  host_resolver_ = io_thread->globals()->host_resolver.get();

  // ScopedRunnableMethodFactory instances need to be created and destroyed
  // on the same thread. The predictor lives on the IO thread and will die
  // from there so now that we're on the IO thread we need to properly
  // initialize the ScopedrunnableMethodFactory.
  trim_task_factory_.reset(new ScopedRunnableMethodFactory<Predictor>(this));

  // Prefetch these hostnames on startup.
  DnsPrefetchMotivatedList(startup_urls, UrlInfo::STARTUP_LIST_MOTIVATED);
  DeserializeReferrersThenDelete(referral_list);
}

//-----------------------------------------------------------------------------
// This section intermingles prefetch results with actual browser HTTP
// network activity.  It supports calculating of the benefit of a prefetch, as
// well as recording what prefetched hostname resolutions might be potentially
// helpful during the next chrome-startup.
//-----------------------------------------------------------------------------

void Predictor::LearnAboutInitialNavigation(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!predictor_enabled_ || NULL == initial_observer_.get() )
    return;
  initial_observer_->Append(url, this);
}

// This API is only used in the browser process.
// It is called from an IPC message originating in the renderer.  It currently
// includes both Page-Scan, and Link-Hover prefetching.
// TODO(jar): Separate out link-hover prefetching, and page-scan results.
void Predictor::DnsPrefetchList(const NameList& hostnames) {
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

void Predictor::DnsPrefetchMotivatedList(
    const UrlList& urls,
    UrlInfo::ResolutionMotivation motivation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!predictor_enabled_)
    return;

  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    ResolveList(urls, motivation);
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Predictor::ResolveList, base::Unretained(this),
                   urls, motivation));
  }
}

//-----------------------------------------------------------------------------
// Functions to handle saving of hostnames from one session to the next, to
// expedite startup times.

static void SaveDnsPrefetchStateForNextStartupAndTrimOnIOThread(
    base::ListValue* startup_list,
    base::ListValue* referral_list,
    base::WaitableEvent* completion,
    Predictor* predictor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (NULL == predictor) {
    completion->Signal();
    return;
  }
  predictor->SaveDnsPrefetchStateForNextStartupAndTrim(
      startup_list, referral_list, completion);
}

void Predictor::SaveStateForNextStartupAndTrim(PrefService* prefs) {
  if (!predictor_enabled_)
    return;

  base::WaitableEvent completion(true, false);

  ListPrefUpdate update_startup_list(prefs, prefs::kDnsPrefetchingStartupList);
  ListPrefUpdate update_referral_list(prefs,
                                      prefs::kDnsPrefetchingHostReferralList);
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    SaveDnsPrefetchStateForNextStartupAndTrimOnIOThread(
        update_startup_list.Get(),
        update_referral_list.Get(),
        &completion,
        this);
  } else {
    bool posted = BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(
            SaveDnsPrefetchStateForNextStartupAndTrimOnIOThread,
            update_startup_list.Get(),
            update_referral_list.Get(),
            &completion,
            this));

    // TODO(jar): Synchronous waiting for the IO thread is a potential source
    // to deadlocks and should be investigated. See http://crbug.com/78451.
    DCHECK(posted);
    if (posted)
      completion.Wait();
  }
}

void Predictor::SaveDnsPrefetchStateForNextStartupAndTrim(
    base::ListValue* startup_list,
    base::ListValue* referral_list,
    base::WaitableEvent* completion) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (initial_observer_.get())
    initial_observer_->GetInitialDnsResolutionList(startup_list);

  // Do at least one trim at shutdown, in case the user wasn't running long
  // enough to do any regular trimming of referrers.
  TrimReferrersNow();
  SerializeReferrers(referral_list);

  completion->Signal();
}

void Predictor::EnablePredictor(bool enable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    EnablePredictorOnIOThread(enable);
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Predictor::EnablePredictorOnIOThread,
                   base::Unretained(this), enable));
  }
}

void Predictor::EnablePredictorOnIOThread(bool enable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  predictor_enabled_ = enable;
}

void Predictor::PredictFrameSubresources(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!predictor_enabled_)
    return;
  DCHECK_EQ(url.GetWithEmptyPath(), url);
  // Add one pass through the message loop to allow current navigation to
  // proceed.
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    PrepareFrameSubresources(url);
  } else {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Predictor::PrepareFrameSubresources,
                   base::Unretained(this), url));
  }
}

enum SubresourceValue {
  PRECONNECTION,
  PRERESOLUTION,
  TOO_NEW,
  SUBRESOURCE_VALUE_MAX
};

void Predictor::PrepareFrameSubresources(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(url.GetWithEmptyPath(), url);
  Referrers::iterator it = referrers_.find(url);
  if (referrers_.end() == it) {
    // Only when we don't know anything about this url, make 2 connections
    // available.  We could do this completely via learning (by prepopulating
    // the referrer_ list with this expected value), but it would swell the
    // size of the list with all the "Leaf" nodes in the tree (nodes that don't
    // load any subresources).  If we learn about this resource, we will instead
    // provide a more carefully estimated preconnection count.
    if (preconnect_enabled_)
      PreconnectOnIOThread(url, UrlInfo::SELF_REFERAL_MOTIVATED, 2);
    return;
  }

  Referrer* referrer = &(it->second);
  referrer->IncrementUseCount();
  const UrlInfo::ResolutionMotivation motivation =
      UrlInfo::LEARNED_REFERAL_MOTIVATED;
  for (Referrer::iterator future_url = referrer->begin();
       future_url != referrer->end(); ++future_url) {
    SubresourceValue evalution(TOO_NEW);
    double connection_expectation = future_url->second.subresource_use_rate();
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.PreconnectSubresourceExpectation",
                                static_cast<int>(connection_expectation * 100),
                                10, 5000, 50);
    future_url->second.ReferrerWasObserved();
    if (preconnect_enabled_ &&
        connection_expectation > kPreconnectWorthyExpectedValue) {
      evalution = PRECONNECTION;
      future_url->second.IncrementPreconnectionCount();
      int count = static_cast<int>(std::ceil(connection_expectation));
      if (url.host() == future_url->first.host())
        ++count;
      PreconnectOnIOThread(future_url->first, motivation, count);
    } else if (connection_expectation > kDNSPreresolutionWorthyExpectedValue) {
      evalution = PRERESOLUTION;
      future_url->second.preresolution_increment();
      UrlInfo* queued_info = AppendToResolutionQueue(future_url->first,
                                                     motivation);
      if (queued_info)
        queued_info->SetReferringHostname(url);
    }
    UMA_HISTOGRAM_ENUMERATION("Net.PreconnectSubresourceEval", evalution,
                              SUBRESOURCE_VALUE_MAX);
  }
}

void Predictor::OnLookupFinished(LookupRequest* request, const GURL& url,
                                 bool found) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  LookupFinished(request, url, found);
  pending_lookups_.erase(request);
  delete request;

  StartSomeQueuedResolutions();
}

void Predictor::LookupFinished(LookupRequest* request, const GURL& url,
                               bool found) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  UrlInfo* info = &results_[url];
  DCHECK(info->HasUrl(url));
  if (info->is_marked_to_delete()) {
    results_.erase(url);
  } else {
    if (found)
      info->SetFoundState();
    else
      info->SetNoSuchNameState();
  }
}

UrlInfo* Predictor::AppendToResolutionQueue(
    const GURL& url,
    UrlInfo::ResolutionMotivation motivation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(url.has_host());

  if (shutdown_)
    return NULL;

  UrlInfo* info = &results_[url];
  info->SetUrl(url);  // Initialize or DCHECK.
  // TODO(jar):  I need to discard names that have long since expired.
  // Currently we only add to the domain map :-/

  DCHECK(info->HasUrl(url));

  if (!info->NeedsDnsUpdate()) {
    info->DLogResultsStats("DNS PrefetchNotUpdated");
    return NULL;
  }

  info->SetQueuedState(motivation);
  work_queue_.Push(url, motivation);
  StartSomeQueuedResolutions();
  return info;
}

bool Predictor::CongestionControlPerformed(UrlInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Note: queue_duration is ONLY valid after we go to assigned state.
  if (info->queue_duration() < max_dns_queue_delay_)
    return false;
  // We need to discard all entries in our queue, as we're keeping them waiting
  // too long.  By doing this, we'll have a chance to quickly service urgent
  // resolutions, and not have a bogged down system.
  while (true) {
    info->RemoveFromQueue();
    if (work_queue_.IsEmpty())
      break;
    info = &results_[work_queue_.Pop()];
    info->SetAssignedState();
  }
  return true;
}

void Predictor::StartSomeQueuedResolutions() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  while (!work_queue_.IsEmpty() &&
         pending_lookups_.size() < max_concurrent_dns_lookups_) {
    const GURL url(work_queue_.Pop());
    UrlInfo* info = &results_[url];
    DCHECK(info->HasUrl(url));
    info->SetAssignedState();

    if (CongestionControlPerformed(info)) {
      DCHECK(work_queue_.IsEmpty());
      return;
    }

    LookupRequest* request = new LookupRequest(this, host_resolver_, url);
    int status = request->Start();
    if (status == net::ERR_IO_PENDING) {
      // Will complete asynchronously.
      pending_lookups_.insert(request);
      peak_pending_lookups_ = std::max(peak_pending_lookups_,
                                       pending_lookups_.size());
    } else {
      // Completed synchronously (was already cached by HostResolver), or else
      // there was (equivalently) some network error that prevents us from
      // finding the name.  Status net::OK means it was "found."
      LookupFinished(request, url, status == net::OK);
      delete request;
    }
  }
}

void Predictor::TrimReferrers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!urls_being_trimmed_.empty())
    return;   // There is incremental trimming in progress already.

  // Check to see if it is time to trim yet.
  base::TimeTicks now = base::TimeTicks::Now();
  if (now < next_trim_time_)
    return;
  next_trim_time_ = now + kDurationBetweenTrimmings;

  LoadUrlsForTrimming();
  PostIncrementalTrimTask();
}

void Predictor::LoadUrlsForTrimming() {
  DCHECK(urls_being_trimmed_.empty());
  for (Referrers::const_iterator it = referrers_.begin();
       it != referrers_.end(); ++it)
    urls_being_trimmed_.push_back(it->first);
  UMA_HISTOGRAM_COUNTS("Net.PredictionTrimSize", urls_being_trimmed_.size());
}

void Predictor::PostIncrementalTrimTask() {
  if (urls_being_trimmed_.empty())
    return;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      trim_task_factory_->NewRunnableMethod(
          &Predictor::IncrementalTrimReferrers, false),
      kDurationBetweenTrimmingIncrements.InMilliseconds());
}

void Predictor::IncrementalTrimReferrers(bool trim_all_now) {
  size_t trim_count = urls_being_trimmed_.size();
  if (!trim_all_now)
    trim_count = std::min(trim_count, kUrlsTrimmedPerIncrement);
  while (trim_count-- != 0) {
    Referrers::iterator it = referrers_.find(urls_being_trimmed_.back());
    urls_being_trimmed_.pop_back();
    if (it == referrers_.end())
      continue;  // Defensive code: It got trimmed away already.
    if (!it->second.Trim(kReferrerTrimRatio, kDiscardableExpectedValue))
      referrers_.erase(it);
  }
  PostIncrementalTrimTask();
}

// ---------------------- End IO methods. -------------------------------------

//-----------------------------------------------------------------------------

Predictor::HostNameQueue::HostNameQueue() {
}

Predictor::HostNameQueue::~HostNameQueue() {
}

void Predictor::HostNameQueue::Push(const GURL& url,
    UrlInfo::ResolutionMotivation motivation) {
  switch (motivation) {
    case UrlInfo::STATIC_REFERAL_MOTIVATED:
    case UrlInfo::LEARNED_REFERAL_MOTIVATED:
    case UrlInfo::MOUSE_OVER_MOTIVATED:
      rush_queue_.push(url);
      break;

    default:
      background_queue_.push(url);
      break;
  }
}

bool Predictor::HostNameQueue::IsEmpty() const {
  return rush_queue_.empty() && background_queue_.empty();
}

GURL Predictor::HostNameQueue::Pop() {
  DCHECK(!IsEmpty());
  std::queue<GURL> *queue(rush_queue_.empty() ? &background_queue_
                                              : &rush_queue_);
  GURL url(queue->front());
  queue->pop();
  return url;
}

//-----------------------------------------------------------------------------
// Member definitions for InitialObserver class.

Predictor::InitialObserver::InitialObserver() {
}

Predictor::InitialObserver::~InitialObserver() {
}

void Predictor::InitialObserver::Append(const GURL& url,
                                        Predictor* predictor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // TODO(rlp): Do we really need the predictor check here?
  if (NULL == predictor)
    return;
  if (kStartupResolutionCount <= first_navigations_.size())
    return;

  DCHECK(url.SchemeIs("http") || url.SchemeIs("https"));
  DCHECK_EQ(url, Predictor::CanonicalizeUrl(url));
  if (first_navigations_.find(url) == first_navigations_.end())
    first_navigations_[url] = base::TimeTicks::Now();
}

void Predictor::InitialObserver::GetInitialDnsResolutionList(
    base::ListValue* startup_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(startup_list);
  startup_list->Clear();
  DCHECK_EQ(0u, startup_list->GetSize());
  startup_list->Append(
      new base::FundamentalValue(kPredictorStartupFormatVersion));
  for (FirstNavigations::iterator it = first_navigations_.begin();
       it != first_navigations_.end();
       ++it) {
    DCHECK(it->first == Predictor::CanonicalizeUrl(it->first));
    startup_list->Append(new StringValue(it->first.spec()));
  }
}

void Predictor::InitialObserver::GetFirstResolutionsHtml(
    std::string* output) {
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

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

// static
GURL Predictor::CanonicalizeUrl(const GURL& url) {
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

void SimplePredictor::InitNetworkPredictor(PrefService* user_prefs,
                                           PrefService* local_state,
                                           IOThread* io_thread) {
  // Empty function for unittests.
}

void SimplePredictor::ShutdownOnUIThread(PrefService* user_prefs) {
  SetShutdown(true);
}

}  // namespace chrome_browser_net
