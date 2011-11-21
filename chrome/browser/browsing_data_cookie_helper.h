// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COOKIE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_COOKIE_HELPER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "net/base/cookie_monster.h"

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
// The client must call CancelNotification() if it's destroyed before the
// callback is notified.
class BrowsingDataCookieHelper
    : public base::RefCountedThreadSafe<BrowsingDataCookieHelper> {
 public:
  explicit BrowsingDataCookieHelper(Profile* profile);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      const base::Callback<void(const net::CookieList& cookies)>& callback);

  // Cancels the notification callback (i.e., the window that created it no
  // longer exists).
  // This must be called only in the UI thread.
  virtual void CancelNotification();

  // Requests a single cookie to be deleted in the IO thread. This must be
  // called in the UI thread.
  virtual void DeleteCookie(const net::CookieMonster::CanonicalCookie& cookie);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataCookieHelper>;
  virtual ~BrowsingDataCookieHelper();

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
  explicit CannedBrowsingDataCookieHelper(Profile* profile);

  // Return a copy of the cookie helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // everytime we instantiate a cookies tree model for it.
  CannedBrowsingDataCookieHelper* Clone();

  // Adds cookies and delete the current cookies with the same Name, Domain,
  // and Path as the newly created ones.
  void AddReadCookies(const GURL& url,
                      const net::CookieList& cookie_list);

  // Adds cookies that will be stored by the CookieMonster. Designed to mirror
  // the logic of SetCookieWithOptions.
  void AddChangedCookie(const GURL& url,
                        const std::string& cookie_line,
                        const net::CookieOptions& options);

  // Clears the list of canned cookies.
  void Reset();

  // True if no cookie are currently stored.
  bool empty() const;

  // BrowsingDataCookieHelper methods.
  virtual void StartFetching(
      const net::CookieMonster::GetCookieListCallback& callback) OVERRIDE;
  virtual void CancelNotification() OVERRIDE;

 private:
  // Check if the cookie list contains a cookie with the same name,
  // domain, and path as the newly created cookie. Delete the old cookie
  // if does.
  bool DeleteMetchingCookie(
      const net::CookieMonster::CanonicalCookie& add_cookie);

  virtual ~CannedBrowsingDataCookieHelper();

  net::CookieList cookie_list_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataCookieHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COOKIE_HELPER_H_
