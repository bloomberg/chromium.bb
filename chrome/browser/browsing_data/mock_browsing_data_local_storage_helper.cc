// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_local_storage_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataLocalStorageHelper::MockBrowsingDataLocalStorageHelper(
    Profile* profile)
    : BrowsingDataLocalStorageHelper(profile) {
}

MockBrowsingDataLocalStorageHelper::~MockBrowsingDataLocalStorageHelper() {
}

void MockBrowsingDataLocalStorageHelper::StartFetching(
    const FetchCallback& callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = callback;
}

void MockBrowsingDataLocalStorageHelper::DeleteOrigin(
    const GURL& origin) {
  ASSERT_FALSE(callback_.is_null());
  ASSERT_TRUE(base::ContainsKey(origins_, origin));
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

void MockBrowsingDataLocalStorageHelper::
    AddLocalStorageSamplesWithSuborigins() {
  AddLocalStorageSamples();
  const GURL kSuborigin3("http-so://foobar.host3:3");
  const GURL kOrigin4("http://host4:4");
  const GURL kSuborigin4("http-so://foobar.host4:4");
  response_.push_back(BrowsingDataLocalStorageHelper::LocalStorageInfo(
      kSuborigin3, 1, base::Time()));
  origins_[kSuborigin3] = true;
  response_.push_back(BrowsingDataLocalStorageHelper::LocalStorageInfo(
      kOrigin4, 1, base::Time()));
  origins_[kOrigin4] = true;
  response_.push_back(BrowsingDataLocalStorageHelper::LocalStorageInfo(
      kSuborigin4, 1, base::Time()));
  origins_[kSuborigin4] = true;
}

void MockBrowsingDataLocalStorageHelper::Notify() {
  callback_.Run(response_);
}

void MockBrowsingDataLocalStorageHelper::Reset() {
  for (auto& pair : origins_)
    pair.second = true;
}

bool MockBrowsingDataLocalStorageHelper::AllDeleted() {
  for (const auto& pair : origins_) {
    if (pair.second)
      return false;
  }
  return true;
}
