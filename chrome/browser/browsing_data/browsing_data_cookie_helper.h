// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COOKIE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COOKIE_HELPER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "net/cookies/cookie_monster.h"

class GURL;

namespace net {
class CanonicalCookie;
class URLRequestContextGetter;
}

// This class fetches cookie information on behalf of a caller
// on the UI thread.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI
// thread at some later point.
class BrowsingDataCookieHelper
    : public base::RefCountedThreadSafe<BrowsingDataCookieHelper> {
 public:
  explicit BrowsingDataCookieHelper(
      net::URLRequestContextGetter* request_context_getter);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      const base::Callback<void(const net::CookieList& cookies)>& callback);

  // Requests a single cookie to be deleted in the IO thread. This must be
  // called in the UI thread.
  virtual void DeleteCookie(const net::CanonicalCookie& cookie);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataCookieHelper>;
  virtual ~BrowsingDataCookieHelper();

  net::URLRequestContextGetter* request_context_getter() {
    return request_context_getter_.get();
  }

 private:
  // Fetch the cookies. This must be called in the IO thread.
  void FetchCookiesOnIOThread();

  // Callback function for get cookie. This must be called in the IO thread.
  void OnFetchComplete(const net::CookieList& cookies);

  // Notifies the completion callback. This must be called in the UI thread.
  void NotifyInUIThread(const net::CookieList& cookies);

  // Delete a single cookie. This must be called in IO thread.
  void DeleteCookieOnIOThread(const net::CanonicalCookie& cookie);

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notify the callback in the UI thread.
  // This member is only mutated on the UI thread.
  bool is_fetching_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // This member is only mutated on the UI thread.
  base::Callback<void(const net::CookieList& cookies)> completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataCookieHelper);
};

// This class is a thin wrapper around BrowsingDataCookieHelper that does not
// fetch its information from the persistent cookie store. It is a simple
// container for CanonicalCookies. Clients that use this container can add
// cookies that are sent to a server via the AddReadCookies method and cookies
// that are received from a server or set via JavaScript using the method
// AddChangedCookie.
// Cookies are distinguished by the tuple cookie name (called cookie-name in
// RFC 6265), cookie domain (called cookie-domain in RFC 6265), cookie path
// (called cookie-path in RFC 6265) and host-only-flag (see RFC 6265 section
// 5.3). Cookies with same tuple (cookie-name, cookie-domain, cookie-path,
// host-only-flag) as cookie that are already stored, will replace the stored
// cookies.
class CannedBrowsingDataCookieHelper : public BrowsingDataCookieHelper {
 public:
  typedef std::map<GURL, net::CookieList*> OriginCookieListMap;

  explicit CannedBrowsingDataCookieHelper(
      net::URLRequestContextGetter* request_context);

  // Return a copy of the cookie helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // everytime we instantiate a cookies tree model for it.
  CannedBrowsingDataCookieHelper* Clone();

  // Adds the cookies from |cookie_list|. Current cookies that have the same
  // cookie name, cookie domain, cookie path, host-only-flag tuple as passed
  // cookies are replaced by the passed cookies.
  void AddReadCookies(const GURL& frame_url,
                      const GURL& request_url,
                      const net::CookieList& cookie_list);

  // Adds a CanonicalCookie that is created from the passed |cookie_line|
  // (called set-cookie-string in RFC 6225). The |cookie_line| is parsed,
  // normalized and validated. Invalid |cookie_line|s are ignored. The logic
  // for parsing, normalizing an validating the |cookie_line| mirrors the logic
  // of CookieMonster's method SetCookieWithOptions. If the |cookie_line| does
  // not include a cookie domain attribute (called domain-av in RFC 6265) or a
  // cookie path (called path-av in RFC 6265), then the host and the
  // default-path of the request-uri are used as domain-value and path-value
  // for the cookie. CanonicalCookies created from a |cookie_line| with no
  // cookie domain attribute are host only cookies.
  // TODO(markusheintz): Remove the dublicated logic.
  void AddChangedCookie(const GURL& frame_url,
                        const GURL& request_url,
                        const std::string& cookie_line,
                        const net::CookieOptions& options);

  // Clears the list of canned cookies.
  void Reset();

  // True if no cookie are currently stored.
  bool empty() const;

  // BrowsingDataCookieHelper methods.
  virtual void StartFetching(
      const net::CookieMonster::GetCookieListCallback& callback) OVERRIDE;
  virtual void DeleteCookie(const net::CanonicalCookie& cookie) OVERRIDE;

  // Returns the number of stored cookies.
  size_t GetCookieCount() const;

  // Returns the map that contains the cookie lists for all frame urls.
  const OriginCookieListMap& origin_cookie_list_map() {
    return origin_cookie_list_map_;
  }

 private:
  // Check if the cookie list contains a cookie with the same name,
  // domain, and path as the newly created cookie. Delete the old cookie
  // if does.
  bool DeleteMatchingCookie(const net::CanonicalCookie& add_cookie,
                            net::CookieList* cookie_list);

  virtual ~CannedBrowsingDataCookieHelper();

  // Returns the |CookieList| for the given |origin|.
  net::CookieList* GetCookiesFor(const GURL& origin);

  // Adds the |cookie| to the cookie list for the given |frame_url|.
  void AddCookie(const GURL& frame_url,
                 const net::CanonicalCookie& cookie);

  // Map that contains the cookie lists for all frame origins.
  OriginCookieListMap origin_cookie_list_map_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataCookieHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COOKIE_HELPER_H_
