// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_APPCACHE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_APPCACHE_HELPER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/appcache_service.h"
#include "net/base/completion_callback.h"
#include "url/gurl.h"

class Profile;

namespace content {
class AppCacheService;
}

// This class fetches appcache information on behalf of a caller
// on the UI thread.
class BrowsingDataAppCacheHelper
    : public base::RefCountedThreadSafe<BrowsingDataAppCacheHelper> {
 public:
  typedef std::map<GURL, content::AppCacheInfoVector> OriginAppCacheInfoMap;

  explicit BrowsingDataAppCacheHelper(Profile* profile);

  virtual void StartFetching(const base::Closure& completion_callback);
  virtual void DeleteAppCacheGroup(const GURL& manifest_url);

  content::AppCacheInfoCollection* info_collection() const {
    DCHECK(!is_fetching_);
    return info_collection_.get();
  }

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataAppCacheHelper>;
  virtual ~BrowsingDataAppCacheHelper();

  base::Closure completion_callback_;
  scoped_refptr<content::AppCacheInfoCollection> info_collection_;

 private:
  void OnFetchComplete(int rv);

  bool is_fetching_;
  content::AppCacheService* appcache_service_;
  net::CancelableCompletionCallback appcache_info_callback_;

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
  // every time we instantiate a cookies tree model for it.
  CannedBrowsingDataAppCacheHelper* Clone();

  // Add an appcache to the set of canned caches that is returned by this
  // helper.
  void AddAppCache(const GURL& manifest_url);

  // Clears the list of canned caches.
  void Reset();

  // True if no appcaches are currently stored.
  bool empty() const;

  // Returns the number of app cache resources.
  size_t GetAppCacheCount() const;

  // Returns a current map with the |AppCacheInfoVector|s per origin.
  const OriginAppCacheInfoMap& GetOriginAppCacheInfoMap() const;

  // BrowsingDataAppCacheHelper methods.
  virtual void StartFetching(const base::Closure& completion_callback) OVERRIDE;
  virtual void DeleteAppCacheGroup(const GURL& manifest_url) OVERRIDE;

 private:
  virtual ~CannedBrowsingDataAppCacheHelper();

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataAppCacheHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_APPCACHE_HELPER_H_
