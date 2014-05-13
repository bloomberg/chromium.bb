// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_EVICTED_DOMAIN_COOKIE_COUNTER_H_
#define CHROME_BROWSER_NET_EVICTED_DOMAIN_COOKIE_COUNTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/cookies/cookie_monster.h"

namespace net {
class CanonicalCookie;
}  // namespace net

namespace chrome_browser_net {

// The Evicted Domain Cookie Counter generates statistics on "wrongly evicted"
// cookies, i.e., cookies that were "evicted" (on reaching domain cookie limit)
// but are then "reinstated" later because they were important. A specific
// scenario is as follows: a long-lived login session cookie gets evicted owing
// to its age, thereby forcing the user to lose session, and is reinstated when
// the user re-authenticates.
//
// A solution to the above problem is the Cookie Priority Field, which enables
// servers to protect important cookies, thereby decreasing the chances that
// these cookies are wrongly evicted. To measure the effectiveness of this
// solution, we will compare eviction user metrics before vs. after the fix.
//
// Specifically, we wish to record user metrics on "reinstatement delay", i.e.,
// the duration between eviction and reinstatement of cookie. We expect that
// after the fix, average reinstatement delays will increase, since low priority
// cookies are less likely to be reinstated after eviction.
//
// Metrics for Google domains are tracked separately.
//
class EvictedDomainCookieCounter : public net::CookieMonster::Delegate {
 public:
  // Structure to store sanitized data from CanonicalCookie.
  struct EvictedCookie {
    EvictedCookie(base::Time eviction_time_in,
                  base::Time expiry_time_in,
                  bool is_google_in)
        : eviction_time(eviction_time_in),
          expiry_time(expiry_time_in),
          is_google(is_google_in) {}

    bool is_expired(const base::Time& current_time) const {
      return !expiry_time.is_null() && current_time >= expiry_time;
    }

    base::Time eviction_time;
    base::Time expiry_time;
    bool is_google;
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when a stored evicted cookie is reinstated.
    virtual void Report(const EvictedCookie& evicted_cookie,
                        const base::Time& reinstatement_time) = 0;

    // Getter of time is placed here to enable mocks.
    virtual base::Time CurrentTime() const = 0;
  };

  // |next_cookie_monster_delegate| can be NULL.
  explicit EvictedDomainCookieCounter(
      scoped_refptr<net::CookieMonster::Delegate> next_cookie_monster_delegate);

  // Constructor exposed for testing only.
  EvictedDomainCookieCounter(
      scoped_refptr<net::CookieMonster::Delegate> next_cookie_monster_delegate,
      scoped_ptr<Delegate> cookie_counter_delegate,
      size_t max_size,
      size_t purge_count);

  // Returns the number of evicted cookies stored.
  size_t GetStorageSize() const;

  // CookieMonster::Delegate implementation.
  virtual void OnCookieChanged(const net::CanonicalCookie& cookie,
                               bool removed,
                               ChangeCause cause) OVERRIDE;
  virtual void OnLoaded() OVERRIDE;

 private:
  // Identifier of an evicted cookie.
  typedef std::string EvictedCookieKey;

  // Storage class of evicted cookie.
  typedef std::map<EvictedCookieKey, EvictedCookie*> EvictedCookieMap;

  virtual ~EvictedDomainCookieCounter();

  // Computes key for |cookie| compatible with CanonicalCookie::IsEquivalent(),
  // i.e., IsEquivalent(a, b) ==> GetKey(a) == GetKey(b).
  static EvictedCookieKey GetKey(const net::CanonicalCookie& cookie);

  // Comparator for sorting, to make recently evicted cookies appear earlier.
  static bool CompareEvictedCookie(
      const EvictedCookieMap::iterator evicted_cookie1,
      const EvictedCookieMap::iterator evicted_cookie2);

  // If too many evicted cookies are stored, delete the expired ones, then
  // delete cookies that were evicted the longest, until size limit reached.
  void GarbageCollect(const base::Time& current_time);

  // Called when a cookie is evicted. Adds the evicted cookie to storage,
  // possibly replacing an existing equivalent cookie.
  void StoreEvictedCookie(const EvictedCookieKey& key,
                          const net::CanonicalCookie& cookie,
                          const base::Time& current_time);

  // Called when a new cookie is added. If reinstatement occurs, then notifies
  // |cookie_counter_delegate_| and then removes the evicted cookie.
  void ProcessNewCookie(const EvictedCookieKey& key,
                        const net::CanonicalCookie& cookie,
                        const base::Time& current_time);

  // Another delegate to forward events to.
  scoped_refptr<net::CookieMonster::Delegate> next_cookie_monster_delegate_;

  scoped_ptr<Delegate> cookie_counter_delegate_;

  EvictedCookieMap evicted_cookies_;

  // Capacity of the evicted cookie storage, before garbage collection occurs.
  const size_t max_size_;

  // After garbage collection, size reduces to <= |max_size_| - |purge_count_|.
  const size_t purge_count_;

  DISALLOW_COPY_AND_ASSIGN(EvictedDomainCookieCounter);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_EVICTED_DOMAIN_COOKIE_COUNTER_H_
