// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/cookie_store_ios.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
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
#include "ios/net/ios_net_buildflags.h"
#import "net/base/mac/url_conversions.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

using CookieDeletionInfo = CookieDeletionInfo;

namespace {

class CookieStoreIOSCookieChangeSubscription : public CookieChangeSubscription {
 public:
  using CookieChangeCallbackList =
      base::CallbackList<void(const CanonicalCookie& cookie,
                              CookieChangeCause cause)>;
  CookieStoreIOSCookieChangeSubscription(
      std::unique_ptr<CookieChangeCallbackList::Subscription> subscription)
      : subscription_(std::move(subscription)) {}
  ~CookieStoreIOSCookieChangeSubscription() override {}

 private:
  std::unique_ptr<CookieChangeCallbackList::Subscription> subscription_;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreIOSCookieChangeSubscription);
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

#pragma mark CookieStoreIOS::CookieChangeDispatcherIOS

CookieStoreIOS::CookieChangeDispatcherIOS::CookieChangeDispatcherIOS(
    CookieStoreIOS* cookie_store)
    : cookie_store_(cookie_store) {
  DCHECK(cookie_store);
}

CookieStoreIOS::CookieChangeDispatcherIOS::~CookieChangeDispatcherIOS() =
    default;

std::unique_ptr<CookieChangeSubscription>
CookieStoreIOS::CookieChangeDispatcherIOS::AddCallbackForCookie(
    const GURL& gurl,
    const std::string& name,
    CookieChangeCallback callback) {
  return cookie_store_->AddCallbackForCookie(gurl, name, std::move(callback));
}

std::unique_ptr<CookieChangeSubscription>
CookieStoreIOS::CookieChangeDispatcherIOS::AddCallbackForUrl(
    const GURL& gurl,
    CookieChangeCallback callback) {
  // Implement when needed by iOS consumers.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<CookieChangeSubscription>
CookieStoreIOS::CookieChangeDispatcherIOS::AddCallbackForAllChanges(
    CookieChangeCallback callback) {
  // Implement when needed by iOS consumers.
  NOTIMPLEMENTED();
  return nullptr;
}

#pragma mark CookieStoreIOS

CookieStoreIOS::CookieStoreIOS(
    std::unique_ptr<SystemCookieStore> system_cookie_store)
    : CookieStoreIOS(/*persistent_store=*/nullptr,
                     std::move(system_cookie_store)) {}

CookieStoreIOS::CookieStoreIOS(NSHTTPCookieStorage* ns_cookie_store)
    : CookieStoreIOS(
          std::make_unique<NSHTTPSystemCookieStore>(ns_cookie_store)) {}

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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  // The exclude_httponly() option would only be used by a javascript
  // engine.
  DCHECK(!options.exclude_httponly());

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

void CookieStoreIOS::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    SetCookiesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

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

void CookieStoreIOS::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  // TODO(mkwst): If/when iOS supports Same-Site cookies, we'll need to pass
  // options in here as well. https://crbug.com/459154
  system_store_->GetCookiesForURLAsync(
      url,
      base::BindOnce(&CookieStoreIOS::RunGetCookieListCallbackOnSystemCookies,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void CookieStoreIOS::GetAllCookiesAsync(GetCookieListCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  // TODO(crbug.com/459154): If/when iOS supports Same-Site cookies, we'll need
  // to pass options in here as well.
  system_store_->GetAllCookiesAsync(
      base::BindOnce(&CookieStoreIOS::RunGetCookieListCallbackOnSystemCookies,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void CookieStoreIOS::DeleteCookieAsync(const GURL& url,
                                       const std::string& cookie_name,
                                       base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  // This relies on the fact cookies are given unique creation dates.
  CookieDeletionInfo delete_info(cookie.CreationDate(), cookie.CreationDate());
  DeleteCookiesMatchingInfoAsync(std::move(delete_info), std::move(callback));
}

void CookieStoreIOS::DeleteAllCreatedInTimeRangeAsync(
    const CookieDeletionInfo::TimeRange& creation_range,
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  if (metrics_enabled())
    ResetCookieCountMetrics();

  CookieDeletionInfo delete_info(creation_range.start(), creation_range.end());
  DeleteCookiesMatchingInfoAsync(std::move(delete_info), std::move(callback));
}

void CookieStoreIOS::DeleteAllMatchingInfoAsync(CookieDeletionInfo delete_info,
                                                DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  if (metrics_enabled())
    ResetCookieCountMetrics();

  DeleteCookiesMatchingInfoAsync(std::move(delete_info), std::move(callback));
}

void CookieStoreIOS::DeleteSessionCookiesAsync(DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If cookies are not allowed, a CookieStoreIOS subclass should be used
  // instead.
  DCHECK(SystemCookiesAllowed());

  if (metrics_enabled())
    ResetCookieCountMetrics();

  CookieDeletionInfo delete_info;
  delete_info.session_control =
      CookieDeletionInfo::SessionControl::SESSION_COOKIES;
  DeleteCookiesMatchingInfoAsync(std::move(delete_info), std::move(callback));
}

void CookieStoreIOS::FlushStore(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (SystemCookiesAllowed()) {
    // If cookies are disabled, the system store is empty, and the cookies are
    // stashed on disk. Do not delete the cookies on the disk in this case.
    system_store_->GetAllCookiesAsync(
        base ::BindOnce(&CookieStoreIOS::FlushStoreFromCookies,
                        weak_factory_.GetWeakPtr(), std::move(closure)));
    return;
  }

  // This code path is used by a CookieStoreIOS subclass, which shares this
  // implementation.
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
      change_dispatcher_(this),
      weak_factory_(this) {
  DCHECK(system_store_);

  NotificationTrampoline::GetInstance()->AddObserver(this);

  cookie_monster_->SetPersistSessionCookies(true);
  cookie_monster_->SetForceKeepSessionState();
}

void CookieStoreIOS::FlushStoreFromCookies(base::OnceClosure closure,
                                           NSArray<NSHTTPCookie*>* cookies) {
  WriteToCookieMonster(cookies);
  cookie_monster_->FlushStore(std::move(closure));
  flush_closure_.Cancel();
}

CookieStoreIOS::SetCookiesCallback CookieStoreIOS::WrapSetCallback(
    SetCookiesCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterSet,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

CookieStoreIOS::DeleteCallback CookieStoreIOS::WrapDeleteCallback(
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterDelete,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

base::OnceClosure CookieStoreIOS::WrapClosure(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::BindOnce(&CookieStoreIOS::UpdateCachesAfterClosure,
                        weak_factory_.GetWeakPtr(), std::move(callback));
}

#pragma mark -
#pragma mark Private methods

bool CookieStoreIOS::SystemCookiesAllowed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return system_store_->GetCookieAcceptPolicy() !=
         NSHTTPCookieAcceptPolicyNever;
}

void CookieStoreIOS::WriteToCookieMonster(NSArray* system_cookies) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

void CookieStoreIOS::DeleteCookiesMatchingInfoAsync(
    CookieDeletionInfo delete_info,
    DeleteCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  __block DeleteCallback shared_callback = std::move(callback);
  __block CookieDeletionInfo shared_delete_info = std::move(delete_info);
  base::WeakPtr<SystemCookieStore> weak_system_store =
      system_store_->GetWeakPtr();
  system_store_->GetAllCookiesAsync(
      base::BindBlockArc(^(NSArray<NSHTTPCookie*>* cookies) {
        if (!weak_system_store) {
          if (!shared_callback.is_null())
            std::move(shared_callback).Run(0);
          return;
        }
        int to_delete_count = 0;
        for (NSHTTPCookie* cookie in cookies) {
          base::Time creation_time =
              weak_system_store->GetCookieCreationTime(cookie);
          CanonicalCookie cc =
              CanonicalCookieFromSystemCookie(cookie, creation_time);
          if (shared_delete_info.Matches(cc)) {
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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

CookieChangeDispatcher& CookieStoreIOS::GetChangeDispatcher() {
  return change_dispatcher_;
}

bool CookieStoreIOS::IsEphemeral() {
  return cookie_monster_->IsEphemeral();
}

std::unique_ptr<CookieChangeSubscription> CookieStoreIOS::AddCallbackForCookie(
    const GURL& gurl,
    const std::string& name,
    CookieChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Prefill cookie cache with all pertinent cookies for |url| if needed.
  std::pair<GURL, std::string> key(gurl, name);
  if (hook_map_.count(key) == 0) {
    UpdateCacheForCookieFromSystem(gurl, name, /*run_callbacks=*/false);
    hook_map_[key] = std::make_unique<CookieChangeCallbackList>();
  }

  DCHECK(hook_map_.find(key) != hook_map_.end());
  return std::make_unique<CookieStoreIOSCookieChangeSubscription>(
      hook_map_[key]->Add(std::move(callback)));
}

void CookieStoreIOS::UpdateCacheForCookieFromSystem(
    const GURL& gurl,
    const std::string& cookie_name,
    bool run_callbacks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
                           net::CookieChangeCause::UNKNOWN_DELETION);
    RunCallbacksForCookies(gurl, cookie_name, out_added_cookies,
                           net::CookieChangeCause::INSERTED);
  }
}

void CookieStoreIOS::RunCallbacksForCookies(
    const GURL& url,
    const std::string& name,
    const std::vector<net::CanonicalCookie>& cookies,
    net::CookieChangeCause cause) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (cookies.empty())
    return;

  std::pair<GURL, std::string> key(url, name);
  CookieChangeCallbackList* callbacks = hook_map_[key].get();
  for (const auto& cookie : cookies) {
    DCHECK_EQ(name, cookie.Name());
    callbacks->Notify(cookie, cause);
  }
}

void CookieStoreIOS::GotCookieListFor(const std::pair<GURL, std::string> key,
                                      const net::CookieList& cookies) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  net::CookieList filtered;
  OnlyCookiesWithName(cookies, key.second, &filtered);
  std::vector<net::CanonicalCookie> removed_cookies;
  std::vector<net::CanonicalCookie> added_cookies;
  if (cookie_cache_->Update(key.first, key.second, filtered, &removed_cookies,
                            &added_cookies)) {
    RunCallbacksForCookies(key.first, key.second, removed_cookies,
                           net::CookieChangeCause::UNKNOWN_DELETION);
    RunCallbacksForCookies(key.first, key.second, added_cookies,
                           net::CookieChangeCause::INSERTED);
  }
}

void CookieStoreIOS::UpdateCachesFromCookieMonster() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    GetCookieListCallback callback = base::BindOnce(
        &CookieStoreIOS::GotCookieListFor, weak_factory_.GetWeakPtr(), key);
    cookie_monster_->GetAllCookiesForURLAsync(key.first, std::move(callback));
  }
}

void CookieStoreIOS::UpdateCachesAfterSet(SetCookiesCallback callback,
                                          bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (success)
    UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    std::move(callback).Run(success);
}

void CookieStoreIOS::UpdateCachesAfterDelete(DeleteCallback callback,
                                             uint32_t num_deleted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    std::move(callback).Run(num_deleted);
}

void CookieStoreIOS::UpdateCachesAfterClosure(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
