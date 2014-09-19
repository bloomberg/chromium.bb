// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_appcache_helper.h"

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataAppCacheHelper::MockBrowsingDataAppCacheHelper(
    content::BrowserContext* browser_context)
    : BrowsingDataAppCacheHelper(browser_context) {
}

MockBrowsingDataAppCacheHelper::~MockBrowsingDataAppCacheHelper() {
}

void MockBrowsingDataAppCacheHelper::StartFetching(
    const base::Closure& completion_callback) {
  ASSERT_FALSE(completion_callback.is_null());
  ASSERT_TRUE(completion_callback_.is_null());
  completion_callback_ = completion_callback;
}

void MockBrowsingDataAppCacheHelper::DeleteAppCacheGroup(
    const GURL& manifest_url) {
}
