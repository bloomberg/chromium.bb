// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_browsing_data_appcache_helper.h"

#include "base/callback.h"

MockBrowsingDataAppCacheHelper::MockBrowsingDataAppCacheHelper(
  Profile* profile)
  : BrowsingDataAppCacheHelper(profile) {
}

MockBrowsingDataAppCacheHelper::~MockBrowsingDataAppCacheHelper() {
}

void MockBrowsingDataAppCacheHelper::StartFetching(
    Callback0::Type* completion_callback) {
  completion_callback_.reset(completion_callback);
}

void MockBrowsingDataAppCacheHelper::CancelNotification() {
  completion_callback_.reset(NULL);
}

void MockBrowsingDataAppCacheHelper::DeleteAppCacheGroup(
    const GURL& manifest_url) {
}
