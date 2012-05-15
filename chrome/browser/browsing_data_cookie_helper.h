// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COOKIE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_COOKIE_HELPER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "net/cookies/cookie_monster.h"

class GURL;
class Profile;

namespace net {
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
  explicit BrowsingDataCookieHelper(Profile* profile);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      const base::Callback<void(const net::CookieList& cookies)>& callback);

  // Requests a single cookie to be deleted in the IO thread. This must be
  // called in the UI thread.
  virtual void DeleteCookie(const net::CookieMonster::CanonicalCookie& cookie);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataCookieHelper>;
  virtual ~BrowsingDataCookieHelper();
  Profile* profile() { return profile_; }

 private:
  // Fetch the cookies. This must be called in the IO thread.
  void FetchCookiesOnIOThread();

  // Callback function for get cookie. This must be called in the IO thread.
  void OnFetchComplete(const net::CookieList& cookies);

  // Notifies the completion callback. This must be called in the UI thread.
  void NotifyInUIThread(const net::CookieList& cookies);

  // Delete a single cookie. This must be called in IO thread.
  void DeleteCookieOnIOThread(
      const net::CookieMonster::CanonicalCookie& cookie);

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notify the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  Profile* profile_;

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // This only mutates on the UI thread.
  base::Callback<void(const net::CookieList& cookies)> completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataCookieHelper);
};

// This class is a thin wrapper around BrowsingDataCookieHelper that does not
// fetch its information from the persistent cookie store, but gets them passed
// as a parameter during construction.
class CannedBrowsingDataCookieHelper : public BrowsingDataCookieHelper {
 public:
  typedef std::map<GURL, net::CookieList*> OriginCookieListMap;

  explicit CannedBrowsingDataCookieHelper(Profile* profile);

  // Return a copy of the cookie helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // everytime we instantiate a cookies tree model for it.
  CannedBrowsingDataCookieHelper* Clone();

  // Adds cookies and delete the current cookies with the same Name, Domain,
  // and Path as the newly created ones.
  void AddReadCookies(const GURL& frame_url,
                      const GURL& request_url,
                      const net::CookieList& cookie_list);

  // Adds cookies that will be stored by the CookieMonster. Designed to mirror
  // the logic of SetCookieWithOptions.
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
  bool DeleteMatchingCookie(
      const net::CookieMonster::CanonicalCookie& add_cookie,
      net::CookieList* cookie_list);

  virtual ~CannedBrowsingDataCookieHelper();

  // Returns the |CookieList| for the given |origin|.
  net::CookieList* GetCookiesFor(const GURL& origin);

  // Adds the |cookie| to the cookie list for the given |frame_url|.
  void AddCookie(
      const GURL& frame_url,
      const net::CookieMonster::CanonicalCookie& cookie);

  // Map that contains the cookie lists for all frame origins.
  OriginCookieListMap origin_cookie_list_map_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataCookieHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COOKIE_HELPER_H_
