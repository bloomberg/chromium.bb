// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/referrer.h"

#include <limits.h>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/net/predictor.h"

namespace chrome_browser_net {

//------------------------------------------------------------------------------
// Smoothing parameter for updating subresource_use_rate_.

// We always combine our old expected value, weighted by some factor W (we use
// kWeightingForOldExpectedValue), with the new expected value Enew.  The new
// "expected value" is the number of actual connections made due to the current
// navigations.
// That means that IF we end up needing to connect, we should apply the formula:
// Eupdated = Eold * W  +  Enew * (1 - W)
// If we visit the containing url, but don't end up needing a connection, then
// Enew == 0, so we use the formula:
// Eupdated = Eold * W
// To achieve the above updating algorithm, we end up doing the multiplication
// by W every time we contemplate doing a preconnection (i.e., when we navigate
// to the containing URL, and consider doing a preconnection), and then IFF we
// learn that we really needed a connection to the subresource, we complete the
// above algorithm by adding the (1 - W) for each connection we make.

// We weight the new expected value by a factor which is in the range of 0.0 to
// 1.0.
static const double kWeightingForOldExpectedValue = 0.66;

// To estimate the expected value of the number of connections that we'll need
// when a referrer is navigated to, we start with the following rather low
// initial value.  Each time we do indeed (again) need the subresource, this
// value will get increased.  Each time we navigate to the refererrer but never
// end up needing this subresource, the value will decrease.
// Very conservative is 0.0, which will mean that we have to wait for a while
// before doing much speculative acvtivity... but we do persist results, so
// we'll save the asymptotic (correct?) learned answer in the long run.
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
  // and least is lifetime (age).
  GURL least_useful_url;
  double lowest_rate_seen = 0.0;
  // We use longs for durations because we will use multiplication on them.
  int64 least_useful_lifetime = 0;  // Duration in milliseconds.

  const base::Time kNow(base::Time::Now());  // Avoid multiple calls.
  for (SubresourceMap::iterator it = begin(); it != end(); ++it) {
    int64 lifetime = (kNow - it->second.birth_time()).InMilliseconds();
    double rate = it->second.subresource_use_rate();
    if (least_useful_url.has_host()) {
      if (rate > lowest_rate_seen)
        continue;
      if (lifetime <= least_useful_lifetime)
        continue;
    }
    least_useful_url = it->first;
    lowest_rate_seen = rate;
    least_useful_lifetime = lifetime;
  }
  if (least_useful_url.has_host())
    erase(least_useful_url);
}

bool Referrer::Trim() {
  std::vector<GURL> discarded_urls;
  for (SubresourceMap::iterator it = begin(); it != end(); ++it)
    if (!it->second.Trim())
      discarded_urls.push_back(it->first);
  for (size_t i = 0; i < discarded_urls.size(); ++i)
    erase(discarded_urls[i]);
  return size() > 0;
}

bool ReferrerValue::Trim() {
  subresource_use_rate_ /= 2.0;
  return subresource_use_rate_ > Predictor::kPersistWorthyExpectedValue;
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
    double rate;
    if (!subresource_list->GetDouble(index++, &rate))
      return;

    GURL url(url_spec);
    // TODO(jar): We could be more direct, and change birth date or similar to
    // show that this is a resurrected value we're adding in.  I'm not yet sure
    // of how best to optimize the learning and pruning (Trim) algorithm at this
    // level, so for now, we just suggest subresources, which leaves them all
    // with the same birth date (typically start of process).
    SuggestHost(url);
    (*this)[url].SetSubresourceUseRate(rate);
  }
}

Value* Referrer::Serialize() const {
  ListValue* subresource_list(new ListValue);
  for (const_iterator it = begin(); it != end(); ++it) {
    StringValue* url_spec(new StringValue(it->first.spec()));
    FundamentalValue* rate(new FundamentalValue(
        it->second.subresource_use_rate()));

    subresource_list->Append(url_spec);
    subresource_list->Append(rate);
  }
  return subresource_list;
}

//------------------------------------------------------------------------------

ReferrerValue::ReferrerValue()
    : birth_time_(base::Time::Now()),
      navigation_count_(0),
      preconnection_count_(0),
      preresolution_count_(0),
      subresource_use_rate_(kInitialExpectedValue) {
}

void ReferrerValue::SubresourceIsNeeded() {
  DCHECK_GE(kWeightingForOldExpectedValue, 0);
  DCHECK_LE(kWeightingForOldExpectedValue, 1.0);
  ++navigation_count_;
  subresource_use_rate_ += 1 - kWeightingForOldExpectedValue;
}

void ReferrerValue::ReferrerWasObserved() {
  subresource_use_rate_ *= kWeightingForOldExpectedValue;
  // Note: the use rate is temporarilly possibly incorect, as we need to find
  // out if we really end up connecting.  This will happen in a few hundred
  // milliseconds (when content arrives, etc.).
  // Value of subresource_use_rate_ should be sampled before this call.
}

}  // namespace chrome_browser_net
