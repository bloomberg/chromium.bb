// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_appcache_helper.h"

#include "base/callback.h"

MockBrowsingDataAppCacheHelper::MockBrowsingDataAppCacheHelper(
  Profile* profile)
  : BrowsingDataAppCacheHelper(profile) {
}

MockBrowsingDataAppCacheHelper::~MockBrowsingDataAppCacheHelper() {
}

void MockBrowsingDataAppCacheHelper::StartFetching(
    const base::Closure& completion_callback) {
  completion_callback_ = completion_callback;
}

void MockBrowsingDataAppCacheHelper::DeleteAppCacheGroup(
    const GURL& manifest_url) {
}
