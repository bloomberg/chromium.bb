// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/cookie_store_ios.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/ios/ios_util.h"
#include "base/location.h"
#include "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ios/net/cookies/cookie_store_ios_client.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "ios/net/cookies/system_cookie_util.h"
#include "ios/net/ios_net_features.h"
#import "net/base/mac/url_conversions.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

namespace {

class CookieStoreIOSCookieChangedSubscription
    : public CookieStore::CookieChangedSubscription {
 public:
  CookieStoreIOSCookieChangedSubscription(
      std::unique_ptr<CookieStore::CookieChangedCallbackList::Subscription>
          subscription)
      : subscription_(std::move(subscription)) {}
  ~CookieStoreIOSCookieChangedSubscription() override {}

 private:
  std::unique_ptr<CookieStore::CookieChangedCallbackList::Subscription>
      subscription_;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreIOSCookieChangedSubscription);
};

#pragma mark NotificationTrampoline

// NotificationTrampoline dispatches cookie notifications to all the existing
// CookieStoreIOS.
class NotificationTrampoline {
 public:
  static NotificationTrampoline* GetInstance();

  void AddObserver(CookieNotificationObserver* obs);
  void RemoveObserver(CookieNotificationObserver* obs);

  // Notify the observers.
  void NotifyCookiesChanged();

 private:
  NotificationTrampoline();
  ~NotificationTrampoline();

  base::ObserverList<CookieNotificationObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTrampoline);

  static NotificationTrampoline* g_notification_trampoline;
};

#pragma mark NotificationTrampoline implementation

NotificationTrampoline* NotificationTrampoline::GetInstance() {
  if (!g_notification_trampoline)
    g_notification_trampoline = new NotificationTrampoline;
  return g_notification_trampoline;
}

void NotificationTrampoline::AddObserver(CookieNotificationObserver* obs) {
  observer_list_.AddObserver(obs);
}

void NotificationTrampoline::RemoveObserver(CookieNotificationObserver* obs) {
  observer_list_.RemoveObserver(obs);
}

void NotificationTrampoline::NotifyCookiesChanged() {
  for (auto& observer : observer_list_)
    observer.OnSystemCookiesChanged();
}

NotificationTrampoline::NotificationTrampoline() {
}

NotificationTrampoline::~NotificationTrampoline() {
}

// Global instance of NotificationTrampoline.
NotificationTrampoline* NotificationTrampoline::g_notification_trampoline =
    nullptr;

#pragma mark Utility functions

// Builds a NSHTTPCookie from a header cookie line ("Set-Cookie: xxx") and a
// URL.
NSHTTPCookie* GetNSHTTPCookieFromCookieLine(const std::string& cookie_line,
                                            const GURL& url,
                                            base::Time server_time) {
  NSURL* nsurl = net::NSURLWithGURL(url);
  NSString* ns_cookie_line = base::SysUTF8ToNSString(cookie_line);
  if (!ns_cookie_line) {
    DLOG(ERROR) << "Cookie line is not UTF8: " << cookie_line;
    return nil;
  }
  NSArray* cookies = [NSHTTPCookie cookiesWithResponseHeaderFields:@{
    @"Set-Cookie" : ns_cookie_line
  } forURL:nsurl];
  if ([cookies count] != 1)
    return nil;

  NSHTTPCookie* cookie = [cookies objectAtIndex:0];
  if (![cookie expiresDate] || server_time.is_null())
    return cookie;

  // Perform clock skew correction.
  base::TimeDelta clock_skew = base::Time::Now() - server_time;
  NSDate* corrected_expire_date =
      [[cookie expiresDate] dateByAddingTimeInterval:clock_skew.InSecondsF()];
  NSMutableDictionary* properties =
      [NSMutableDictionary dictionaryWithDictionary:[cookie properties]];
  [properties setObject:corrected_expire_date forKey:NSHTTPCookieExpires];
  NSHTTPCookie* corrected_cookie =
      [NSHTTPCookie cookieWithProperties:properties];
  DCHECK(corrected_cookie);
  return corrected_cookie;
}


// Builds a cookie line (such as "key1=value1; key2=value2") from an array of
// cookies.
std::string BuildCookieLineWithOptions(NSArray* cookies,
                                       const net::CookieOptions& options) {
  // The exclude_httponly() option would only be used by a javascript engine.
  DCHECK(!options.exclude_httponly());

  // This utility function returns all the cookies, including the httponly ones.
  // This is fine because we don't support the exclude_httponly option.
  NSDictionary* header = [NSHTTPCookie requestHeaderFieldsWithCookies:cookies];
  return base::SysNSStringToUTF8([header valueForKey:@"Cookie"]);
}

// Returns an empty closure if |callback| is null callback or binds the
// callback to |success|.
base::OnceClosure BindSetCookiesCallback(
    CookieStoreIOS::SetCookiesCallback* callback,
    bool success) {
  base::OnceClosure set_callback;
  if (!callback->is_null()) {
    set_callback = base::BindOnce(std::move(*callback), success);
  }
  return set_callback;
}

// Runs |callback| on CookieLine generated from |options| and |cookies|.
void RunGetCookiesCallbackOnSystemCookies(
    const net::CookieOptions& options,
    CookieStoreIOS::GetCookiesCallback callback,
    NSArray<NSHTTPCookie*>* cookies) {
  if (!callback.is_null()) {
    std::move(callback).Run(BuildCookieLineWithOptions(cookies, options));
  }
}

// Tests whether the |cookie| is a session cookie.
bool IsCookieSessionCookie(NSHTTPCookie* cookie, base::Time time) {
  return [cookie isSessionOnly];
}

// Tests whether the |creation_time| of |cookie| is in the time range defined
// by |time_begin| and |time_end|. A null |time_end| means end-of-time.
bool IsCookieCreatedBetween(base::Time time_begin,
                            base::Time time_end,
                            NSHTTPCookie* cookie,
                            base::Time creation_time) {
  return time_begin <= creation_time &&
         (time_end.is_null() || creation_time <= time_end);
}

// Tests whether the |creation_time| of |cookie| is in the time range defined
// by |time_begin| and |time_end| and the cookie host match |host|. A null
// |time_end| means end-of-time.
bool IsCookieCreatedBetweenWithPredicate(
    base::Time time_begin,
    base::Time time_end,
    const net::CookieStore::CookiePredicate& predicate,
    NSHTTPCookie* cookie,
    base::Time creation_time) {
  if (predicate.is_null())
    return false;
  CanonicalCookie canonical_cookie =
      CanonicalCookieFromSystemCookie(cookie, creation_time);
  return IsCookieCreatedBetween(time_begin, time_end, cookie, creation_time) &&
         predicate.Run(canonical_cookie);
}

// Adds cookies in |cookies| with name |name| to |filtered|.
void OnlyCookiesWithName(const net::CookieList& cookies,
                         const std::string& name,
                         net::CookieList* filtered) {
  for (const auto& cookie : cookies) {
    if (cookie.Name() == name)
      filtered->push_back(cookie);
  }
}

// Returns whether the specified cookie line has an explicit Domain attribute or
// not.
bool HasExplicitDomain(const std::string& cookie_line) {
  ParsedCookie cookie(cookie_line);
  return cookie.HasDomain();
}

}  // namespace

#pragma mark -
#pragma mark CookieStoreIOS

CookieStoreIOS::CookieStoreIOS(
    std::unique_ptr<SystemCookieStore> system_cookie_store)
    : CookieStoreIOS(/*persistent_store=*/nullptr,
                     std::move(system_cookie_store)) {}

CookieStoreIOS::CookieStoreIOS(NSHTTPCookieStorage* ns_cookie_store)
    : CookieStoreIOS(
          base::MakeUnique<NSHTTPSystemCookieStore>(ns_cookie_store)) {}

CookieStoreIOS::~CookieStoreIOS() {
  NotificationTrampoline::GetInstance()->RemoveObserver(this);
}

// static
void CookieStoreIOS::NotifySystemCookiesChanged() {
  NotificationTrampoline::GetInstance()->NotifyCookiesChanged();
}

void CookieStoreIOS::SetMetricsEnabled() {
  static CookieStoreIOS* g_cookie_store_with_metrics = nullptr;
  DCHECK(!g_cookie_store_with_metrics || g_cookie_store_with_metrics == this)
      << "Only one cookie store may use metrics.";
  g_cookie_store_with_metrics = this;
  metrics_enabled_ = true;
}

#pragma mark -
#pragma mark CookieStore methods

void CookieStoreIOS::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    SetCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // The exclude_httponly() option would only be used by a javascript
  // engine.
  DCHECK(!options.exclude_httponly());

  // If cookies are not allowed, they are stashed in the CookieMonster, and
  // should be written there instead.
  DCHECK(SystemCookiesAllowed());

  base::Time server_time =
      options.has_server_time() ? options.server_time() : base::Time();
  NSHTTPCookie* cookie =
      GetNSHTTPCookieFromCookieLine(cookie_line, url, server_time);
  DLOG_IF(WARNING, !cookie) << "Could not create cookie for line: "
                            << cookie_line;

  // On iOS, [cookie domain] is not empty when the cookie domain is not
  // specified: it is inferred from the URL instead. The only case when it
  // is empty is when the domain attribute is incorrectly formatted.
  std::string domain_string(base::SysNSStringToUTF8([cookie domain]));
  std::string dummy;
  bool has_explicit_domain = HasExplicitDomain(cookie_line);
  bool has_valid_domain =
      net::cookie_util::GetCookieDomainWithString(url, domain_string, &dummy);
  // A cookie can be set if all of:
  //   a) The cookie line is well-formed
  //   b) The Domain attribute, if present, was not malformed
  //   c) At least one of:
  //       1) The cookie had no explicit Domain, so the Domain was inferred
  //          from the URL, or
  //       2) The cookie had an explicit Domain for which the URL is allowed
  //          to set cookies.
  bool success = (cookie != nil) && !domain_string.empty() &&
                 (!has_explicit_domain || has_valid_domain);

  if (success) {
    system_store_->SetCookieAsync(cookie,
                                  BindSetCookiesCallback(&callback, true));
    return;
  }

  if (!callback.is_null())
    std::move(callback).Run(false);
}

void CookieStoreIOS::SetCookieWithDetailsAsync(const GURL& url,
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
                                               SetCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If cookies are not allowed, they are stashed in the CookieMonster, and
  // should be written there instead.
  DCHECK(SystemCookiesAllowed());

  if (creation_time.is_null())
    creation_time = base::Time::Now();

  // Validate consistency of passed arguments.
  if (ParsedCookie::ParseTokenString(name) != name ||
      ParsedCookie::ParseValueString(value) != value ||
      ParsedCookie::ParseValueString(domain) != domain ||
      ParsedCookie::ParseValueString(path) != path) {
    if (!callback.is_null())
      std::move(callback).Run(false);
    return;
  }

  // Validate passed arguments against URL.
  std::string cookie_domain;
  std::string cookie_path = CanonicalCookie::CanonPathWithString(url, path);
  if ((secure && !url.SchemeIsCryptographic()) ||
      !cookie_util::GetCookieDomainWithString(url, domain, &cookie_domain) ||
      (!path.empty() && cookie_path != path)) {
    if (!callback.is_null())
      std::move(callback).Run(false);
    return;
  }

  // Canonicalize path again to make sure it escapes characters as needed.
  url::Component path_component(0, cookie_path.length());
  url::RawCanonOutputT<char> canon_path;
  url::Component canon_path_component;
  url::CanonicalizePath(cookie_path.data(), path_component, &canon_path,
                        &canon_path_component);
  cookie_path = std::string(canon_path.data() + canon_path_component.begin,
                            canon_path_component.len);

  std::unique_ptr<net::CanonicalCookie> canonical_cookie =
      base::MakeUnique<net::CanonicalCookie>(
          name, value, cookie_domain, cookie_path, creation_time,
          expiration_time, creation_time, secure, http_only, same_site,
          priority);

  if (canonical_cookie) {
    NSHTTPCookie* cookie = SystemCookieFromCanonicalCookie(*canonical_cookie);

    if (cookie != nil) {
      system_store_->SetCookieAsync(cookie, &canonical_cookie->CreationDate(),
                                    BindSetCookiesCallback(&callback, true));
      return;
    }
  }

  if (!callback.is_null())
    std::move(callback).Run(false);
}

void CookieStoreIOS::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    SetCookiesCallback callback) {
  DCHECK(cookie->IsCanonical());
  // The exclude_httponly() option would only be used by a javascript
  // engine.
  DCHECK(modify_http_only);

  if (cookie->IsSecure() && !secure_source) {
    if (!callback.is_null())
      std::move(callback).Run(false);
    return;
  }

  NSHTTPCookie* ns_cookie = SystemCookieFromCanonicalCookie(*cookie.get());

  if (ns_cookie != nil) {
    system_store_->SetCookieAsync(ns_cookie, &cookie->CreationDate(),
                                  BindSetCookiesCallback(&callback, true));
    return;
  }

  if (!callback.is_null())
    std::move(callback).Run(false);
}

void CookieStoreIOS::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If cookies are not allowed, they are stashed in the CookieMonster, and
  // should be read from there instead.
  DCHECK(SystemCookiesAllowed());
  // The exclude_httponly() option would only be used by a javascript
  // engine.
  DCHECK(!options.exclude_httponly());

  // TODO(crbug.com/459154): If/when iOS supports Same-Site cookies, we'll need
  // to pass options in here as well.
  system_store_->GetCookiesForURLAsync(
      url, base::BindOnce(&RunGetCookiesCallbackOnSystemCookies, options,
                          base::Passed(&callback)));
}

void CookieStoreIOS::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!SystemCookiesAllowed()) {
    // If cookies are not allowed, the cookies are stashed in the
    // CookieMonster, so get them from there.
    cookie_monster_->GetCookieListWithOptionsAsync(url, options,
                                                   std::move(callback));
    return;
  }

  // TODO(mkwst): If/when iOS supports Same-Site cookies, we'll need to pass
  // options in here as well. https://crbug.com/459154
  system_store_->GetCookiesForURLAsync(
      url,
      base::BindOnce(&CookieStoreIOS::RunGetCookieListCallbackOnSystemCookies,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void CookieStoreIOS::GetAllCookiesAsync(GetCookieListCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!SystemCookiesAllowed()) {
    // If cookies are not allowed, the cookies are stashed in the
    // CookieMonster, so get them from there.
    cookie_monster_->GetAllCookiesAsync(std::move(callback));
    return;
  }
  // TODO(crbug.com/459154): If/when iOS supports Same-Site cookies, we'll need
  // to pass options in here as well.
  system_store_->GetAllCookiesAsync(
      base::BindOnce(&CookieStoreIOS::RunGetCookieListCallbackOnSystemCookies,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void CookieStoreIOS::DeleteCookieAsync(const GURL& url,
                                       const std::string& cookie_name,
                                       base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  __block base::OnceClosure shared_callback = std::move(callback);
  base::WeakPtr<SystemCookieStore> weak_system_store =
      system_store_->GetWeakPtr();
  system_store_->GetCookiesForURLAsync(
      url, base::BindBlockArc(^(NSArray<NSHTTPCookie*>* cookies) {
        for (NSHTTPCookie* cookie in cookies) {
          if ([cookie.name
                  isEqualToString:base::SysUTF8ToNSString(cookie_name)] &&
              weak_system_store) {
            weak_system_store->DeleteCookieAsync(
                cookie, SystemCookieStore::SystemCookieCallback());
          }
        }
        if (!shared_callback.is_null())
          std::move(shared_callback).Run();
      }));
}

void CookieStoreIOS::DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                                DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This relies on the fact cookies are given unique creation dates.
  CookieFilterFunction filter = base::Bind(
      IsCookieCreatedBetween, cookie.CreationDate(), cookie.CreationDate());
  DeleteCookiesWithFilterAsync(std::move(filter), std::move(callback));
}

void CookieStoreIOS::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  CookieFilterFunction filter = base::Bind(
      &IsCookieCreatedBetween, delete_begin, delete_end);
  DeleteCookiesWithFilterAsync(std::move(filter), std::move(callback));
}

void CookieStoreIOS::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  CookieFilterFunction filter = base::Bind(
      IsCookieCreatedBetweenWithPredicate, delete_begin, delete_end, predicate);
  DeleteCookiesWithFilterAsync(std::move(filter), std::move(callback));
}

void CookieStoreIOS::DeleteSessionCookiesAsync(DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  CookieFilterFunction filter = base::Bind(&IsCookieSessionCookie);
  DeleteCookiesWithFilterAsync(std::move(filter), std::move(callback));
}

void CookieStoreIOS::FlushStore(base::OnceClosure closure) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (SystemCookiesAllowed()) {
    // If cookies are disabled, the system store is empty, and the cookies are
    // stashed on disk. Do not delete the cookies on the disk in this case.
    system_store_->GetAllCookiesAsync(
        base ::BindOnce(&CookieStoreIOS::FlushStoreFromCookies,
                        weak_factory_.GetWeakPtr(), std::move(closure)));
    return;
  }

  cookie_monster_->FlushStore(std::move(closure));
  flush_closure_.Cancel();
}

void CookieStoreIOS::FlushStoreFromCookies(base::OnceClosure closure,
                                           NSArray<NSHTTPCookie*>* cookies) {
  WriteToCookieMonster(cookies);
  cookie_monster_->FlushStore(std::move(closure));
  flush_closure_.Cancel();
}

#pragma mark -
#pragma mark Protected methods

CookieStoreIOS::CookieStoreIOS(
    net::CookieMonster::PersistentCookieStore* persistent_store,
    std::unique_ptr<SystemCookieStore> system_store)
    : cookie_monster_(new net::CookieMonster(persistent_store)),
      system_store_(std::move(system_store)),
      metrics_enabled_(false),
      cookie_cache_(new CookieCache()),
      weak_factory_(this) {
  DCHECK(system_store_);

  NotificationTrampoline::GetInstance()->AddObserver(this);

  cookie_monster_->SetPersistSessionCookies(true);
  cookie_monster_->SetForceKeepSessionState();
}

CookieStoreIOS::SetCookiesCallback CookieStoreIOS::WrapSetCallback(
    SetCookiesCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterSet,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

CookieStoreIOS::DeleteCallback CookieStoreIOS::WrapDeleteCallback(
    DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterDelete,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

base::OnceClosure CookieStoreIOS::WrapClosure(base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterClosure,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

#pragma mark -
#pragma mark Private methods

bool CookieStoreIOS::SystemCookiesAllowed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return system_store_->GetCookieAcceptPolicy() !=
         NSHTTPCookieAcceptPolicyNever;
}

void CookieStoreIOS::WriteToCookieMonster(NSArray* system_cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Copy the cookies from the global cookie store to |cookie_monster_|.
  // Unlike the system store, CookieMonster requires unique creation times.
  net::CookieList cookie_list;
  NSUInteger cookie_count = [system_cookies count];
  cookie_list.reserve(cookie_count);
  for (NSHTTPCookie* cookie in system_cookies) {
    cookie_list.push_back(CanonicalCookieFromSystemCookie(
        cookie, system_store_->GetCookieCreationTime(cookie)));
  }
  cookie_monster_->SetAllCookiesAsync(cookie_list, SetCookiesCallback());

  // Update metrics.
  if (metrics_enabled_)
    UMA_HISTOGRAM_COUNTS_10000("CookieIOS.CookieWrittenCount", cookie_count);
}

void CookieStoreIOS::DeleteCookiesWithFilterAsync(CookieFilterFunction filter,
                                                  DeleteCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!filter.is_null());
  __block DeleteCallback shared_callback = std::move(callback);
  __block CookieFilterFunction shared_filter = std::move(filter);
  base::WeakPtr<SystemCookieStore> weak_system_store =
      system_store_->GetWeakPtr();
  system_store_->GetAllCookiesAsync(
      base::BindBlockArc(^(NSArray<NSHTTPCookie*>* cookies) {
        int to_delete_count = 0;
        for (NSHTTPCookie* cookie in cookies) {
          if (weak_system_store &&
              shared_filter.Run(
                  cookie, weak_system_store->GetCookieCreationTime(cookie))) {
            weak_system_store->DeleteCookieAsync(
                cookie, SystemCookieStore::SystemCookieCallback());
            to_delete_count++;
          }
        }

        if (!shared_callback.is_null())
          std::move(shared_callback).Run(to_delete_count);
      }));
}

void CookieStoreIOS::OnSystemCookiesChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    UpdateCacheForCookieFromSystem(key.first, key.second,
                                   /*run_callbacks=*/true);
  }

  // Do not schedule a flush if one is already scheduled.
  if (!flush_closure_.IsCancelled())
    return;

  flush_closure_.Reset(base::Bind(&CookieStoreIOS::FlushStore,
                                  weak_factory_.GetWeakPtr(), base::Closure()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, flush_closure_.callback(), base::TimeDelta::FromSeconds(10));
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
CookieStoreIOS::AddCallbackForCookie(const GURL& gurl,
                                     const std::string& name,
                                     const CookieChangedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prefill cookie cache with all pertinent cookies for |url| if needed.
  std::pair<GURL, std::string> key(gurl, name);
  if (hook_map_.count(key) == 0) {
    UpdateCacheForCookieFromSystem(gurl, name, /*run_callbacks=*/false);
    hook_map_[key] = base::MakeUnique<CookieChangedCallbackList>();
  }

  DCHECK(hook_map_.find(key) != hook_map_.end());
  return std::make_unique<CookieStoreIOSCookieChangedSubscription>(
      hook_map_[key]->Add(callback));
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
CookieStoreIOS::AddCallbackForAllChanges(
    const CookieChangedCallback& callback) {
  // Implement when needed by iOS consumers.
  NOTIMPLEMENTED();
  return nullptr;
}

bool CookieStoreIOS::IsEphemeral() {
  return cookie_monster_->IsEphemeral();
}

void CookieStoreIOS::UpdateCacheForCookieFromSystem(
    const GURL& gurl,
    const std::string& cookie_name,
    bool run_callbacks) {
  DCHECK(thread_checker_.CalledOnValidThread());
  system_store_->GetCookiesForURLAsync(
      gurl, base::BindOnce(&CookieStoreIOS::UpdateCacheForCookies,
                           weak_factory_.GetWeakPtr(), gurl, cookie_name,
                           run_callbacks));
}

void CookieStoreIOS::UpdateCacheForCookies(const GURL& gurl,
                                           const std::string& cookie_name,
                                           bool run_callbacks,
                                           NSArray<NSHTTPCookie*>* nscookies) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<net::CanonicalCookie> out_removed_cookies;
  std::vector<net::CanonicalCookie> out_added_cookies;
  for (NSHTTPCookie* nscookie in nscookies) {
    if (base::SysNSStringToUTF8(nscookie.name) == cookie_name) {
      net::CanonicalCookie canonical_cookie = CanonicalCookieFromSystemCookie(
          nscookie, system_store_->GetCookieCreationTime(nscookie));
      cookies.push_back(canonical_cookie);
    }
  }

  bool changes = cookie_cache_->Update(
      gurl, cookie_name, cookies, &out_removed_cookies, &out_added_cookies);
  if (run_callbacks && changes) {
    RunCallbacksForCookies(gurl, cookie_name, out_removed_cookies,
                           net::CookieStore::ChangeCause::UNKNOWN_DELETION);
    RunCallbacksForCookies(gurl, cookie_name, out_added_cookies,
                           net::CookieStore::ChangeCause::INSERTED);
  }
}

void CookieStoreIOS::RunCallbacksForCookies(
    const GURL& url,
    const std::string& name,
    const std::vector<net::CanonicalCookie>& cookies,
    net::CookieStore::ChangeCause cause) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cookies.empty())
    return;

  std::pair<GURL, std::string> key(url, name);
  CookieChangedCallbackList* callbacks = hook_map_[key].get();
  for (const auto& cookie : cookies) {
    DCHECK_EQ(name, cookie.Name());
    callbacks->Notify(cookie, cause);
  }
}

void CookieStoreIOS::GotCookieListFor(const std::pair<GURL, std::string> key,
                                      const net::CookieList& cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());

  net::CookieList filtered;
  OnlyCookiesWithName(cookies, key.second, &filtered);
  std::vector<net::CanonicalCookie> removed_cookies;
  std::vector<net::CanonicalCookie> added_cookies;
  if (cookie_cache_->Update(key.first, key.second, filtered, &removed_cookies,
                            &added_cookies)) {
    RunCallbacksForCookies(key.first, key.second, removed_cookies,
                           net::CookieStore::ChangeCause::UNKNOWN_DELETION);
    RunCallbacksForCookies(key.first, key.second, added_cookies,
                           net::CookieStore::ChangeCause::INSERTED);
  }
}

void CookieStoreIOS::UpdateCachesFromCookieMonster() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    GetCookieListCallback callback = base::BindOnce(
        &CookieStoreIOS::GotCookieListFor, weak_factory_.GetWeakPtr(), key);
    cookie_monster_->GetAllCookiesForURLAsync(key.first, std::move(callback));
  }
}

void CookieStoreIOS::UpdateCachesAfterSet(SetCookiesCallback callback,
                                          bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (success)
    UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    std::move(callback).Run(success);
}

void CookieStoreIOS::UpdateCachesAfterDelete(DeleteCallback callback,
                                             uint32_t num_deleted) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    std::move(callback).Run(num_deleted);
}

void CookieStoreIOS::UpdateCachesAfterClosure(base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    std::move(callback).Run();
}

net::CookieList
CookieStoreIOS::CanonicalCookieListFromSystemCookies(NSArray* cookies) {
  net::CookieList cookie_list;
  cookie_list.reserve([cookies count]);
  for (NSHTTPCookie* cookie in cookies) {
    base::Time created = system_store_->GetCookieCreationTime(cookie);
    cookie_list.push_back(CanonicalCookieFromSystemCookie(cookie, created));
  }
  return cookie_list;
}

void CookieStoreIOS::RunGetCookieListCallbackOnSystemCookies(
    CookieStoreIOS::GetCookieListCallback callback,
    NSArray<NSHTTPCookie*>* cookies) {
  if (!callback.is_null()) {
    std::move(callback).Run(CanonicalCookieListFromSystemCookies(cookies));
  }
}

}  // namespace net
