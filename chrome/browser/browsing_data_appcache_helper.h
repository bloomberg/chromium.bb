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

class Profile;

// This class fetches appcache information on behalf of a caller
// on the UI thread.
class BrowsingDataAppCacheHelper
    : public base::RefCountedThreadSafe<BrowsingDataAppCacheHelper> {
 public:
  // Contains detailed information about an appcache.
  struct AppCacheInfo {
    AppCacheInfo() {}
    AppCacheInfo(const GURL& manifest_url,
                 int64 size,
                 base::Time creation_time,
                 base::Time last_access_time,
                 int64 group_id)
        : manifest_url(manifest_url),
          size(size),
          creation_time(creation_time),
          last_access_time(last_access_time),
          group_id(group_id) {
    }

    GURL manifest_url;
    int64 size;
    base::Time creation_time;
    base::Time last_access_time;
    int64 group_id;
  };

  explicit BrowsingDataAppCacheHelper(Profile* profile);

  virtual void StartFetching(Callback0::Type* completion_callback);
  virtual void CancelNotification();
  virtual void DeleteAppCache(int64 group_id);

  const std::vector<AppCacheInfo>& info_list() const {
    DCHECK(!is_fetching_);
    return info_list_;
  }

 private:
  friend class base::RefCountedThreadSafe<BrowsingDataAppCacheHelper>;
  friend class MockBrowsingDataAppCacheHelper;

  virtual ~BrowsingDataAppCacheHelper() {}

  void StartFetchingInIOThread();
  void OnFetchComplete();
  void DeleteAppCacheInIOThread(int64 group_id);

  bool is_fetching_;
  std::vector<AppCacheInfo> info_list_;
  scoped_ptr<Callback0::Type> completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataAppCacheHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_APPCACHE_HELPER_H_
