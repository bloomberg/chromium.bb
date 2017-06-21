// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_STORE_IOS_H_
#define IOS_NET_COOKIES_COOKIE_STORE_IOS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "ios/net/cookies/cookie_cache.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "url/gurl.h"

@class NSHTTPCookie;
@class NSHTTPCookieStorage;
@class NSArray;

namespace net {

class CookieCreationTimeManager;

// Observer for changes on |NSHTTPCookieStorge sharedHTTPCookieStorage|.
class CookieNotificationObserver {
 public:
  // Called when any cookie is added, deleted or changed in
  // |NSHTTPCookieStorge sharedHTTPCookieStorage|.
  virtual void OnSystemCookiesChanged() = 0;
};

// The CookieStoreIOS is an implementation of CookieStore relying on
// NSHTTPCookieStorage, ensuring that the cookies are consistent between the
// network stack and NSHTTPCookieStorage. CookieStoreIOS is not thread safe.
//
// CookieStoreIOS is created synchronized with the system cookie store -
// changes are written directly to the system cookie store, then propagated to
// the backing store by OnSystemCookiesChanged, which is called by the system
// store once the change to the system store is written back.
// For not synchronized CookieStore, please see CookieStoreIOSPersistent.
class CookieStoreIOS : public net::CookieStore,
                       public CookieNotificationObserver {
 public:
  // Creates an instance of CookieStoreIOS that is generated from the cookies
  // stored in |cookie_storage|. The CookieStoreIOS uses the |cookie_storage|
  // as its default backend and is initially synchronized with it.
  // Apple does not persist the cookies' creation dates in NSHTTPCookieStorage,
  // so callers should not expect these values to be populated.
  explicit CookieStoreIOS(NSHTTPCookieStorage* cookie_storage);

  ~CookieStoreIOS() override;

  enum CookiePolicy { ALLOW, BLOCK };

  // Must be called when the state of
  // |NSHTTPCookieStorage sharedHTTPCookieStorage| changes.
  // Affects only those CookieStoreIOS instances that are backed by
  // |NSHTTPCookieStorage sharedHTTPCookieStorage|.
  static void NotifySystemCookiesChanged();

  // Only one cookie store may enable metrics.
  void SetMetricsEnabled();

  // Inherited CookieStore methods.
  void SetCookieWithOptionsAsync(const GURL& url,
                                 const std::string& cookie_line,
                                 const net::CookieOptions& options,
                                 const SetCookiesCallback& callback) override;
  void SetCookieWithDetailsAsync(const GURL& url,
                                 const std::string& name,
                                 const std::string& value,
                                 const std::string& domain,
                                 const std::string& path,
                                 base::Time creation_time,
                                 base::Time expiration_time,
                                 base::Time last_access_time,
                                 bool secure,
                                 bool http_only,
                                 CookieSameSite same_site,
                                 CookiePriority priority,
                                 const SetCookiesCallback& callback) override;
  void SetCanonicalCookieAsync(std::unique_ptr<CanonicalCookie> cookie,
                               bool secure_source,
                               bool modify_http_only,
                               const SetCookiesCallback& callback) override;
  void GetCookiesWithOptionsAsync(const GURL& url,
                                  const net::CookieOptions& options,
                                  const GetCookiesCallback& callback) override;
  void GetCookieListWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookieListCallback& callback) override;
  void GetAllCookiesAsync(const GetCookieListCallback& callback) override;
  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         const base::Closure& callback) override;
  void DeleteCanonicalCookieAsync(
      const CanonicalCookie& cookie,
      const DeleteCallback& callback) override;
  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    const DeleteCallback& callback) override;
  void DeleteAllCreatedBetweenWithPredicateAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const CookiePredicate& predicate,
      const DeleteCallback& callback) override;
  void DeleteSessionCookiesAsync(const DeleteCallback& callback) override;
  void FlushStore(const base::Closure& callback) override;

  std::unique_ptr<CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) override;

  bool IsEphemeral() override;

 protected:
  CookieStoreIOS(net::CookieMonster::PersistentCookieStore* persistent_store,
                 NSHTTPCookieStorage* system_store);

  // These three functions are used for wrapping user-supplied callbacks given
  // to CookieStoreIOS mutator methods. Given a callback, they return a new
  // callback that invokes UpdateCachesFromCookieMonster() to schedule an
  // asynchronous synchronization of the cookie cache and then calls the
  // original callback.
  SetCookiesCallback WrapSetCallback(const SetCookiesCallback& callback);
  DeleteCallback WrapDeleteCallback(const DeleteCallback& callback);
  base::Closure WrapClosure(const base::Closure& callback);

  bool metrics_enabled() { return metrics_enabled_; }

  net::CookieMonster* cookie_monster() { return cookie_monster_.get(); }

  const base::ThreadChecker& thread_checker() { return thread_checker_; }

 private:
  // Cookie filter for DeleteCookiesWithFilter().
  // Takes a cookie and a creation time and returns true the cookie must be
  // deleted.
  typedef base::Callback<bool(NSHTTPCookie*, base::Time)> CookieFilterFunction;

  // Clears the system cookie store.
  void ClearSystemStore();
  // Returns true if the system cookie store policy is
  // |NSHTTPCookieAcceptPolicyAlways|.
  bool SystemCookiesAllowed();
  // Copies the cookies to the backing CookieMonster.
  virtual void WriteToCookieMonster(NSArray* system_cookies);

  // Inherited CookieNotificationObserver methods.
  void OnSystemCookiesChanged() override;

  void DeleteCookiesWithFilter(const CookieFilterFunction& filter,
                               const DeleteCallback& callback);

  std::unique_ptr<net::CookieMonster> cookie_monster_;
  base::scoped_nsobject<NSHTTPCookieStorage> system_store_;
  std::unique_ptr<CookieCreationTimeManager> creation_time_manager_;
  bool metrics_enabled_;
  base::CancelableClosure flush_closure_;

  base::ThreadChecker thread_checker_;

  // Cookie notification methods.
  // The cookie cache is updated from both the system store and the
  // CookieStoreIOS' own mutators. Changes when the CookieStoreIOS is
  // synchronized are signalled by the system store; changes when the
  // CookieStoreIOS is not synchronized are signalled by the appropriate
  // mutators on CookieStoreIOS. The cookie cache tracks the system store when
  // the CookieStoreIOS is synchronized and the CookieStore when the
  // CookieStoreIOS is not synchronized.

  // Fetches any cookies named |name| that would be sent with a request for
  // |url| from the system cookie store and pushes them onto the back of the
  // vector pointed to by |cookies|. Returns true if any cookies were pushed
  // onto the vector, and false otherwise.
  bool GetSystemCookies(const GURL& url,
                        const std::string& name,
                        std::vector<net::CanonicalCookie>* cookies);

  // Updates the cookie cache with the current set of system cookies named
  // |name| that would be sent with a request for |url|. Returns whether the
  // cache changed.
  // |out_removed_cookies|, if not null, will be populated with the cookies that
  // were removed.
  // |out_changed_cookies|, if not null, will be populated with the cookies that
  // were added.
  bool UpdateCacheForCookieFromSystem(
      const GURL& gurl,
      const std::string& name,
      std::vector<net::CanonicalCookie>* out_removed_cookies,
      std::vector<net::CanonicalCookie>* out_added_cookies);

  // Runs all callbacks registered for cookies named |name| that would be sent
  // with a request for |url|.
  // All cookies in |cookies| must have the name equal to |name|.
  void RunCallbacksForCookies(const GURL& url,
                              const std::string& name,
                              const std::vector<net::CanonicalCookie>& cookies,
                              net::CookieStore::ChangeCause cause);

  // Called by this CookieStoreIOS' internal CookieMonster instance when
  // GetAllCookiesForURLAsync() completes. Updates the cookie cache and runs
  // callbacks if the cache changed.
  void GotCookieListFor(const std::pair<GURL, std::string> key,
                        const net::CookieList& cookies);

  // Fetches new values for all (url, name) pairs that have hooks registered,
  // asynchronously invoking callbacks if necessary.
  void UpdateCachesFromCookieMonster();

  // Called after cookies are cleared from NSHTTPCookieStorage so that cookies
  // can be cleared from .binarycookies file. |callback| is called after all the
  // cookies are deleted (with the total number of cookies deleted).
  // |num_deleted| contains the number of cookies deleted from
  // NSHTTPCookieStorage.
  void DidClearNSHTTPCookieStorageCookies(const DeleteCallback& callback,
                                          int num_deleted);
  // Called after cookies are cleared from .binarycookies files. |callback| is
  // called after all the cookies are deleted with the total number of cookies
  // deleted.
  // |num_deleted_from_nshttp_cookie_storage| contains the number of cookies
  // deleted from NSHTTPCookieStorage.
  void DidClearBinaryCookiesFileCookies(
      const DeleteCallback& callback,
      int num_deleted_from_nshttp_cookie_storage);

  // Callback-wrapping:
  // When this CookieStoreIOS object is synchronized with the system store,
  // OnSystemCookiesChanged is responsible for updating the cookie cache (and
  // hence running callbacks).
  //
  // When this CookieStoreIOS object is not synchronized, the various mutator
  // methods (SetCookieWithOptionsAsync &c) instead store their state in a
  // CookieMonster object to be written back when the system store synchronizes.
  // To deliver notifications in a timely manner, the mutators have to ensure
  // that hooks get run, but only after the changes have been written back to
  // CookieMonster. To do this, the mutators wrap the user-supplied callback in
  // a callback which schedules an asynchronous task to synchronize the cache
  // and run callbacks, then calls through to the user-specified callback.
  //
  // These three UpdateCachesAfter functions are responsible for scheduling an
  // asynchronous cache update (using UpdateCachesFromCookieMonster()) and
  // calling the provided callback.

  void UpdateCachesAfterSet(const SetCookiesCallback& callback, bool success);
  void UpdateCachesAfterDelete(const DeleteCallback& callback, int num_deleted);
  void UpdateCachesAfterClosure(const base::Closure& callback);

  // Takes an NSArray of NSHTTPCookies as returns a net::CookieList.
  // The returned cookies are ordered by longest path, then earliest
  // creation date.
  net::CookieList CanonicalCookieListFromSystemCookies(NSArray* cookies);

  // Cached values of system cookies. Only cookies which have an observer added
  // with AddCallbackForCookie are kept in this cache.
  std::unique_ptr<CookieCache> cookie_cache_;

  // Callbacks for cookie changes installed by AddCallbackForCookie.
  std::map<std::pair<GURL, std::string>,
           std::unique_ptr<CookieChangedCallbackList>>
      hook_map_;

  base::WeakPtrFactory<CookieStoreIOS> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreIOS);
};

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_H_
