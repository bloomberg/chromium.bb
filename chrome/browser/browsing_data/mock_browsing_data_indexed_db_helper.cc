// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_indexed_db_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataIndexedDBHelper::MockBrowsingDataIndexedDBHelper(
    Profile* profile)
    : BrowsingDataIndexedDBHelper(
        content::BrowserContext::GetDefaultStoragePartition(profile)->
            GetIndexedDBContext()) {
}

MockBrowsingDataIndexedDBHelper::~MockBrowsingDataIndexedDBHelper() {
}

void MockBrowsingDataIndexedDBHelper::StartFetching(
    const base::Callback<void(const std::list<content::IndexedDBInfo>&)>&
    callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = callback;
}

void MockBrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const GURL& origin) {
  ASSERT_FALSE(callback_.is_null());
  ASSERT_TRUE(origins_.find(origin) != origins_.end());
  origins_[origin] = false;
}

void MockBrowsingDataIndexedDBHelper::AddIndexedDBSamples() {
  const GURL kOrigin1("http://idbhost1:1/");
  const GURL kOrigin2("http://idbhost2:2/");
  content::IndexedDBInfo info1(kOrigin1, 1, base::Time(), base::FilePath(), 0);
  response_.push_back(info1);
  origins_[kOrigin1] = true;
  content::IndexedDBInfo info2(kOrigin2, 2, base::Time(), base::FilePath(), 0);
  response_.push_back(info2);
  origins_[kOrigin2] = true;
}

void MockBrowsingDataIndexedDBHelper::Notify() {
  callback_.Run(response_);
}

void MockBrowsingDataIndexedDBHelper::Reset() {
  for (std::map<GURL, bool>::iterator i = origins_.begin();
       i != origins_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataIndexedDBHelper::AllDeleted() {
  for (std::map<GURL, bool>::const_iterator i = origins_.begin();
       i != origins_.end(); ++i)
    if (i->second)
      return false;
  return true;
}
