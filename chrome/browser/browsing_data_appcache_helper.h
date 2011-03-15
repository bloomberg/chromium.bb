// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_APPCACHE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_APPCACHE_HELPER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "googleurl/src/gurl.h"

class Profile;

// This class fetches appcache information on behalf of a caller
// on the UI thread.
class BrowsingDataAppCacheHelper
    : public base::RefCountedThreadSafe<BrowsingDataAppCacheHelper> {
 public:
  explicit BrowsingDataAppCacheHelper(Profile* profile);

  virtual void StartFetching(Callback0::Type* completion_callback);
  virtual void CancelNotification();
  virtual void DeleteAppCacheGroup(const GURL& manifest_url);

  appcache::AppCacheInfoCollection* info_collection() const {
    DCHECK(!is_fetching_);
    return info_collection_;
  }

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataAppCacheHelper>;
  virtual ~BrowsingDataAppCacheHelper();

  scoped_ptr<Callback0::Type> completion_callback_;
  scoped_refptr<appcache::AppCacheInfoCollection> info_collection_;

 private:
  void OnFetchComplete(int rv);
  ChromeAppCacheService* GetAppCacheService();

  scoped_refptr<URLRequestContextGetter> request_context_getter_;
  bool is_fetching_;
  scoped_refptr<net::CancelableCompletionCallback<BrowsingDataAppCacheHelper> >
      appcache_info_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataAppCacheHelper);
};

// This class is a thin wrapper around BrowsingDataAppCacheHelper that does not
// fetch its information from the appcache service, but gets them passed as
// a parameter during construction.
class CannedBrowsingDataAppCacheHelper : public BrowsingDataAppCacheHelper {
 public:
  explicit CannedBrowsingDataAppCacheHelper(Profile* profile);

  // Return a copy of the appcache helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // everytime we instantiate a cookies tree model for it.
  CannedBrowsingDataAppCacheHelper* Clone();

  // Add an appcache to the set of canned caches that is returned by this
  // helper.
  void AddAppCache(const GURL& manifest_url);

  // Clears the list of canned caches.
  void Reset();

  // True if no appcaches are currently stored.
  bool empty() const;

  // BrowsingDataAppCacheHelper methods.
  virtual void StartFetching(Callback0::Type* completion_callback);
  virtual void CancelNotification() {}

 private:
  virtual ~CannedBrowsingDataAppCacheHelper();

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataAppCacheHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_APPCACHE_HELPER_H_
