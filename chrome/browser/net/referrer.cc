// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/referrer.h"

#include <limits.h>

#include "base/logging.h"

namespace chrome_browser_net {

//------------------------------------------------------------------------------
// Smoothing parameter for updating subresource_use_rate_.

// We always combine our old expected value, weighted by some factor, with the
// new expected value Enew.  The new "expected value" is the number of actual
// connections made due to the curernt navigations.
// This means the formula (in a concise form) is:
// Eupdated = Eold * W  + Enew * (1 - W)
// That means that IF we end up needing to connect, we should apply the formula:
// Pupdated = Pold * W  +  Enew * (1 - W)
// If we visit the containing url, but don't end up needing a connection:
// Pupdated = Pold * W
// To achive the above upating algorithm, we end up doing the multiplication
// by W every time we contemplate doing a preconneciton (i.e., when we navigate
// to the containing URL, and consider doing a preconnection), and then IFF we
// learn that we really needed a connection to the subresource, we complete the
// above algorithm by adding the (1 - W) for each connection we make.

// We weight the new expected value by a factor which is in the range of 0.0 to
// 1.0.
static const double kWeightingForOldExpectedValue = 0.66;

// The expected value needed before we actually do a preconnection.
static const double kPreconnectWorthyExpectedValue = 0.7;

// The expected value that we'll need a preconnection when we first see the
// subresource getting fetched.  Very conservative is 0.0, which will mean that
// we have to wait for a while before using preconnection... but we do persist
// results, so we'll have the learned answer in the long run.
static const double kInitialExpectedValue = 0.0;

// static
bool Referrer::use_preconnect_valuations_ = false;

void Referrer::SuggestHost(const GURL& url) {
  // Limit how large our list can get, in case we make mistakes about what
  // hostnames are in sub-resources (example: Some advertisments have a link to
  // the ad agency, and then provide a "surprising" redirect to the advertised
  // entity, which then (mistakenly) appears to be a subresource on the page
  // hosting the ad).
  // TODO(jar): Do experiments to optimize the max count of suggestions.
  static const size_t kMaxSuggestions = 10;

  if (!url.has_host())  // TODO(jar): Is this really needed????
    return;
  DCHECK(url == url.GetWithEmptyPath());
  SubresourceMap::iterator it = find(url);
  if (it != end()) {
    it->second.SubresourceIsNeeded();
    return;
  }

  if (kMaxSuggestions <= size()) {
    DeleteLeastUseful();
    DCHECK(kMaxSuggestions > size());
  }
  (*this)[url].SubresourceIsNeeded();
}

void Referrer::DeleteLeastUseful() {
  // Find the item with the lowest value.  Most important is preconnection_rate,
  // next is latency savings, and last is lifetime (age).
  GURL least_useful_url;
  double lowest_rate_seen = 0.0;
  // We use longs for durations because we will use multiplication on them.
  int64 lowest_latency_seen = 0;  // Duration in milliseconds.
  int64 least_useful_lifetime = 0;  // Duration in milliseconds.

  const base::Time kNow(base::Time::Now());  // Avoid multiple calls.
  for (SubresourceMap::iterator it = begin(); it != end(); ++it) {
    int64 lifetime = (kNow - it->second.birth_time()).InMilliseconds();
    int64 latency = it->second.latency().InMilliseconds();
    double rate = it->second.subresource_use_rate();
    if (least_useful_url.has_host()) {
      if (rate > lowest_rate_seen)
        continue;
      if (!latency && !lowest_latency_seen) {
        // Older name is less useful.
        if (lifetime <= least_useful_lifetime)
          continue;
      } else {
        // Compare the ratios:
        // latency/lifetime
        // vs.
        // lowest_latency_seen/least_useful_lifetime
        // by cross multiplying (to avoid integer division hassles).  Overflow's
        // won't happen until both latency and lifetime pass about 49 days.
        if (latency * least_useful_lifetime >
            lowest_latency_seen * lifetime) {
          continue;
        }
      }
    }
    least_useful_url = it->first;
    lowest_rate_seen = rate;
    lowest_latency_seen = latency;
    least_useful_lifetime = lifetime;
  }
  erase(least_useful_url);
  // Note: there is a small chance that we will discard a least_useful_url
  // that is currently being prefetched because it *was* in this referer list.
  // In that case, when a benefit appears in AccrueValue() below, we are careful
  // to check before accessing the member.
}

void Referrer::AccrueValue(const base::TimeDelta& delta,
                           const GURL& url) {
  SubresourceMap::iterator it = find(url);
  // Be careful that we weren't evicted from this referrer in DeleteLeastUseful.
  if (it != end())
    it->second.AccrueValue(delta);
}

bool Referrer::Trim() {
  bool has_some_latency_left = false;
  for (SubresourceMap::iterator it = begin(); it != end(); ++it)
    if (it->second.Trim())
      has_some_latency_left = true;
  return has_some_latency_left;
}

bool ReferrerValue::Trim() {
  int64 latency_ms = latency_.InMilliseconds() / 2;
  latency_ = base::TimeDelta::FromMilliseconds(latency_ms);
  return latency_ms > 0 ||
      subresource_use_rate_ > kPreconnectWorthyExpectedValue / 2;
}


void Referrer::Deserialize(const Value& value) {
  if (value.GetType() != Value::TYPE_LIST)
    return;
  const ListValue* subresource_list(static_cast<const ListValue*>(&value));
  size_t index = 0;  // Bounds checking is done by subresource_list->Get*().
  while (true) {
    std::string url_spec;
    if (!subresource_list->GetString(index++, &url_spec))
      return;
    int latency_ms;
    if (!subresource_list->GetInteger(index++, &latency_ms))
      return;
    double rate;
    if (!subresource_list->GetReal(index++, &rate))
      return;

    GURL url(url_spec);
    base::TimeDelta latency = base::TimeDelta::FromMilliseconds(latency_ms);
    // TODO(jar): We could be more direct, and change birth date or similar to
    // show that this is a resurrected value we're adding in.  I'm not yet sure
    // of how best to optimize the learning and pruning (Trim) algorithm at this
    // level, so for now, we just suggest subresources, which leaves them all
    // with the same birth date (typically start of process).
    SuggestHost(url);
    AccrueValue(latency, url);
    (*this)[url].SetSubresourceUseRate(rate);
  }
}

Value* Referrer::Serialize() const {
  ListValue* subresource_list(new ListValue);
  for (const_iterator it = begin(); it != end(); ++it) {
    StringValue* url_spec(new StringValue(it->first.spec()));
    int latency_integer = static_cast<int>(it->second.latency().
                                           InMilliseconds());
    // Watch out for overflow in the above static_cast!  Check to see if we went
    // negative, and just use a "big" value.  The value seems unimportant once
    // we get to such high latencies.  Probable cause of high latency is a bug
    // in other code, so also do a DCHECK.
    DCHECK_GE(latency_integer, 0);
    if (latency_integer < 0)
      latency_integer = INT_MAX;
    FundamentalValue* latency(new FundamentalValue(latency_integer));
    FundamentalValue* rate(new FundamentalValue(
        it->second.subresource_use_rate()));

    subresource_list->Append(url_spec);
    subresource_list->Append(latency);
    subresource_list->Append(rate);
  }
  return subresource_list;
}

//------------------------------------------------------------------------------

ReferrerValue::ReferrerValue()
    : birth_time_(base::Time::Now()),
      navigation_count_(0),
      preconnection_count_(0),
      subresource_use_rate_(kInitialExpectedValue) {
}

void ReferrerValue::SubresourceIsNeeded() {
  DCHECK_GE(kWeightingForOldExpectedValue, 0);
  DCHECK_LE(kWeightingForOldExpectedValue, 1.0);
  ++navigation_count_;
  subresource_use_rate_ += 1 - kWeightingForOldExpectedValue;
}

bool ReferrerValue::IsPreconnectWorthDoing() {
  bool preconnecting = kPreconnectWorthyExpectedValue < subresource_use_rate_;
  if (preconnecting)
    ++preconnection_count_;
  subresource_use_rate_ *= kWeightingForOldExpectedValue;
  // Note: the use rate is temporarilly possibly incorect, as we need to find
  // out if we really end up connecting.  This will happen in a few hundred
  // milliseconds (when content arrives, etc.).
  return preconnecting;
}

}  // namespace chrome_browser_net
