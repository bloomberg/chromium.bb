// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_appcache_helper.h"

BrowsingDataAppCacheHelper::BrowsingDataAppCacheHelper(Profile* profile)
    : is_fetching_(false) {
}

void BrowsingDataAppCacheHelper::StartFetching(Callback0::Type* callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(callback);
  is_fetching_ = true;
  info_list_.clear();
  completion_callback_.reset(callback);
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, NewRunnableMethod(
      this, &BrowsingDataAppCacheHelper::StartFetchingInIOThread));
}

void BrowsingDataAppCacheHelper::CancelNotification() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  completion_callback_.reset();
}

void BrowsingDataAppCacheHelper::DeleteAppCache(int64 group_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, NewRunnableMethod(
      this, &BrowsingDataAppCacheHelper::DeleteAppCacheInIOThread,
      group_id));
}

void BrowsingDataAppCacheHelper::StartFetchingInIOThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

#ifndef NDEBUG
  // TODO(michaeln): get real data from the appcache service
  info_list_.push_back(
      AppCacheInfo(GURL("http://bogus/manifest"),
      1 * 1024 * 1024, base::Time::Now(), base::Time::Now(), 1));
  info_list_.push_back(
      AppCacheInfo(GURL("https://bogus/manifest"),
      2 * 1024 * 1024, base::Time::Now(), base::Time::Now(), 2));
  info_list_.push_back(
      AppCacheInfo(GURL("http://bogus:8080/manifest"),
      3 * 1024 * 1024, base::Time::Now(), base::Time::Now(), 3));
#endif

  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(
      this, &BrowsingDataAppCacheHelper::OnFetchComplete));
}

void BrowsingDataAppCacheHelper::OnFetchComplete() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(is_fetching_);
  is_fetching_ = false;
  if (completion_callback_ != NULL) {
    completion_callback_->Run();
    completion_callback_.reset();
  }
}

void BrowsingDataAppCacheHelper::DeleteAppCacheInIOThread(
    int64 group_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // TODO(michaeln): plumb to the appcache service
}
