// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_local_storage_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataLocalStorageHelper::MockBrowsingDataLocalStorageHelper(
    Profile* profile)
    : BrowsingDataLocalStorageHelper(profile) {
}

MockBrowsingDataLocalStorageHelper::~MockBrowsingDataLocalStorageHelper() {
}

void MockBrowsingDataLocalStorageHelper::StartFetching(
    const base::Callback<void(const std::list<LocalStorageInfo>&)>& callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = callback;
}

void MockBrowsingDataLocalStorageHelper::DeleteOrigin(
    const GURL& origin) {
  ASSERT_FALSE(callback_.is_null());
  ASSERT_TRUE(origins_.find(origin) != origins_.end());
  last_deleted_origin_ = origin;
  origins_[origin] = false;
}

void MockBrowsingDataLocalStorageHelper::AddLocalStorageSamples() {
  const GURL kOrigin1("http://host1:1/");
  const GURL kOrigin2("http://host2:2/");
  response_.push_back(
      BrowsingDataLocalStorageHelper::LocalStorageInfo(
          kOrigin1, 1, base::Time()));
  origins_[kOrigin1] = true;
  response_.push_back(
      BrowsingDataLocalStorageHelper::LocalStorageInfo(
          kOrigin2, 2, base::Time()));
  origins_[kOrigin2] = true;
}

void MockBrowsingDataLocalStorageHelper::Notify() {
  callback_.Run(response_);
}

void MockBrowsingDataLocalStorageHelper::Reset() {
  for (std::map<const GURL, bool>::iterator i = origins_.begin();
       i != origins_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataLocalStorageHelper::AllDeleted() {
  for (std::map<const GURL, bool>::const_iterator i =
       origins_.begin(); i != origins_.end(); ++i)
    if (i->second)
      return false;
  return true;
}
