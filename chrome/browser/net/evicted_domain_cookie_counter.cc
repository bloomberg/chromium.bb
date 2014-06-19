// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/evicted_domain_cookie_counter.h"

#include <algorithm>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/google/core/browser/google_util.h"
#include "net/cookies/canonical_cookie.h"

namespace chrome_browser_net {

using base::Time;
using base::TimeDelta;

namespace {

const size_t kMaxEvictedDomainCookies = 500;
const size_t kPurgeEvictedDomainCookies = 100;

class DelegateImpl : public EvictedDomainCookieCounter::Delegate {
 public:
  DelegateImpl();

  // EvictedDomainCookieCounter::Delegate implementation.
  virtual void Report(
      const EvictedDomainCookieCounter::EvictedCookie& evicted_cookie,
      const Time& reinstatement_time) OVERRIDE;
  virtual Time CurrentTime() const OVERRIDE;
};

DelegateImpl::DelegateImpl() {}

void DelegateImpl::Report(
    const EvictedDomainCookieCounter::EvictedCookie& evicted_cookie,
    const Time& reinstatement_time) {
  TimeDelta reinstatement_delay(
      reinstatement_time - evicted_cookie.eviction_time);
  // Need to duplicate HISTOGRAM_CUSTOM_TIMES(), since it is a macro that
  // defines a static variable.
  if (evicted_cookie.is_google) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Cookie.ReinstatedCookiesGoogle",
                               reinstatement_delay,
                               TimeDelta::FromSeconds(1),
                               TimeDelta::FromDays(7),
                               50);
  } else {
    UMA_HISTOGRAM_CUSTOM_TIMES("Cookie.ReinstatedCookiesOther",
                               reinstatement_delay,
                               TimeDelta::FromSeconds(1),
                               TimeDelta::FromDays(7),
                               50);
  }
}

Time DelegateImpl::CurrentTime() const {
  return Time::Now();
}

}  // namespace

EvictedDomainCookieCounter::EvictedDomainCookieCounter(
    scoped_refptr<net::CookieMonster::Delegate> next_cookie_monster_delegate)
    : next_cookie_monster_delegate_(next_cookie_monster_delegate),
      cookie_counter_delegate_(new DelegateImpl),
      max_size_(kMaxEvictedDomainCookies),
      purge_count_(kPurgeEvictedDomainCookies) {
}

EvictedDomainCookieCounter::EvictedDomainCookieCounter(
    scoped_refptr<net::CookieMonster::Delegate> next_cookie_monster_delegate,
    scoped_ptr<Delegate> cookie_counter_delegate,
    size_t max_size,
    size_t purge_count)
    : next_cookie_monster_delegate_(next_cookie_monster_delegate),
      cookie_counter_delegate_(cookie_counter_delegate.Pass()),
      max_size_(max_size),
      purge_count_(purge_count) {
  DCHECK(cookie_counter_delegate_);
  DCHECK_LT(purge_count, max_size_);
}

EvictedDomainCookieCounter::~EvictedDomainCookieCounter() {
  STLDeleteContainerPairSecondPointers(evicted_cookies_.begin(),
                                       evicted_cookies_.end());
}

size_t EvictedDomainCookieCounter::GetStorageSize() const {
  return evicted_cookies_.size();
}

void EvictedDomainCookieCounter::OnCookieChanged(
    const net::CanonicalCookie& cookie,
    bool removed,
    ChangeCause cause) {
  EvictedDomainCookieCounter::EvictedCookieKey key(GetKey(cookie));
  Time current_time(cookie_counter_delegate_->CurrentTime());
  if (removed) {
    if (cause == net::CookieMonster::Delegate::CHANGE_COOKIE_EVICTED)
      StoreEvictedCookie(key, cookie, current_time);
  } else {  // Includes adds or updates.
    ProcessNewCookie(key, cookie, current_time);
  }

  if (next_cookie_monster_delegate_.get())
    next_cookie_monster_delegate_->OnCookieChanged(cookie, removed, cause);
}

void EvictedDomainCookieCounter::OnLoaded() {
  if (next_cookie_monster_delegate_.get())
    next_cookie_monster_delegate_->OnLoaded();
}

// static
EvictedDomainCookieCounter::EvictedCookieKey
    EvictedDomainCookieCounter::GetKey(const net::CanonicalCookie& cookie) {
  return cookie.Domain() + ";" + cookie.Path() + ";" + cookie.Name();
}

// static
bool EvictedDomainCookieCounter::CompareEvictedCookie(
    const EvictedCookieMap::iterator evicted_cookie1,
    const EvictedCookieMap::iterator evicted_cookie2) {
  return evicted_cookie1->second->eviction_time
      < evicted_cookie2->second->eviction_time;
}

void EvictedDomainCookieCounter::GarbageCollect(const Time& current_time) {
  if (evicted_cookies_.size() <= max_size_)
    return;

  // From |evicted_cookies_|, removed all expired cookies, and remove cookies
  // with the oldest |eviction_time| so that |size_goal| is attained.
  size_t size_goal = max_size_ - purge_count_;
  // Bound on number of non-expired cookies to remove.
  size_t remove_quota = evicted_cookies_.size() - size_goal;
  DCHECK_GT(remove_quota, 0u);

  std::vector<EvictedCookieMap::iterator> remove_list;
  remove_list.reserve(evicted_cookies_.size());

  EvictedCookieMap::iterator it = evicted_cookies_.begin();
  while (it != evicted_cookies_.end()) {
    if (it->second->is_expired(current_time)) {
      delete it->second;
      evicted_cookies_.erase(it++); // Post-increment idiom for in-loop removal.
      if (remove_quota)
        --remove_quota;
    } else {
      if (remove_quota)  // Don't bother storing if quota met.
        remove_list.push_back(it);
      ++it;
    }
  }

  // Free the oldest |remove_quota| non-expired cookies.
  std::partial_sort(remove_list.begin(), remove_list.begin() + remove_quota,
                    remove_list.end(), CompareEvictedCookie);
  for (size_t i = 0; i < remove_quota; ++i) {
    delete remove_list[i]->second;
    evicted_cookies_.erase(remove_list[i]);
  }

  // Apply stricter check if non-expired cookies were deleted.
  DCHECK(remove_quota ? evicted_cookies_.size() == size_goal :
         evicted_cookies_.size() <= size_goal);
}

void EvictedDomainCookieCounter::StoreEvictedCookie(
    const EvictedCookieKey& key,
    const net::CanonicalCookie& cookie,
    const Time& current_time) {
  bool is_google = google_util::IsGoogleHostname(
      cookie.Domain(), google_util::ALLOW_SUBDOMAIN);
  EvictedCookie* evicted_cookie =
      new EvictedCookie(current_time, cookie.ExpiryDate(), is_google);
  std::pair<EvictedCookieMap::iterator, bool> prev_entry =
      evicted_cookies_.insert(
          EvictedCookieMap::value_type(key, evicted_cookie));
  if (!prev_entry.second) {
    NOTREACHED();
    delete prev_entry.first->second;
    prev_entry.first->second = evicted_cookie;
  }

  GarbageCollect(current_time);
}

void EvictedDomainCookieCounter::ProcessNewCookie(
    const EvictedCookieKey& key,
    const net::CanonicalCookie& cc,
    const Time& current_time) {
  EvictedCookieMap::iterator it = evicted_cookies_.find(key);
  if (it != evicted_cookies_.end()) {
    if (!it->second->is_expired(current_time))  // Reinstatement.
      cookie_counter_delegate_->Report(*it->second, current_time);
    delete it->second;
    evicted_cookies_.erase(it);
  }
}

}  // namespace chrome_browser_net
