// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class helps to remember what domains may be needed to be resolved when a
// navigation takes place to a given URL.  This information is gathered when a
// navigation to a subresource identifies a referring URL.
// When future navigations take place to known referrer sites, then we
// speculatively either pre-warm a TCP/IP conneciton, or at a minimum, resolve
// the host name via DNS.

// All access to this class is performed via the Predictor class, which only
// operates on the IO thread.

#ifndef CHROME_BROWSER_NET_REFERRER_H_
#define CHROME_BROWSER_NET_REFERRER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/host_port_pair.h"

namespace chrome_browser_net {

//------------------------------------------------------------------------------
// For each hostname in a Referrer, we have a ReferrerValue.  It indicates
// exactly how much value (re: latency reduction, or connection use) has
// resulted from having this entry.
class ReferrerValue {
 public:
  ReferrerValue();

  // Used during deserialization.
  void SetSubresourceUseRate(double rate) { subresource_use_rate_ = rate; }

  base::TimeDelta latency() const { return latency_; }
  base::Time birth_time() const { return birth_time_; }
  void AccrueValue(const base::TimeDelta& delta) { latency_ += delta; }

  // Record the fact that we navigated to the associated subresource URL.
  void SubresourceIsNeeded();

  // Evaluate if it is worth making this preconnection, and return true if it
  // seems worthwhile.  As a side effect, we also tally the proconnection for
  // statistical purposes only.
  bool IsPreconnectWorthDoing();

  int64 navigation_count() const { return navigation_count_; }
  int64 preconnection_count() const { return preconnection_count_; }
  double subresource_use_rate() const { return subresource_use_rate_; }

  // Reduce the latency figure by a factor of 2, and return true if any latency
  // remains.
  bool Trim();

 private:
  base::TimeDelta latency_;  // Accumulated DNS resolution latency savings.
  const base::Time birth_time_;

  // The number of times this item was navigated to with the fixed referrer.
  int64 navigation_count_;

  // The number of times this item was preconnected as a consequence of its
  // referrer.
  int64 preconnection_count_;

  // A smoothed estimate of the probability that a connection will be needed.
  double subresource_use_rate_;
};

//------------------------------------------------------------------------------
// A list of domain names to pre-resolve. The names are the keys to this map,
// and the values indicate the amount of benefit derived from having each name
// around.
typedef std::map<GURL, ReferrerValue> SubresourceMap;

//------------------------------------------------------------------------------
// There is one Referrer instance for each hostname that has acted as an HTTP
// referer (note mispelling is intentional) for a hostname that was otherwise
// unexpectedly navgated towards ("unexpected" in the sense that the hostname
// was probably needed as a subresource of a page, and was not otherwise
// predictable until the content with the reference arrived).  Most typically,
// an outer page was a page fetched by the user, and this instance lists names
// in SubresourceMap which are subresources and that were needed to complete the
// rendering of the outer page.
class Referrer : public SubresourceMap {
 public:
  Referrer() : use_count_(1) {}
  void IncrementUseCount() { ++use_count_; }
  int64 use_count() const { return use_count_; }

  // Add the indicated url to the list that are resolved via DNS when the user
  // navigates to this referrer.  Note that if the list is long, an entry may be
  // discarded to make room for this insertion.
  void SuggestHost(const GURL& url);

  // Record additional usefulness of having this url in the list.
  // Value is expressed as positive latency of amount delta.
  void AccrueValue(const base::TimeDelta& delta,
                   const GURL& url);

  // Trim the Referrer, by first diminishing (scaling down) the latency for each
  // ReferredValue.
  // Returns true if there are any referring names with some latency left.
  bool Trim();

  // Provide methods for persisting, and restoring contents into a Value class.
  Value* Serialize() const;
  void Deserialize(const Value& referrers);

  static void SetUsePreconnectValuations(bool dns) {
      use_preconnect_valuations_ = dns;
  }

 private:
  // Helper function for pruning list.  Metric for usefulness is "large accrued
  // value," in the form of latency_ savings associated with a host name.  We
  // also give credit for a name being newly added, by scalling latency per
  // lifetime (time since birth).  For instance, when two names have accrued
  // the same latency_ savings, the older one is less valuable as it didn't
  // accrue savings as quickly.
  void DeleteLeastUseful();

  // The number of times this referer had its subresources scaned for possible
  // preconnection or DNS preresolution.
  int64 use_count_;

  // Select between DNS prefetch latency savings, or preconnection valuations
  // for a metric to decide which referers to save.
  static bool use_preconnect_valuations_;

  // We put these into a std::map<>, so we need copy constructors.
  // DISALLOW_COPY_AND_ASSIGN(Referrer);
  // TODO(jar): Consider optimization to use pointers to these instances, and
  // avoid deep copies during re-alloc of the containing map.
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_REFERRER_H_
