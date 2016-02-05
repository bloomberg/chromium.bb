// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_store_ios.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/ios/ios_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "ios/net/cookies/cookie_creation_time_manager.h"
#include "ios/net/cookies/cookie_store_ios_client.h"
#include "ios/net/cookies/system_cookie_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "url/gurl.h"

namespace net {

namespace {

#if !defined(NDEBUG)
// The current cookie store. This weak pointer must not be used to do actual
// work. Its only purpose is to check that there is only one synchronized
// cookie store.
CookieStoreIOS* g_current_synchronized_store = nullptr;
#endif

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
  void NotifyCookiePolicyChanged();

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
  FOR_EACH_OBSERVER(CookieNotificationObserver, observer_list_,
                    OnSystemCookiesChanged());
}

void NotificationTrampoline::NotifyCookiePolicyChanged() {
  FOR_EACH_OBSERVER(CookieNotificationObserver, observer_list_,
                    OnSystemCookiePolicyChanged());
}

NotificationTrampoline::NotificationTrampoline() {
}

NotificationTrampoline::~NotificationTrampoline() {
}

// Global instance of NotificationTrampoline.
NotificationTrampoline* NotificationTrampoline::g_notification_trampoline =
    nullptr;

#pragma mark Utility functions

// Returns the path to Cookie.binarycookies file on the file system where
// WKWebView flushes its cookies.
base::FilePath GetBinaryCookiesFilePath() {
  base::FilePath path = base::mac::GetUserLibraryPath();
  // The relative path of the file (from the user library folder) where
  // WKWebView stores its cookies.
  const std::string kCookiesFilePath = "Cookies/Cookies.binarycookies";
  return path.Append(kCookiesFilePath);
}

// Clears all cookies from the .binarycookies file.
// Must be called from a thread where IO operations are allowed.
// Preconditions: There must be no active WKWebViews present in the app.
void ClearAllCookiesFromBinaryCookiesFile() {
  // The .binarycookies file is present only on iOS8+.
  if (!base::ios::IsRunningOnIOS8OrLater()) {
    return;
  }
  base::FilePath path = GetBinaryCookiesFilePath();
  if (base::PathExists(path)) {
    bool success = base::DeleteFile(path, false);
    if (!success) {
      DLOG(WARNING) << "Failed to remove binarycookies file.";
    }
  }
}

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

// Compares cookies based on the path lengths and the creation times, as per
// RFC6265.
NSInteger CompareCookies(id a, id b, void* context) {
  NSHTTPCookie* cookie_a = (NSHTTPCookie*)a;
  NSHTTPCookie* cookie_b = (NSHTTPCookie*)b;
  // Compare path lengths first.
  NSUInteger path_length_a = [[cookie_a path] length];
  NSUInteger path_length_b = [[cookie_b path] length];
  if (path_length_a < path_length_b)
    return NSOrderedDescending;
  if (path_length_b < path_length_a)
    return NSOrderedAscending;

  // Compare creation times.
  CookieCreationTimeManager* manager = (CookieCreationTimeManager*)context;
  DCHECK(manager);
  base::Time created_a = manager->GetCreationTime(cookie_a);
  base::Time created_b = manager->GetCreationTime(cookie_b);
#if !defined(CRNET)
  // CookieCreationTimeManager is returning creation times that are null.
  // Since in CrNet, the cookie store is recreated on startup, let's suppress
  // this warning for now.
  // TODO(huey): Instead of suppressing the warning, assign a creation time
  // to cookies if one doesn't already exist.
  DLOG_IF(ERROR, created_a.is_null() || created_b.is_null())
      << "Cookie without creation date";
#endif
  if (created_a < created_b)
    return NSOrderedAscending;
  return (created_a > created_b) ? NSOrderedDescending : NSOrderedSame;
}

// Gets the cookies for |url| from the system cookie store.
NSArray* GetCookiesForURL(NSHTTPCookieStorage* system_store,
                          const GURL& url, CookieCreationTimeManager* manager) {
  NSArray* cookies = [system_store cookiesForURL:net::NSURLWithGURL(url)];

  // Sort cookies by decreasing path length, then creation time, as per RFC6265.
  return [cookies sortedArrayUsingFunction:CompareCookies context:manager];
}

// Gets all cookies from the system cookie store.
NSArray* GetAllCookies(NSHTTPCookieStorage* system_store,
                       CookieCreationTimeManager* manager) {
  NSArray* cookies = [system_store cookies];

  // Sort cookies by decreasing path length, then creation time, as per RFC6265.
  return [cookies sortedArrayUsingFunction:CompareCookies context:manager];
}

// Builds a cookie line (such as "key1=value1; key2=value2") from an array of
// cookies.
std::string BuildCookieLine(NSArray* cookies,
                            const net::CookieOptions& options) {
  // The exclude_httponly() option would only be used by a javascript engine.
  DCHECK(!options.exclude_httponly());

  // This utility function returns all the cookies, including the httponly ones.
  // This is fine because we don't support the exclude_httponly option.
  NSDictionary* header = [NSHTTPCookie requestHeaderFieldsWithCookies:cookies];
  return base::SysNSStringToUTF8([header valueForKey:@"Cookie"]);
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
bool IsCookieCreatedBetweenForHost(base::Time time_begin,
                                   base::Time time_end,
                                   NSString* host,
                                   NSHTTPCookie* cookie,
                                   base::Time creation_time) {
  NSString* domain = [cookie domain];
  return [domain characterAtIndex:0] != '.' &&
         [domain caseInsensitiveCompare:host] == NSOrderedSame &&
         IsCookieCreatedBetween(time_begin, time_end, cookie, creation_time);
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
    net::CookieMonster::PersistentCookieStore* persistent_store)
    : CookieStoreIOS(persistent_store,
                     [NSHTTPCookieStorage sharedHTTPCookieStorage]) {
}

CookieStoreIOS::CookieStoreIOS(
    net::CookieMonster::PersistentCookieStore* persistent_store,
    NSHTTPCookieStorage* system_store)
    : system_store_(system_store),
      creation_time_manager_(new CookieCreationTimeManager),
      metrics_enabled_(false),
      flush_delay_(base::TimeDelta::FromSeconds(10)),
      synchronization_state_(NOT_SYNCHRONIZED),
      cookie_cache_(new CookieCache()) {
  DCHECK(system_store);

  NotificationTrampoline::GetInstance()->AddObserver(this);
  cookie_monster_ = new net::CookieMonster(persistent_store, nullptr);
  cookie_monster_->SetPersistSessionCookies(true);
  cookie_monster_->SetForceKeepSessionState();
}

// static
void CookieStoreIOS::SetCookiePolicy(CookiePolicy setting) {
  NSHTTPCookieAcceptPolicy policy = (setting == ALLOW)
                                        ? NSHTTPCookieAcceptPolicyAlways
                                        : NSHTTPCookieAcceptPolicyNever;
  NSHTTPCookieStorage* store = [NSHTTPCookieStorage sharedHTTPCookieStorage];
  NSHTTPCookieAcceptPolicy current_policy = [store cookieAcceptPolicy];
  if (current_policy == policy)
    return;
  [store setCookieAcceptPolicy:policy];
  NotificationTrampoline::GetInstance()->NotifyCookiePolicyChanged();
}

// static
CookieStoreIOS* CookieStoreIOS::CreateCookieStore(
    NSHTTPCookieStorage* cookie_storage) {
  DCHECK(cookie_storage);
  // TODO(huey): Update this when CrNet supports multiple cookie jars.
  [cookie_storage setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];

  // Create a cookie store with no persistent store backing. Then, populate
  // it from the system's cookie jar.
  CookieStoreIOS* cookie_store = new CookieStoreIOS(nullptr, cookie_storage);
  cookie_store->synchronization_state_ = SYNCHRONIZED;
  cookie_store->FlushStore(base::Closure());
  return cookie_store;
}

// static
void CookieStoreIOS::SwitchSynchronizedStore(CookieStoreIOS* old_store,
                                             CookieStoreIOS* new_store) {
  DCHECK(new_store);
  DCHECK_NE(new_store, old_store);
  if (old_store)
    old_store->SetSynchronizedWithSystemStore(false);
  new_store->SetSynchronizedWithSystemStore(true);
}

// static
void CookieStoreIOS::NotifySystemCookiesChanged() {
  NotificationTrampoline::GetInstance()->NotifyCookiesChanged();
}

void CookieStoreIOS::UnSynchronize() {
  SetSynchronizedWithSystemStore(false);
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
    const SetCookiesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->SetCookieWithOptionsAsync(url, cookie_line, options,
                                                 WrapSetCallback(callback));
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(
          base::Bind(&CookieStoreIOS::SetCookieWithOptionsAsync, this, url,
                     cookie_line, options, WrapSetCallback(callback)));
      break;
    case SYNCHRONIZED:
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
      DLOG_IF(WARNING, !cookie)
          << "Could not create cookie for line: " << cookie_line;

      // On iOS, [cookie domain] is not empty when the cookie domain is not
      // specified: it is inferred from the URL instead. The only case when it
      // is empty is when the domain attribute is incorrectly formatted.
      std::string domain_string(base::SysNSStringToUTF8([cookie domain]));
      std::string dummy;
      bool has_explicit_domain = HasExplicitDomain(cookie_line);
      bool has_valid_domain =
          net::cookie_util::GetCookieDomainWithString(
              url, domain_string, &dummy);
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
        [system_store_ setCookie:cookie];
        creation_time_manager_->SetCreationTime(
            cookie,
            creation_time_manager_->MakeUniqueCreationTime(base::Time::Now()));
      }

      if (!callback.is_null())
        callback.Run(success);
      break;
  }
}

void CookieStoreIOS::SetCookieWithDetailsAsync(
    const GURL& url,
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
    const SetCookiesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->SetCookieWithDetailsAsync(
          url, name, value, domain, path, creation_time, expiration_time,
          last_access_time, secure, http_only, same_site, enforce_strict_secure,
          priority, WrapSetCallback(callback));
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(base::Bind(
          &CookieStoreIOS::SetCookieWithDetailsAsync, this, url, name, value,
          domain, path, creation_time, expiration_time, last_access_time,
          secure, http_only, same_site, enforce_strict_secure, priority,
          WrapSetCallback(callback)));
      break;
    case SYNCHRONIZED:
      // If cookies are not allowed, they are stashed in the CookieMonster, and
      // should be written there instead.
      DCHECK(SystemCookiesAllowed());

      bool success = false;

      if (creation_time.is_null())
        creation_time = base::Time::Now();

      // First create a CanonicalCookie, to normalize the arguments,
      // particularly domain and path, and perform validation.
      scoped_ptr<net::CanonicalCookie> canonical_cookie =
          net::CanonicalCookie::Create(
              url, name, value, domain, path, creation_time, expiration_time,
              secure, http_only, same_site, enforce_strict_secure,
              priority);

      if (canonical_cookie) {
        NSHTTPCookie* cookie =
            SystemCookieFromCanonicalCookie(*canonical_cookie);

        if (cookie != nil) {
          [system_store_ setCookie:cookie];
          creation_time_manager_->SetCreationTime(
              cookie, creation_time_manager_->MakeUniqueCreationTime(
                          canonical_cookie->CreationDate()));
          success = true;
        }
      }

      if (!callback.is_null())
        callback.Run(success);
      break;
  }
}

void CookieStoreIOS::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookiesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->GetCookiesWithOptionsAsync(url, options, callback);
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(
          base::Bind(&CookieStoreIOS::GetCookiesWithOptionsAsync, this, url,
                     options, callback));
      break;
    case SYNCHRONIZED:
      // If cookies are not allowed, they are stashed in the CookieMonster, and
      // should be read from there instead.
      DCHECK(SystemCookiesAllowed());
      // The exclude_httponly() option would only be used by a javascript
      // engine.
      DCHECK(!options.exclude_httponly());

      NSArray* cookies = GetCookiesForURL(system_store_,
                                          url, creation_time_manager_.get());
      if (!callback.is_null())
        callback.Run(BuildCookieLine(cookies, options));
      break;
  }
}

void CookieStoreIOS::GetAllCookiesForURLAsync(
    const GURL& url,
    const GetCookieListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->GetAllCookiesForURLAsync(url, callback);
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(base::Bind(
          &CookieStoreIOS::GetAllCookiesForURLAsync, this, url, callback));
      break;
    case SYNCHRONIZED:
      if (!SystemCookiesAllowed()) {
        // If cookies are not allowed, the cookies are stashed in the
        // CookieMonster, so get them from there.
        cookie_monster_->GetAllCookiesForURLAsync(url, callback);
        return;
      }

      NSArray* cookies = GetCookiesForURL(system_store_,
                                          url, creation_time_manager_.get());
      net::CookieList cookie_list = CanonicalCookieListFromSystemCookies(
          cookies);
      if (!callback.is_null())
        callback.Run(cookie_list);
      break;
  }
}

void CookieStoreIOS::GetAllCookiesAsync(const GetCookieListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->GetAllCookiesAsync(callback);
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(base::Bind(
          &CookieStoreIOS::GetAllCookiesAsync, this, callback));
      break;
    case SYNCHRONIZED:
      if (!SystemCookiesAllowed()) {
        // If cookies are not allowed, the cookies are stashed in the
        // CookieMonster, so get them from there.
        cookie_monster_->GetAllCookiesAsync(callback);
        return;
      }

      NSArray* cookies = GetAllCookies(system_store_,
                                       creation_time_manager_.get());
      net::CookieList cookie_list = CanonicalCookieListFromSystemCookies(
          cookies);
      if (!callback.is_null()) {
        callback.Run(cookie_list);
      }
      break;
  }
}

void CookieStoreIOS::DeleteCookieAsync(const GURL& url,
                                       const std::string& cookie_name,
                                       const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->DeleteCookieAsync(url, cookie_name,
                                         WrapClosure(callback));
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(
          base::Bind(&CookieStoreIOS::DeleteCookieAsync, this, url, cookie_name,
                     WrapClosure(callback)));
      break;
    case SYNCHRONIZED:
      NSArray* cookies = GetCookiesForURL(system_store_,
                                          url, creation_time_manager_.get());
      for (NSHTTPCookie* cookie in cookies) {
        if ([[cookie name]
                isEqualToString:base::SysUTF8ToNSString(cookie_name)]) {
          [system_store_ deleteCookie:cookie];
          creation_time_manager_->DeleteCreationTime(cookie);
        }
      }
      if (!callback.is_null())
        callback.Run();
      break;
  }
}

void CookieStoreIOS::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->DeleteCanonicalCookieAsync(cookie,
                                                  WrapDeleteCallback(callback));
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(
          base::Bind(&CookieStoreIOS::DeleteCanonicalCookieAsync, this, cookie,
                     WrapDeleteCallback(callback)));
      break;
    case SYNCHRONIZED:
      // This relies on the fact cookies are given unique creation dates.
      CookieFilterFunction filter = base::Bind(
          IsCookieCreatedBetween, cookie.CreationDate(), cookie.CreationDate());
      DeleteCookiesWithFilter(filter, callback);
  }
}

// CookieStoreIOS is an implementation of CookieStore which is not a
// CookieMonster. As CookieStore is the main cookie API, a caller of
// GetCookieMonster must handle the case where this returns null.
net::CookieMonster* CookieStoreIOS::GetCookieMonster() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

void CookieStoreIOS::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->DeleteAllCreatedBetweenAsync(
          delete_begin, delete_end, WrapDeleteCallback(callback));
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(
          base::Bind(&CookieStoreIOS::DeleteAllCreatedBetweenAsync, this,
                     delete_begin, delete_end, WrapDeleteCallback(callback)));
      break;
    case SYNCHRONIZED:
      CookieFilterFunction filter =
          base::Bind(&IsCookieCreatedBetween, delete_begin, delete_end);
      DeleteCookiesWithFilter(filter, callback);
      break;
  }
}

void CookieStoreIOS::DeleteAllCreatedBetweenForHostAsync(
    const base::Time delete_begin,
    const base::Time delete_end,
    const GURL& url,
    const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->DeleteAllCreatedBetweenForHostAsync(
          delete_begin, delete_end, url, WrapDeleteCallback(callback));
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(base::Bind(
          &CookieStoreIOS::DeleteAllCreatedBetweenForHostAsync, this,
          delete_begin, delete_end, url, WrapDeleteCallback(callback)));
      break;
    case SYNCHRONIZED:
      NSString* host = base::SysUTF8ToNSString(url.host());
      CookieFilterFunction filter = base::Bind(IsCookieCreatedBetweenForHost,
                                               delete_begin, delete_end, host);
      DeleteCookiesWithFilter(filter, callback);
      break;
  }
}

void CookieStoreIOS::DeleteSessionCookiesAsync(const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  switch (synchronization_state_) {
    case NOT_SYNCHRONIZED:
      cookie_monster_->DeleteSessionCookiesAsync(WrapDeleteCallback(callback));
      break;
    case SYNCHRONIZING:
      tasks_pending_synchronization_.push_back(
          base::Bind(&CookieStoreIOS::DeleteSessionCookiesAsync, this,
                     WrapDeleteCallback(callback)));
      break;
    case SYNCHRONIZED:
      CookieFilterFunction filter = base::Bind(&IsCookieSessionCookie);
      DeleteCookiesWithFilter(filter, callback);
      break;
  }
}

void CookieStoreIOS::FlushStore(const base::Closure& closure) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (SystemCookiesAllowed()) {
    // If cookies are disabled, the system store is empty, and the cookies are
    // stashed on disk. Do not delete the cookies on the disk in this case.
    WriteToCookieMonster([system_store_ cookies]);
  }
  cookie_monster_->FlushStore(closure);
  flush_closure_.Cancel();
}

#pragma mark -
#pragma mark Protected methods

CookieStoreIOS::~CookieStoreIOS() {
  NotificationTrampoline::GetInstance()->RemoveObserver(this);
  STLDeleteContainerPairSecondPointers(hook_map_.begin(), hook_map_.end());
}

#pragma mark -
#pragma mark Private methods

void CookieStoreIOS::ClearSystemStore() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::scoped_nsobject<NSArray> copy(
      [[NSArray alloc] initWithArray:[system_store_ cookies]]);
  for (NSHTTPCookie* cookie in copy.get())
    [system_store_ deleteCookie:cookie];
  DCHECK_EQ(0u, [[system_store_ cookies] count]);
  creation_time_manager_->Clear();
}

void CookieStoreIOS::OnSystemCookiePolicyChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the CookieStoreIOS is not synchronized or is not backed by
  // |NSHTTPCookieStorage sharedHTTPCookieStorage| this callback is irrelevant.
  if (synchronization_state_ == NOT_SYNCHRONIZED ||
      system_store_ != [NSHTTPCookieStorage sharedHTTPCookieStorage]) {
    return;
  }

  NSHTTPCookieAcceptPolicy policy =
      [system_store_ cookieAcceptPolicy];
  if (policy == NSHTTPCookieAcceptPolicyAlways) {
    // If cookies are disabled, the system cookie store should be empty.
    DCHECK(![[system_store_ cookies] count]);
    DCHECK(synchronization_state_ != SYNCHRONIZING);
    synchronization_state_ = SYNCHRONIZING;
    cookie_monster_->GetAllCookiesAsync(
        base::Bind(&CookieStoreIOS::AddCookiesToSystemStore, this));
  } else {
    DCHECK_EQ(NSHTTPCookieAcceptPolicyNever, policy);
    // FlushStore() does not write the cookies to disk when they are disabled.
    // Explicitly copy them.
    WriteToCookieMonster([system_store_ cookies]);
    FlushStore(base::Closure());
    ClearSystemStore();
    if (synchronization_state_ == SYNCHRONIZING) {
      // If synchronization was in progress, abort it and leave the cookie store
      // empty.
      // Temporarily toggle the synchronization state so that pending tasks are
      // redirected to cookie_monster_ and can complete normally.
      synchronization_state_ = NOT_SYNCHRONIZED;
      RunAllPendingTasks();
      synchronization_state_ = SYNCHRONIZED;
    }
  }
}

void CookieStoreIOS::SetSynchronizedWithSystemStore(bool synchronized) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (synchronized == (synchronization_state_ != NOT_SYNCHRONIZED))
    return;  // The cookie store is already in the desired state.

#if !defined(NDEBUG)
  if (!synchronized) {
    DCHECK_EQ(this, g_current_synchronized_store)
        << "This cookie store was not synchronized";
    g_current_synchronized_store = nullptr;
  } else {
    DCHECK_EQ((CookieStoreIOS*)nullptr, g_current_synchronized_store)
        << "Un-synchronize the current cookie store first.";
    g_current_synchronized_store = this;
  }
#endif

  NSHTTPCookieAcceptPolicy policy =
      [system_store_ cookieAcceptPolicy];
  DCHECK(policy == NSHTTPCookieAcceptPolicyAlways ||
         policy == NSHTTPCookieAcceptPolicyNever);

  // If cookies are disabled, the system cookie store should be empty.
  DCHECK(policy == NSHTTPCookieAcceptPolicyAlways ||
         ![[system_store_ cookies] count]);

  // If cookies are disabled, nothing is done now, the work will be done when
  // cookies are re-enabled.
  if (policy == NSHTTPCookieAcceptPolicyAlways) {
    if (synchronized) {
      synchronization_state_ = SYNCHRONIZING;
      ClearSystemStore();
      cookie_monster_->GetAllCookiesAsync(
          base::Bind(&CookieStoreIOS::AddCookiesToSystemStore, this));
      return;
    } else {
      // Copy the cookies from the global store to |cookie_monster_|.
      FlushStore(base::Closure());
    }
  }
  synchronization_state_ = synchronized ? SYNCHRONIZED : NOT_SYNCHRONIZED;

  if (synchronization_state_ == NOT_SYNCHRONIZED) {
    // If there are pending tasks, then it means that the synchronization is
    // being canceled. All pending tasks can be sent to cookie_monster_.
    RunAllPendingTasks();
  }
}

bool CookieStoreIOS::SystemCookiesAllowed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return [system_store_ cookieAcceptPolicy] ==
         NSHTTPCookieAcceptPolicyAlways;
}

void CookieStoreIOS::AddCookiesToSystemStore(const net::CookieList& cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!SystemCookiesAllowed() || synchronization_state_ != SYNCHRONIZING) {
    // If synchronization was aborted, the pending tasks have been processed at
    // that time. Now is too late.
    DCHECK(tasks_pending_synchronization_.empty());
    return;
  }

  // Report metrics.
  if (metrics_enabled_) {
    size_t cookie_count = cookies.size();
    UMA_HISTOGRAM_COUNTS_10000("CookieIOS.CookieReadCount", cookie_count);
    CheckForCookieLoss(cookie_count, COOKIES_READ);
  }

  net::CookieList::const_iterator it;
  for (it = cookies.begin(); it != cookies.end(); ++it) {
    const net::CanonicalCookie& net_cookie = *it;
    NSHTTPCookie* system_cookie = SystemCookieFromCanonicalCookie(net_cookie);
    // Canonical cookie may not be convertable into system cookie if it contains
    // invalid characters.
    if (!system_cookie)
      continue;
    [system_store_ setCookie:system_cookie];
    creation_time_manager_->SetCreationTime(system_cookie,
                                            net_cookie.CreationDate());
  }

  synchronization_state_ = SYNCHRONIZED;
  RunAllPendingTasks();
}

void CookieStoreIOS::WriteToCookieMonster(NSArray* system_cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (synchronization_state_ != SYNCHRONIZED)
    return;

  // Copy the cookies from the global cookie store to |cookie_monster_|.
  // Unlike the system store, CookieMonster requires unique creation times.
  net::CookieList cookie_list;
  NSUInteger cookie_count = [system_cookies count];
  cookie_list.reserve(cookie_count);
  for (NSHTTPCookie* cookie in system_cookies) {
    cookie_list.push_back(CanonicalCookieFromSystemCookie(
        cookie, creation_time_manager_->GetCreationTime(cookie)));
  }
  cookie_monster_->SetAllCookiesAsync(cookie_list, SetCookiesCallback());

  // Update metrics.
  if (metrics_enabled_)
    UMA_HISTOGRAM_COUNTS_10000("CookieIOS.CookieWrittenCount", cookie_count);
}

void CookieStoreIOS::RunAllPendingTasks() {
  // Executing the tasks while synchronizing would not run the tasks, but merely
  // re-enqueue them. This function also does not support mutation of the queue
  // during the iteration.
  DCHECK(synchronization_state_ != SYNCHRONIZING);
  for (const auto& task : tasks_pending_synchronization_) {
    task.Run();
  }
  tasks_pending_synchronization_.clear();
}

void CookieStoreIOS::DeleteCookiesWithFilter(const CookieFilterFunction& filter,
                                             const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(SYNCHRONIZED, synchronization_state_);
  NSArray* cookies = [system_store_ cookies];

  // Collect the cookies to delete.
  base::scoped_nsobject<NSMutableArray> to_delete(
      [[NSMutableArray alloc] init]);
  for (NSHTTPCookie* cookie in cookies) {
    base::Time creation_time = creation_time_manager_->GetCreationTime(cookie);
    if (filter.Run(cookie, creation_time))
      [to_delete addObject:cookie];
  }

  // Delete them.
  for (NSHTTPCookie* cookie in to_delete.get()) {
    [system_store_ deleteCookie:cookie];
    creation_time_manager_->DeleteCreationTime(cookie);
  }

  if (!callback.is_null())
    callback.Run([to_delete count]);
}

void CookieStoreIOS::OnSystemCookiesChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the CookieStoreIOS is not synchronized or is not backed by
  // |NSHTTPCookieStorage sharedHTTPCookieStorage| this callback is irrelevant.
  if (synchronization_state_ != SYNCHRONIZED ||
      system_store_ != [NSHTTPCookieStorage sharedHTTPCookieStorage]) {
    return;
  }

  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    std::vector<net::CanonicalCookie> removed_cookies;
    std::vector<net::CanonicalCookie> added_cookies;
    if (UpdateCacheForCookieFromSystem(key.first, key.second, &removed_cookies,
                                       &added_cookies)) {
      RunCallbacksForCookies(key.first, key.second, removed_cookies, true);
      RunCallbacksForCookies(key.first, key.second, added_cookies, false);
    }
  }

  // Do not schedule a flush if one is already scheduled.
  if (!flush_closure_.IsCancelled())
    return;

  flush_closure_.Reset(base::Bind(&CookieStoreIOS::FlushStore,
                                  base::Unretained(this), base::Closure()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, flush_closure_.callback(), flush_delay_);
}

scoped_ptr<net::CookieStore::CookieChangedSubscription>
CookieStoreIOS::AddCallbackForCookie(const GURL& gurl,
                                     const std::string& name,
                                     const CookieChangedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prefill cookie cache with all pertinent cookies for |url| if needed.
  std::pair<GURL, std::string> key(gurl, name);
  if (hook_map_.count(key) == 0) {
    UpdateCacheForCookieFromSystem(gurl, name, nullptr, nullptr);
    if (hook_map_.count(key) == 0)
      hook_map_[key] = new CookieChangedCallbackList;
  }

  DCHECK(hook_map_.find(key) != hook_map_.end());
  return hook_map_[key]->Add(callback);
}

bool CookieStoreIOS::UpdateCacheForCookieFromSystem(
    const GURL& gurl,
    const std::string& name,
    std::vector<net::CanonicalCookie>* out_removed_cookies,
    std::vector<net::CanonicalCookie>* out_added_cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<net::CanonicalCookie> system_cookies;
  GetSystemCookies(gurl, name, &system_cookies);
  return cookie_cache_->Update(gurl, name, system_cookies, out_removed_cookies,
                               out_added_cookies);
}

void CookieStoreIOS::RunCallbacksForCookies(
    const GURL& url,
    const std::string& name,
    const std::vector<net::CanonicalCookie>& cookies,
    bool removed) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cookies.empty())
    return;

  std::pair<GURL, std::string> key(url, name);
  CookieChangedCallbackList* callbacks = hook_map_[key];
  for (const auto& cookie : cookies) {
    DCHECK_EQ(name, cookie.Name());
    callbacks->Notify(cookie, removed);
  }
}

bool CookieStoreIOS::GetSystemCookies(
    const GURL& gurl,
    const std::string& name,
    std::vector<net::CanonicalCookie>* cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NSURL* url = net::NSURLWithGURL(gurl);
  NSArray* nscookies = [system_store_ cookiesForURL:url];
  bool found_cookies = false;
  for (NSHTTPCookie* nscookie in nscookies) {
    if (nscookie.name.UTF8String == name) {
      net::CanonicalCookie canonical_cookie = CanonicalCookieFromSystemCookie(
          nscookie, creation_time_manager_->GetCreationTime(nscookie));
      cookies->push_back(canonical_cookie);
      found_cookies = true;
    }
  }
  return found_cookies;
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
    RunCallbacksForCookies(key.first, key.second, removed_cookies, true);
    RunCallbacksForCookies(key.first, key.second, added_cookies, false);
  }
}

void CookieStoreIOS::DidClearNSHTTPCookieStorageCookies(
    const DeleteCallback& delete_callback,
    int num_deleted) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CookieStoreIOSClient* client = net::GetCookieStoreIOSClient();
  DCHECK(client);
  auto sequenced_task_runner = client->GetTaskRunner();
  DCHECK(sequenced_task_runner);
  auto callback = base::Bind(&CookieStoreIOS::DidClearBinaryCookiesFileCookies,
                             this, delete_callback, num_deleted);
  sequenced_task_runner.get()->PostTaskAndReply(
      FROM_HERE, base::Bind(&ClearAllCookiesFromBinaryCookiesFile), callback);
}

void CookieStoreIOS::DidClearBinaryCookiesFileCookies(
    const DeleteCallback& callback,
    int num_deleted_from_nshttp_cookie_storage) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CookieStoreIOSClient* client = net::GetCookieStoreIOSClient();
  DCHECK(client);
  client->DidChangeCookieStorage();
  callback.Run(num_deleted_from_nshttp_cookie_storage);
}

void CookieStoreIOS::UpdateCachesFromCookieMonster() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    GetCookieListCallback callback =
        base::Bind(&CookieStoreIOS::GotCookieListFor, this, key);
    cookie_monster_->GetAllCookiesForURLAsync(key.first, callback);
  }
}

void CookieStoreIOS::UpdateCachesAfterSet(const SetCookiesCallback& callback,
                                          bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (success)
    UpdateCachesFromCookieMonster();
  callback.Run(success);
}

void CookieStoreIOS::UpdateCachesAfterDelete(const DeleteCallback& callback,
                                             int num_deleted) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateCachesFromCookieMonster();
  callback.Run(num_deleted);
}

void CookieStoreIOS::UpdateCachesAfterClosure(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateCachesFromCookieMonster();
  callback.Run();
}

net::CookieList
CookieStoreIOS::CanonicalCookieListFromSystemCookies(NSArray* cookies) {
  net::CookieList cookie_list;
  cookie_list.reserve([cookies count]);
  for (NSHTTPCookie* cookie in cookies) {
    base::Time created = creation_time_manager_->GetCreationTime(cookie);
    cookie_list.push_back(CanonicalCookieFromSystemCookie(cookie, created));
  }
  return cookie_list;
}

CookieStoreIOS::SetCookiesCallback CookieStoreIOS::WrapSetCallback(
    const SetCookiesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Bind(&CookieStoreIOS::UpdateCachesAfterSet, this, callback);
}

CookieStoreIOS::DeleteCallback CookieStoreIOS::WrapDeleteCallback(
    const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Bind(&CookieStoreIOS::UpdateCachesAfterDelete, this, callback);
}

base::Closure CookieStoreIOS::WrapClosure(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Bind(&CookieStoreIOS::UpdateCachesAfterClosure, this, callback);
}

}  // namespace net
