// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_STORE_IOS_H_
#define IOS_NET_COOKIES_COOKIE_STORE_IOS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
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
  // Called when the cookie policy changes on
  // |NSHTTPCookieStorge sharedHTTPCookieStorage|.
  virtual void OnSystemCookiePolicyChanged() = 0;
};

// The CookieStoreIOS is an implementation of CookieStore relying on
// NSHTTPCookieStorage, ensuring that the cookies are consistent between the
// network stack and the UIWebViews.
// On iOS, the Chrome CookieMonster is not used in conjunction with UIWebView,
// because UIWebView expects the cookies to be in the shared
// NSHTTPCookieStorage. In particular, javascript may read and write cookies
// there.
// CookieStoreIOS is not thread safe.
//
// At any given time, a CookieStoreIOS can either be synchronized with the
// system cookie store or not. If a CookieStoreIOS is not synchronized with the
// system store, changes are written back to the backing CookieStore. If a
// CookieStoreIOS is synchronized with the system store, changes are written
// directly to the system cookie store, then propagated to the backing store by
// OnSystemCookiesChanged, which is called by the system store once the change
// to the system store is written back.
//
// To unsynchronize, CookieStoreIOS copies the system cookie store into its
// backing CookieStore. To synchronize, CookieStoreIOS clears the system cookie
// store, copies its backing CookieStore into the system cookie store.
class CookieStoreIOS : public net::CookieStore,
                       public CookieNotificationObserver {
 public:
  // Creates a CookieStoreIOS with a default value of
  // |NSHTTPCookieStorage sharedCookieStorage| as the system's cookie store.
  explicit CookieStoreIOS(
      net::CookieMonster::PersistentCookieStore* persistent_store);

  explicit CookieStoreIOS(
      net::CookieMonster::PersistentCookieStore* persistent_store,
      NSHTTPCookieStorage* system_store);

  enum CookiePolicy { ALLOW, BLOCK };

  // Must be called on the thread where CookieStoreIOS instances live.
  // Affects only those CookieStoreIOS instances that are backed by
  // |NSHTTPCookieStorage sharedHTTPCookieStorage|.
  static void SetCookiePolicy(CookiePolicy setting);

  // Create an instance of CookieStoreIOS that is generated from the cookies
  // stored in |cookie_storage|. The CookieStoreIOS uses the |cookie_storage|
  // as its default backend and is initially synchronized with it.
  // Apple does not persist the cookies' creation dates in NSHTTPCookieStorage,
  // so callers should not expect these values to be populated.
  static CookieStoreIOS* CreateCookieStore(NSHTTPCookieStorage* cookie_storage);

  // As there is only one system store, only one CookieStoreIOS at a time may
  // be synchronized with it.
  static void SwitchSynchronizedStore(CookieStoreIOS* old_store,
                                      CookieStoreIOS* new_store);

  // Must be called when the state of
  // |NSHTTPCookieStorage sharedHTTPCookieStorage| changes.
  // Affects only those CookieStoreIOS instances that are backed by
  // |NSHTTPCookieStorage sharedHTTPCookieStorage|.
  static void NotifySystemCookiesChanged();

  // Unsynchronizes the cookie store if it is currently synchronized.
  void UnSynchronize();

  // Only one cookie store may enable metrics.
  void SetMetricsEnabled();

  // Sets the delay between flushes. Only used in tests.
  void set_flush_delay_for_testing(base::TimeDelta delay) {
    flush_delay_ = delay;
  }

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
                                 bool same_site,
                                 bool enforce_strict_secure,
                                 CookiePriority priority,
                                 const SetCookiesCallback& callback) override;
  void GetCookiesWithOptionsAsync(const GURL& url,
                                  const net::CookieOptions& options,
                                  const GetCookiesCallback& callback) override;
  void GetAllCookiesForURLAsync(const GURL& url,
                                const GetCookieListCallback& callback) override;
  void GetAllCookiesAsync(const GetCookieListCallback& callback) override;
  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         const base::Closure& callback) override;
  void DeleteCanonicalCookieAsync(
      const CanonicalCookie& cookie,
      const DeleteCallback& callback) override;
  net::CookieMonster* GetCookieMonster() override;
  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    const DeleteCallback& callback) override;
  void DeleteAllCreatedBetweenForHostAsync(
      const base::Time delete_begin,
      const base::Time delete_end,
      const GURL& url,
      const DeleteCallback& callback) override;
  void DeleteSessionCookiesAsync(const DeleteCallback& callback) override;
  void FlushStore(const base::Closure& callback) override;

  scoped_ptr<CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) override;

 protected:
  ~CookieStoreIOS() override;

 private:
  // For tests.
  friend struct CookieStoreIOSTestTraits;

  enum SynchronizationState {
    NOT_SYNCHRONIZED,  // Uses CookieMonster as backend.
    SYNCHRONIZING,     // Moves from NSHTTPCookieStorage to CookieMonster.
    SYNCHRONIZED       // Uses NSHTTPCookieStorage as backend.
  };

  // Cookie fliter for DeleteCookiesWithFilter().
  // Takes a cookie and a creation time and returns true if the cookie must be
  // deleted.
  typedef base::Callback<bool(NSHTTPCookie*, base::Time)> CookieFilterFunction;

  // Clears the system cookie store.
  void ClearSystemStore();
  // Changes the synchronization of the store.
  // If |synchronized| is true, then the system cookie store is used as a
  // backend, else |cookie_monster_| is used. Cookies are moved from one to
  // the other accordingly.
  void SetSynchronizedWithSystemStore(bool synchronized);
  // Returns true if the system cookie store policy is
  // |NSHTTPCookieAcceptPolicyAlways|.
  bool SystemCookiesAllowed();
  // Converts |cookies| to NSHTTPCookie and add them to the system store.
  void AddCookiesToSystemStore(const net::CookieList& cookies);
  // Copies the cookies to the backing CookieMonster. If the cookie store is not
  // synchronized with the system store, this is a no-op.
  void WriteToCookieMonster(NSArray* system_cookies);
  // Runs all the pending tasks.
  void RunAllPendingTasks();

  // Inherited CookieNotificationObserver methods.
  void OnSystemCookiesChanged() override;
  void OnSystemCookiePolicyChanged() override;

  void DeleteCookiesWithFilter(const CookieFilterFunction& filter,
                               const DeleteCallback& callback);

  scoped_refptr<net::CookieMonster> cookie_monster_;
  NSHTTPCookieStorage* system_store_;
  scoped_ptr<CookieCreationTimeManager> creation_time_manager_;
  bool metrics_enabled_;
  base::TimeDelta flush_delay_;
  base::CancelableClosure flush_closure_;

  SynchronizationState synchronization_state_;
  // Tasks received when SYNCHRONIZING are queued and run when SYNCHRONIZED.
  std::vector<base::Closure> tasks_pending_synchronization_;

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
                              bool removed);

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
  // When this CookieStoreIOS object is not synchronized (or is synchronizing),
  // the various mutator methods (SetCookieWithOptionsAsync &c) instead store
  // their state in a CookieMonster object to be written back when the system
  // store synchronizes. To deliver notifications in a timely manner, the
  // mutators have to ensure that hooks get run, but only after the changes have
  // been written back to CookieMonster. To do this, the mutators wrap the
  // user-supplied callback in a callback which schedules an asynchronous task
  // to synchronize the cache and run callbacks, then calls through to the
  // user-specified callback.
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

  // These three functions are used for wrapping user-supplied callbacks given
  // to CookieStoreIOS mutator methods. Given a callback, they return a new
  // callback that invokes UpdateCachesFromCookieMonster() to schedule an
  // asynchronous synchronization of the cookie cache and then calls the
  // original callback.

  SetCookiesCallback WrapSetCallback(const SetCookiesCallback& callback);
  DeleteCallback WrapDeleteCallback(const DeleteCallback& callback);
  base::Closure WrapClosure(const base::Closure& callback);

  // Cached values of system cookies. Only cookies which have an observer added
  // with AddCallbackForCookie are kept in this cache.
  scoped_ptr<CookieCache> cookie_cache_;

  // Callbacks for cookie changes installed by AddCallbackForCookie.
  typedef std::map<std::pair<GURL, std::string>, CookieChangedCallbackList*>
      CookieChangedHookMap;
  CookieChangedHookMap hook_map_;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreIOS);
};

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_H_
