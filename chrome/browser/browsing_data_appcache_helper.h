// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_APPCACHE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_APPCACHE_HELPER_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/appcache/chrome_appcache_service.h"
#include "chrome/browser/net/url_request_context_getter.h"

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

 private:
  friend class base::RefCountedThreadSafe<BrowsingDataAppCacheHelper>;
  friend class MockBrowsingDataAppCacheHelper;

  virtual ~BrowsingDataAppCacheHelper() {}

  void OnFetchComplete(int rv);
  ChromeAppCacheService* GetAppCacheService();

  scoped_refptr<URLRequestContextGetter> request_context_getter_;
  bool is_fetching_;
  scoped_ptr<Callback0::Type> completion_callback_;
  scoped_refptr<appcache::AppCacheInfoCollection> info_collection_;
  scoped_refptr<net::CancelableCompletionCallback<BrowsingDataAppCacheHelper> >
      appcache_info_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataAppCacheHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_APPCACHE_HELPER_H_
