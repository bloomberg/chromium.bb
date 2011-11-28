// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOCK_BROWSING_DATA_APPCACHE_HELPER_H_
#define CHROME_BROWSER_MOCK_BROWSING_DATA_APPCACHE_HELPER_H_
#pragma once

#include "base/callback_forward.h"
#include "chrome/browser/browsing_data_appcache_helper.h"

class MockBrowsingDataAppCacheHelper
    : public BrowsingDataAppCacheHelper {
 public:
  explicit MockBrowsingDataAppCacheHelper(Profile* profile);

  virtual void StartFetching(const base::Closure& completion_callback) OVERRIDE;
  virtual void CancelNotification() OVERRIDE;
  virtual void DeleteAppCacheGroup(const GURL& manifest_url) OVERRIDE;

 private:
  virtual ~MockBrowsingDataAppCacheHelper();
};

#endif  // CHROME_BROWSER_MOCK_BROWSING_DATA_APPCACHE_HELPER_H_
