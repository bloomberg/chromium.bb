// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/predictor.h"

#include <algorithm>
#include <set>
#include <sstream>

#include "base/compiler_specific.h"
#include "base/histogram.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/preconnect.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"

using base::TimeDelta;

namespace chrome_browser_net {

class Predictor::LookupRequest {
 public:
  LookupRequest(Predictor* predictor,
                net::HostResolver* host_resolver,
                const GURL& url)
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          net_callback_(this, &LookupRequest::OnLookupFinished)),
        predictor_(predictor),
        url_(url),
        resolver_(host_resolver) {
  }

  // Return underlying network resolver status.
  // net::OK ==> Host was found synchronously.
  // net:ERR_IO_PENDING ==> Network will callback later with result.
  // anything else ==> Host was not found synchronously.
  int Start() {
    net::HostResolver::RequestInfo resolve_info(url_.host(),
                                                url_.EffectiveIntPort());

    // Make a note that this is a speculative resolve request. This allows us
    // to separate it from real navigations in the observer's callback, and
    // lets the HostResolver know it can de-prioritize it.
    resolve_info.set_is_speculative(true);
    return resolver_.Resolve(
        resolve_info, &addresses_, &net_callback_, net::BoundNetLog());
  }

 private:
  void OnLookupFinished(int result) {
    predictor_->OnLookupFinished(this, url_, result == net::OK);
  }

  // HostResolver will call us using this callback when resolution is complete.
  net::CompletionCallbackImpl<LookupRequest> net_callback_;

  Predictor* predictor_;  // The predictor which started us.

  const GURL url_;  // Hostname to resolve.
  net::SingleRequestHostResolver resolver_;
  net::AddressList addresses_;

  DISALLOW_COPY_AND_ASSIGN(LookupRequest);
};

Predictor::Predictor(net::HostResolver* host_resolver,
                     base::TimeDelta max_dns_queue_delay,
                     size_t max_concurrent,
                     bool preconnect_enabled)
    : peak_pending_lookups_(0),
      shutdown_(false),
      max_concurrent_dns_lookups_(max_concurrent),
      max_dns_queue_delay_(max_dns_queue_delay),
      host_resolver_(host_resolver),
      preconnect_enabled_(preconnect_enabled) {
  Referrer::SetUsePreconnectValuations(preconnect_enabled);
}

Predictor::~Predictor() {
  DCHECK(shutdown_);
}

void Predictor::Shutdown() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!shutdown_);
  shutdown_ = true;

  std::set<LookupRequest*>::iterator it;
  for (it = pending_lookups_.begin(); it != pending_lookups_.end(); ++it)
    delete *it;
}

// Overloaded Resolve() to take a vector of names.
void Predictor::ResolveList(const UrlList& urls,
                            UrlInfo::ResolutionMotivation motivation) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  for (UrlList::const_iterator it = urls.begin(); it < urls.end(); ++it) {
    AppendToResolutionQueue(*it, motivation);
  }
}

// Basic Resolve() takes an invidual name, and adds it
// to the queue.
void Predictor::Resolve(const GURL& url,
                        UrlInfo::ResolutionMotivation motivation) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!url.has_host())
    return;
  AppendToResolutionQueue(url, motivation);
}

bool Predictor::AccruePrefetchBenefits(const GURL& referrer,
                                       UrlInfo* navigation_info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  GURL url = navigation_info->url();
  Results::iterator it = results_.find(url);
  if (it == results_.end()) {
    // Use UMA histogram to quantify potential future gains here.
    UMA_HISTOGRAM_LONG_TIMES("DNS.UnexpectedResolutionL",
                             navigation_info->resolve_duration());
    navigation_info->DLogResultsStats("DNS UnexpectedResolution");

    LearnFromNavigation(referrer, navigation_info->url());
    return false;
  }
  UrlInfo& prefetched_host_info(it->second);

  // Sometimes a host is used as a subresource by several referrers, so it is
  // in our list, but was never motivated by a page-link-scan.  In that case, it
  // really is an "unexpected" navigation, and we should tally it, and augment
  // our referrers_.
  bool referrer_based_prefetch = !prefetched_host_info.was_linked();
  if (referrer_based_prefetch) {
    // This wasn't the first time this host refered to *some* referrer.
    LearnFromNavigation(referrer, navigation_info->url());
  }

  DnsBenefit benefit = prefetched_host_info.AccruePrefetchBenefits(
      navigation_info);
  switch (benefit) {
    case PREFETCH_NAME_FOUND:
    case PREFETCH_NAME_NONEXISTANT:
      dns_cache_hits_.push_back(*navigation_info);
      if (referrer_based_prefetch) {
        if (referrer.has_host()) {
          referrers_[referrer].AccrueValue(
              navigation_info->benefits_remaining(), url);
        }
      }
      return true;

    case PREFETCH_CACHE_EVICTION:
      cache_eviction_map_[url] = *navigation_info;
      return false;

    case PREFETCH_NO_BENEFIT:
      // Prefetch never hit the network. Name was pre-cached.
      return false;

    default:
      NOTREACHED();
      return false;
  }
}

void Predictor::LearnFromNavigation(const GURL& referring_url,
                                    const GURL& target_url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (referring_url.has_host() &&
      referring_url != target_url) {
    DCHECK(referring_url == referring_url.GetWithEmptyPath());
    referrers_[referring_url].SuggestHost(target_url);
  }
}

void Predictor::PredictSubresources(const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  Referrers::iterator it = referrers_.find(url);
  if (referrers_.end() == it)
    return;
  Referrer* referrer = &(it->second);
  referrer->IncrementUseCount();
  for (Referrer::iterator future_url = referrer->begin();
       future_url != referrer->end(); ++future_url) {
    UrlInfo* queued_info = AppendToResolutionQueue(
        future_url->first,
        UrlInfo::LEARNED_REFERAL_MOTIVATED);
    if (queued_info)
      queued_info->SetReferringHostname(url);
  }
}

void Predictor::PredictFrameSubresources(const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(url.GetWithEmptyPath() == url);
  Referrers::iterator it = referrers_.find(url);
  if (referrers_.end() == it)
    return;
  Referrer* referrer = &(it->second);
  referrer->IncrementUseCount();
  for (Referrer::iterator future_url = referrer->begin();
       future_url != referrer->end(); ++future_url) {
    if (future_url->second.IsPreconnectWorthDoing())
      Preconnect::PreconnectOnIOThread(future_url->first);
  }
}

// Provide sort order so all .com's are together, etc.
struct RightToLeftStringSorter {
  bool operator()(const net::HostPortPair& left,
                  const net::HostPortPair& right) const {
    return string_compare(left.host, right.host);
  }
  bool operator()(const GURL& left,
                  const GURL& right) const {
    return string_compare(left.host(), right.host());
  }

  static bool string_compare(const std::string& left_host,
                             const std::string right_host) {
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

      size_t left_length, right_length;
      size_t left_start = left_host.find_last_of('.', left_already_matched - 1);
      if (std::string::npos == left_start) {
        left_length = left_already_matched;
        left_already_matched = left_start = 0;
      } else {
        left_length = left_already_matched - left_start;
        left_already_matched = left_start;
        ++left_start;  // Don't compare the dot.
      }
      size_t right_start = right_host.find_last_of('.',
                                                   right_already_matched - 1);
      if (std::string::npos == right_start) {
        right_length = right_already_matched;
        right_already_matched = right_start = 0;
      } else {
        right_length = right_already_matched - right_start;
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
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
      "<th>Expected<br>Connects</th>"
      "<th>DNS<br>Savings</th>"
      "<th>Subresource Spec</th></tr>");

  for (SortedNames::iterator it = sorted_names.begin();
       sorted_names.end() != it; ++it) {
    Referrer* referrer = &(referrers_[*it]);
    bool first_set_of_futures = true;
    for (Referrer::iterator future_url = referrer->begin();
         future_url != referrer->end(); ++future_url) {
      output->append("<tr align=right>");
      if (first_set_of_futures)
        StringAppendF(output, "<td rowspan=%d>%s</td><td rowspan=%d>%d</td>",
                      static_cast<int>(referrer->size()),
                      it->spec().c_str(),
                      static_cast<int>(referrer->size()),
                      static_cast<int>(referrer->use_count()));
      first_set_of_futures = false;
      StringAppendF(output,
          "<td>%d</td><td>%d</td><td>%2.3f</td><td>%dms</td><td>%s</td></tr>",
          static_cast<int>(future_url->second.navigation_count()),
          static_cast<int>(future_url->second.preconnection_count()),
          static_cast<double>(future_url->second.subresource_use_rate()),
          static_cast<int>(future_url->second.latency().InMilliseconds()),
          future_url->first.spec().c_str());
    }
  }
  output->append("</table>");
}

void Predictor::GetHtmlInfo(std::string* output) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // Local lists for calling UrlInfo
  UrlInfo::DnsInfoTable cache_hits;
  UrlInfo::DnsInfoTable cache_evictions;
  UrlInfo::DnsInfoTable name_not_found;
  UrlInfo::DnsInfoTable network_hits;
  UrlInfo::DnsInfoTable already_cached;

  // Get copies of all useful data.
  typedef std::map<GURL, UrlInfo, RightToLeftStringSorter>
      Snapshot;
  Snapshot snapshot;
  {
    // UrlInfo supports value semantics, so we can do a shallow copy.
    for (Results::iterator it(results_.begin()); it != results_.end(); it++) {
      snapshot[it->first] = it->second;
    }
    for (Results::iterator it(cache_eviction_map_.begin());
         it != cache_eviction_map_.end();
         it++) {
      cache_evictions.push_back(it->second);
    }
    // Reverse list as we copy cache hits, so that new hits are at the top.
    size_t index = dns_cache_hits_.size();
    while (index > 0) {
      index--;
      cache_hits.push_back(dns_cache_hits_[index]);
    }
  }

  // Partition the UrlInfo's into categories.
  for (Snapshot::iterator it(snapshot.begin()); it != snapshot.end(); it++) {
    if (it->second.was_nonexistant()) {
      name_not_found.push_back(it->second);
      continue;
    }
    if (!it->second.was_found())
      continue;  // Still being processed.
    if (TimeDelta() != it->second.benefits_remaining()) {
      network_hits.push_back(it->second);  // With no benefit yet.
      continue;
    }
    if (UrlInfo::kMaxNonNetworkDnsLookupDuration >
      it->second.resolve_duration()) {
      already_cached.push_back(it->second);
      continue;
    }
    // Remaining case is where prefetch benefit was significant, and was used.
    // Since we shot those cases as historical hits, we won't bother here.
  }

  bool brief = false;
#ifdef NDEBUG
  brief = true;
#endif  // NDEBUG

  // Call for display of each table, along with title.
  UrlInfo::GetHtmlTable(cache_hits,
      "Prefetching DNS records produced benefits for ", false, output);
  UrlInfo::GetHtmlTable(cache_evictions,
      "Cache evictions negated DNS prefetching benefits for ", brief, output);
  UrlInfo::GetHtmlTable(network_hits,
      "Prefetching DNS records was not yet beneficial for ", brief, output);
  UrlInfo::GetHtmlTable(already_cached,
      "Previously cached resolutions were found for ", brief, output);
  UrlInfo::GetHtmlTable(name_not_found,
      "Prefetching DNS records revealed non-existance for ", brief, output);
}

UrlInfo* Predictor::AppendToResolutionQueue(
    const GURL& url,
    UrlInfo::ResolutionMotivation motivation) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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

void Predictor::StartSomeQueuedResolutions() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

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

bool Predictor::CongestionControlPerformed(UrlInfo* info) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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

void Predictor::OnLookupFinished(LookupRequest* request, const GURL& url,
                                 bool found) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  LookupFinished(request, url, found);
  pending_lookups_.erase(request);
  delete request;

  StartSomeQueuedResolutions();
}

void Predictor::LookupFinished(LookupRequest* request, const GURL& url,
                               bool found) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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

void Predictor::DiscardAllResults() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // Delete anything listed so far in this session that shows in about:dns.
  cache_eviction_map_.clear();
  dns_cache_hits_.clear();
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
  DCHECK(assignees.size() <= max_concurrent_dns_lookups_);
  results_.clear();
  // Put back in the names being worked on.
  for (Results::iterator it = assignees.begin(); assignees.end() != it; ++it) {
    DCHECK(it->second.is_marked_to_delete());
    results_[it->first] = it->second;
  }
}

void Predictor::TrimReferrers() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  std::vector<GURL> urls;
  for (Referrers::const_iterator it = referrers_.begin();
       it != referrers_.end(); ++it)
    urls.push_back(it->first);
  for (size_t i = 0; i < urls.size(); ++i)
    if (!referrers_[urls[i]].Trim())
      referrers_.erase(urls[i]);
}

void Predictor::SerializeReferrers(ListValue* referral_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  referral_list->Clear();
  referral_list->Append(new FundamentalValue(DNS_REFERRER_VERSION));
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

void Predictor::DeserializeReferrers(const ListValue& referral_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  int format_version = -1;
  if (referral_list.GetSize() > 0 &&
      referral_list.GetInteger(0, &format_version) &&
      format_version == DNS_REFERRER_VERSION) {
    for (size_t i = 1; i < referral_list.GetSize(); ++i) {
      ListValue* motivator;
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


//------------------------------------------------------------------------------

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

}  // namespace chrome_browser_net
