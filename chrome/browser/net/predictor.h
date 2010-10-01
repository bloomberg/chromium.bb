// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Predictor object is instantiated once in the browser process, and manages
// both preresolution of hostnames, as well as TCP/IP preconnection to expected
// subresources.
// Most hostname lists are provided by the renderer processes, and include URLs
// that *might* be used in the near future by the browsing user.  One goal of
// this class is to cause the underlying DNS structure to lookup a hostname
// before it is really needed, and hence reduce latency in the standard lookup
// paths.
// Subresource relationships are usually acquired from the referrer field in a
// navigation.  A subresource URL may be associated with a referrer URL.  Later
// navigations may, if the likelihood of needing the subresource is high enough,
// cause this module to speculatively create a TCP/IP connection. If there is
// only a low likelihood, then a DNS pre-resolution operation may be performed.

#ifndef CHROME_BROWSER_NET_PREDICTOR_H_
#define CHROME_BROWSER_NET_PREDICTOR_H_
#pragma once

#include <map>
#include <queue>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "chrome/browser/net/url_info.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/common/net/predictor_common.h"
#include "net/base/host_port_pair.h"

class ListValue;

namespace net {
class HostResolver;
}  // namespace net

namespace chrome_browser_net {

typedef chrome_common_net::UrlList UrlList;
typedef chrome_common_net::NameList NameList;
typedef std::map<GURL, UrlInfo> Results;

// Note that Predictor is not thread safe, and must only be called from
// the IO thread. Failure to do so will result in a DCHECK at runtime.
class Predictor : public base::RefCountedThreadSafe<Predictor> {
 public:
  // A version number for prefs that are saved. This should be incremented when
  // we change the format so that we discard old data.
  enum { PREDICTOR_REFERRER_VERSION = 2 };

  // Depending on the expected_subresource_use_, we may either make a TCP/IP
  // preconnection, or merely pre-resolve the hostname via DNS (or even do
  // nothing).  The following are the threasholds for taking those actions.
  static const double kPreconnectWorthyExpectedValue;
  static const double kDNSPreresolutionWorthyExpectedValue;
  // Values of expected_subresource_use_ that are less than the following
  // threshold will be discarded when we Trim() the values, such as is done when
  // the process ends, and some values are persisted.
  static const double kPersistWorthyExpectedValue;

  // |max_concurrent| specifies how many concurrent (parallel) prefetches will
  // be performed. Host lookups will be issued through |host_resolver|.
  Predictor(net::HostResolver* host_resolver,
            base::TimeDelta max_queue_delay_ms, size_t max_concurrent,
            bool preconnect_enabled);

  // Cancel pending requests and prevent new ones from being made.
  void Shutdown();

  // In some circumstances, for privacy reasons, all results should be
  // discarded.  This method gracefully handles that activity.
  // Destroy all our internal state, which shows what names we've looked up, and
  // how long each has taken, etc. etc.  We also destroy records of suggesses
  // (cache hits etc.).
  void DiscardAllResults();

  // Add hostname(s) to the queue for processing.
  void ResolveList(const UrlList& urls,
                   UrlInfo::ResolutionMotivation motivation);
  void Resolve(const GURL& url,
               UrlInfo::ResolutionMotivation motivation);

  // Instigate pre-connection to any URLs, or pre-resolution of related host,
  // that we predict will be needed after this navigation (typically
  // more-embedded resources on a page).  This method will actually post a task
  // to do the actual work, so as not to jump ahead of the frame navigation that
  // instigated this activity.
  void PredictFrameSubresources(const GURL& url);

  // The Omnibox has proposed a given url to the user, and if it is a search
  // URL, then it also indicates that this is preconnectable (i.e., we could
  // preconnect to the search server).
  void AnticipateOmniboxUrl(const GURL& url, bool preconnectable);

  // Preconnect a URL and all of its subresource domains.
  void PreconnectUrlAndSubresources(const GURL& url);

  // Record details of a navigation so that we can preresolve the host name
  // ahead of time the next time the users navigates to the indicated host.
  // Should only be called when urls are distinct, and they should already be
  // canonicalized to not have a path.
  void LearnFromNavigation(const GURL& referring_url, const GURL& target_url);

  // Dump HTML table containing list of referrers for about:dns.
  void GetHtmlReferrerLists(std::string* output);

  // Dump the list of currently known referrer domains and related prefetchable
  // domains.
  void GetHtmlInfo(std::string* output);

  // Discard any referrer for which all the suggested host names are currently
  // annotated with no user latency reduction.  Also scale down (diminish) the
  // total benefit of those that did help, so that their reported contribution
  // wll go done by a factor of 2 each time we trim (moving the referrer closer
  // to being discarded at a future Trim).
  void TrimReferrers();

  // Construct a ListValue object that contains all the data in the referrers_
  // so that it can be persisted in a pref.
  void SerializeReferrers(ListValue* referral_list);

  // Process a ListValue that contains all the data from a previous reference
  // list, as constructed by SerializeReferrers(), and add all the identified
  // values into the current referrer list.
  void DeserializeReferrers(const ListValue& referral_list);

  void DeserializeReferrersThenDelete(ListValue* referral_list);

  // For unit test code only.
  size_t max_concurrent_dns_lookups() const {
    return max_concurrent_dns_lookups_;
  }

  // Flag setting to use preconnection instead of just DNS pre-fetching.
  bool preconnect_enabled() const { return preconnect_enabled_; }

  // Put URL in canonical form, including a scheme, host, and port.
  // Returns GURL::EmptyGURL() if the scheme is not http/https or if the url
  // cannot be otherwise canonicalized.
  static GURL CanonicalizeUrl(const GURL& url);

 private:
  friend class base::RefCountedThreadSafe<Predictor>;
  FRIEND_TEST_ALL_PREFIXES(PredictorTest, BenefitLookupTest);
  FRIEND_TEST_ALL_PREFIXES(PredictorTest, ShutdownWhenResolutionIsPendingTest);
  FRIEND_TEST_ALL_PREFIXES(PredictorTest, SingleLookupTest);
  FRIEND_TEST_ALL_PREFIXES(PredictorTest, ConcurrentLookupTest);
  FRIEND_TEST_ALL_PREFIXES(PredictorTest, MassiveConcurrentLookupTest);
  FRIEND_TEST_ALL_PREFIXES(PredictorTest, PriorityQueuePushPopTest);
  FRIEND_TEST_ALL_PREFIXES(PredictorTest, PriorityQueueReorderTest);
  friend class WaitForResolutionHelper;  // For testing.

  class LookupRequest;

  // A simple priority queue for handling host names.
  // Some names that are queued up have |motivation| that requires very rapid
  // handling.  For example, a sub-resource name lookup MUST be done before the
  // actual sub-resource is fetched.  In contrast, a name that was speculatively
  // noted in a page has to be resolved before the user "gets around to"
  // clicking on a link.  By tagging (with a motivation) each push we make into
  // this FIFO queue, the queue can re-order the more important names to service
  // them sooner (relative to some low priority background resolutions).
  class HostNameQueue {
   public:
    HostNameQueue();
    ~HostNameQueue();
    void Push(const GURL& url,
              UrlInfo::ResolutionMotivation motivation);
    bool IsEmpty() const;
    GURL Pop();

  private:
    // The names in the queue that should be serviced (popped) ASAP.
    std::queue<GURL> rush_queue_;
    // The names in the queue that should only be serviced when rush_queue is
    // empty.
    std::queue<GURL> background_queue_;

  DISALLOW_COPY_AND_ASSIGN(HostNameQueue);
  };

  // A map that is keyed with the host/port that we've learned were the cause
  // of loading additional URLs.  The list of additional targets is held
  // in a Referrer instance, which is a value in this map.
  typedef std::map<GURL, Referrer> Referrers;

  ~Predictor();

  // Perform actual resolution or preconnection to subresources now.  This is
  // an internal worker method that is reached via a post task from
  // PredictFrameSubresources().
  void PrepareFrameSubresources(const GURL& url);

  // Only for testing. Returns true if hostname has been successfully resolved
  // (name found).
  bool WasFound(const GURL& url) const {
    Results::const_iterator it(results_.find(url));
    return (it != results_.end()) &&
            it->second.was_found();
  }

  // Only for testing. Return how long was the resolution
  // or UrlInfo::kNullDuration if it hasn't been resolved yet.
  base::TimeDelta GetResolutionDuration(const GURL& url) {
    if (results_.find(url) == results_.end())
      return UrlInfo::kNullDuration;
    return results_[url].resolve_duration();
  }

  // Only for testing;
  size_t peak_pending_lookups() const { return peak_pending_lookups_; }

  // Access method for use by async lookup request to pass resolution result.
  void OnLookupFinished(LookupRequest* request, const GURL& url, bool found);

  // Underlying method for both async and synchronous lookup to update state.
  void LookupFinished(LookupRequest* request,
                      const GURL& url, bool found);

  // Queue hostname for resolution.  If queueing was done, return the pointer
  // to the queued instance, otherwise return NULL.
  UrlInfo* AppendToResolutionQueue(const GURL& url,
      UrlInfo::ResolutionMotivation motivation);

  // Check to see if too much queuing delay has been noted for the given info,
  // which indicates that there is "congestion" or growing delay in handling the
  // resolution of names.  Rather than letting this congestion potentially grow
  // without bounds, we abandon our queued efforts at pre-resolutions in such a
  // case.
  // To do this, we will recycle |info|, as well as all queued items, back to
  // the state they had before they were queued up.  We can't do anything about
  // the resolutions we've already sent off for processing on another thread, so
  // we just let them complete.  On a slow system, subject to congestion, this
  // will greatly reduce the number of resolutions done, but it will assure that
  // any resolutions that are done, are in a timely and hence potentially
  // helpful manner.
  bool CongestionControlPerformed(UrlInfo* info);

  // Take lookup requests from work_queue_ and tell HostResolver to look them up
  // asynchronously, provided we don't exceed concurrent resolution limit.
  void StartSomeQueuedResolutions();

  // work_queue_ holds a list of names we need to look up.
  HostNameQueue work_queue_;

  // results_ contains information for existing/prior prefetches.
  Results results_;

  // For each URL that we might navigate to (that we've "learned about")
  // we have a Referrer list. Each Referrer list has all hostnames we need to
  // pre-resolve when there is a navigation to the orginial hostname.
  Referrers referrers_;

  std::set<LookupRequest*> pending_lookups_;

  // For testing, to verify that we don't exceed the limit.
  size_t peak_pending_lookups_;

  // When true, we don't make new lookup requests.
  bool shutdown_;

  // The number of concurrent lookups currently allowed.
  const size_t max_concurrent_dns_lookups_;

  // The maximum queueing delay that is acceptable before we enter congestion
  // reduction mode, and discard all queued (but not yet assigned) resolutions.
  const base::TimeDelta max_dns_queue_delay_;

  // The host resolver we warm DNS entries for.
  net::HostResolver* const host_resolver_;

  // Are we currently using preconnection, rather than just DNS resolution, for
  // subresources and omni-box search URLs.
  bool preconnect_enabled_;

  // Most recent suggestion from Omnibox provided via AnticipateOmniboxUrl().
  std::string last_omnibox_host_;

  // The time when the last preresolve was done for last_omnibox_host_.
  base::TimeTicks last_omnibox_preresolve_;

  // The number of consecutive requests to AnticipateOmniboxUrl() that suggested
  // preconnecting (because it was to a search service).
  int consecutive_omnibox_preconnect_count_;

  // The time when the last preconnection was requested to a search service.
  base::TimeTicks last_omnibox_preconnect_;

  DISALLOW_COPY_AND_ASSIGN(Predictor);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PREDICTOR_H_
