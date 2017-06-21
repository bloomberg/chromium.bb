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
#include "ios/net/cookies/cookie_creation_time_manager.h"
#include "ios/net/cookies/cookie_store_ios_client.h"
#include "ios/net/cookies/system_cookie_util.h"
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
// Note that the .binarycookies file is present only on iOS8+.
void ClearAllCookiesFromBinaryCookiesFile() {
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
#if !BUILDFLAG(CRONET_BUILD)
  // CookieCreationTimeManager is returning creation times that are null.
  // Since in Cronet, the cookie store is recreated on startup, let's suppress
  // this warning for now.
  // TODO(mef): Instead of suppressing the warning, assign a creation time
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
std::string BuildCookieLineWithOptions(NSArray* cookies,
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

CookieStoreIOS::CookieStoreIOS(NSHTTPCookieStorage* cookie_storage)
    : CookieStoreIOS(nullptr, cookie_storage) {}

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
    const SetCookiesCallback& callback) {
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
    [system_store_ setCookie:cookie];
    creation_time_manager_->SetCreationTime(
        cookie,
        creation_time_manager_->MakeUniqueCreationTime(base::Time::Now()));
  }

  if (!callback.is_null())
    callback.Run(success);
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
    CookieSameSite same_site,
    CookiePriority priority,
    const SetCookiesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If cookies are not allowed, they are stashed in the CookieMonster, and
  // should be written there instead.
  DCHECK(SystemCookiesAllowed());

  bool success = false;

  if (creation_time.is_null())
    creation_time = base::Time::Now();

  // Validate consistency of passed arguments.
  if (ParsedCookie::ParseTokenString(name) != name ||
      ParsedCookie::ParseValueString(value) != value ||
      ParsedCookie::ParseValueString(domain) != domain ||
      ParsedCookie::ParseValueString(path) != path) {
    if (!callback.is_null())
      callback.Run(false);
    return;
  }

  // Validate passed arguments against URL.
  std::string cookie_domain;
  std::string cookie_path = CanonicalCookie::CanonPathWithString(url, path);
  if ((secure && !url.SchemeIsCryptographic()) ||
      !cookie_util::GetCookieDomainWithString(url, domain, &cookie_domain) ||
      (!path.empty() && cookie_path != path)) {
    if (!callback.is_null())
      callback.Run(false);
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
      [system_store_ setCookie:cookie];
      creation_time_manager_->SetCreationTime(
          cookie, creation_time_manager_->MakeUniqueCreationTime(
              canonical_cookie->CreationDate()));
      success = true;
    }
  }

  if (!callback.is_null())
    callback.Run(success);
}

void CookieStoreIOS::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    bool secure_source,
    bool modify_http_only,
    const SetCookiesCallback& callback) {
  DCHECK(cookie->IsCanonical());
  // The exclude_httponly() option would only be used by a javascript
  // engine.
  DCHECK(modify_http_only);

  if (cookie->IsSecure() && !secure_source) {
    if (!callback.is_null())
      callback.Run(false);
    return;
  }

  NSHTTPCookie* ns_cookie = SystemCookieFromCanonicalCookie(*cookie.get());

  if (ns_cookie != nil) {
    [system_store_ setCookie:ns_cookie];
    creation_time_manager_->SetCreationTime(
        ns_cookie,
        creation_time_manager_->MakeUniqueCreationTime(
            cookie->CreationDate().is_null() ? base::Time::Now()
                                             : cookie->CreationDate()));
    if (!callback.is_null())
      callback.Run(true);
    return;
  }

  if (!callback.is_null())
    callback.Run(false);
}

void CookieStoreIOS::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookiesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If cookies are not allowed, they are stashed in the CookieMonster, and
  // should be read from there instead.
  DCHECK(SystemCookiesAllowed());
  // The exclude_httponly() option would only be used by a javascript
  // engine.
  DCHECK(!options.exclude_httponly());

  // TODO(mkwst): If/when iOS supports Same-Site cookies, we'll need to pass
  // options in here as well. https://crbug.com/459154
  NSArray* cookies =
      GetCookiesForURL(system_store_, url, creation_time_manager_.get());
  if (!callback.is_null())
    callback.Run(BuildCookieLineWithOptions(cookies, options));
}

void CookieStoreIOS::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookieListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!SystemCookiesAllowed()) {
    // If cookies are not allowed, the cookies are stashed in the
    // CookieMonster, so get them from there.
    cookie_monster_->GetCookieListWithOptionsAsync(url, options, callback);
    return;
  }

  // TODO(mkwst): If/when iOS supports Same-Site cookies, we'll need to pass
  // options in here as well. https://crbug.com/459154
  NSArray* cookies =
      GetCookiesForURL(system_store_, url, creation_time_manager_.get());
  net::CookieList cookie_list = CanonicalCookieListFromSystemCookies(cookies);
  if (!callback.is_null())
    callback.Run(cookie_list);
}

void CookieStoreIOS::GetAllCookiesAsync(const GetCookieListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!SystemCookiesAllowed()) {
    // If cookies are not allowed, the cookies are stashed in the
    // CookieMonster, so get them from there.
    cookie_monster_->GetAllCookiesAsync(callback);
    return;
  }

  NSArray* cookies = GetAllCookies(system_store_, creation_time_manager_.get());
  net::CookieList cookie_list = CanonicalCookieListFromSystemCookies(cookies);
  if (!callback.is_null()) {
    callback.Run(cookie_list);
  }
}

void CookieStoreIOS::DeleteCookieAsync(const GURL& url,
                                       const std::string& cookie_name,
                                       const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  NSArray* cookies =
      GetCookiesForURL(system_store_, url, creation_time_manager_.get());
  for (NSHTTPCookie* cookie in cookies) {
    if ([[cookie name] isEqualToString:base::SysUTF8ToNSString(cookie_name)]) {
      [system_store_ deleteCookie:cookie];
      creation_time_manager_->DeleteCreationTime(cookie);
    }
  }

  if (!callback.is_null())
    callback.Run();
}

void CookieStoreIOS::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This relies on the fact cookies are given unique creation dates.
  CookieFilterFunction filter = base::Bind(
      IsCookieCreatedBetween, cookie.CreationDate(), cookie.CreationDate());
  DeleteCookiesWithFilter(filter, callback);
}

void CookieStoreIOS::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  CookieFilterFunction filter = base::Bind(
      &IsCookieCreatedBetween, delete_begin, delete_end);
  DeleteCookiesWithFilter(filter, callback);
}

void CookieStoreIOS::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  CookieFilterFunction filter = base::Bind(
      IsCookieCreatedBetweenWithPredicate, delete_begin, delete_end, predicate);
  DeleteCookiesWithFilter(filter, callback);
}

void CookieStoreIOS::DeleteSessionCookiesAsync(const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (metrics_enabled_)
    ResetCookieCountMetrics();

  CookieFilterFunction filter = base::Bind(&IsCookieSessionCookie);
  DeleteCookiesWithFilter(filter, callback);
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

CookieStoreIOS::CookieStoreIOS(
    net::CookieMonster::PersistentCookieStore* persistent_store,
    NSHTTPCookieStorage* system_store)
    : cookie_monster_(new net::CookieMonster(persistent_store, nullptr)),
      system_store_(system_store),
      creation_time_manager_(new CookieCreationTimeManager),
      metrics_enabled_(false),
      cookie_cache_(new CookieCache()),
      weak_factory_(this) {
  DCHECK(system_store);

  NotificationTrampoline::GetInstance()->AddObserver(this);

  cookie_monster_->SetPersistSessionCookies(true);
  cookie_monster_->SetForceKeepSessionState();
}

CookieStoreIOS::SetCookiesCallback CookieStoreIOS::WrapSetCallback(
    const SetCookiesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Bind(&CookieStoreIOS::UpdateCachesAfterSet,
                    weak_factory_.GetWeakPtr(), callback);
}

CookieStoreIOS::DeleteCallback CookieStoreIOS::WrapDeleteCallback(
    const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Bind(&CookieStoreIOS::UpdateCachesAfterDelete,
                    weak_factory_.GetWeakPtr(), callback);
}

base::Closure CookieStoreIOS::WrapClosure(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::Bind(&CookieStoreIOS::UpdateCachesAfterClosure,
                    weak_factory_.GetWeakPtr(), callback);
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

bool CookieStoreIOS::SystemCookiesAllowed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return [system_store_ cookieAcceptPolicy] != NSHTTPCookieAcceptPolicyNever;
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
        cookie, creation_time_manager_->GetCreationTime(cookie)));
  }
  cookie_monster_->SetAllCookiesAsync(cookie_list, SetCookiesCallback());

  // Update metrics.
  if (metrics_enabled_)
    UMA_HISTOGRAM_COUNTS_10000("CookieIOS.CookieWrittenCount", cookie_count);
}

void CookieStoreIOS::DeleteCookiesWithFilter(const CookieFilterFunction& filter,
                                             const DeleteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
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

  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    std::vector<net::CanonicalCookie> removed_cookies;
    std::vector<net::CanonicalCookie> added_cookies;
    if (UpdateCacheForCookieFromSystem(key.first, key.second, &removed_cookies,
                                       &added_cookies)) {
      RunCallbacksForCookies(key.first, key.second, removed_cookies,
                             net::CookieStore::ChangeCause::UNKNOWN_DELETION);
      RunCallbacksForCookies(key.first, key.second, added_cookies,
                             net::CookieStore::ChangeCause::INSERTED);
    }
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
    UpdateCacheForCookieFromSystem(gurl, name, nullptr, nullptr);
    if (hook_map_.count(key) == 0)
      hook_map_[key] = base::MakeUnique<CookieChangedCallbackList>();
  }

  DCHECK(hook_map_.find(key) != hook_map_.end());
  return hook_map_[key]->Add(callback);
}

bool CookieStoreIOS::IsEphemeral() {
  return cookie_monster_->IsEphemeral();
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
    RunCallbacksForCookies(key.first, key.second, removed_cookies,
                           net::CookieStore::ChangeCause::UNKNOWN_DELETION);
    RunCallbacksForCookies(key.first, key.second, added_cookies,
                           net::CookieStore::ChangeCause::INSERTED);
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
  auto callback =
      base::Bind(&CookieStoreIOS::DidClearBinaryCookiesFileCookies,
                 weak_factory_.GetWeakPtr(), delete_callback, num_deleted);
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
  if (!callback.is_null())
    callback.Run(num_deleted_from_nshttp_cookie_storage);
}

void CookieStoreIOS::UpdateCachesFromCookieMonster() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& hook_map_entry : hook_map_) {
    std::pair<GURL, std::string> key = hook_map_entry.first;
    GetCookieListCallback callback = base::Bind(
        &CookieStoreIOS::GotCookieListFor, weak_factory_.GetWeakPtr(), key);
    cookie_monster_->GetAllCookiesForURLAsync(key.first, callback);
  }
}

void CookieStoreIOS::UpdateCachesAfterSet(const SetCookiesCallback& callback,
                                          bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (success)
    UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    callback.Run(success);
}

void CookieStoreIOS::UpdateCachesAfterDelete(const DeleteCallback& callback,
                                             int num_deleted) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateCachesFromCookieMonster();
  if (!callback.is_null())
    callback.Run(num_deleted);
}

void CookieStoreIOS::UpdateCachesAfterClosure(const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UpdateCachesFromCookieMonster();
  if (!callback.is_null())
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

}  // namespace net
