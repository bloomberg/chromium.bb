// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_browsing_data_indexed_db_helper.h"

#include "base/callback.h"
#include "base/logging.h"

MockBrowsingDataIndexedDBHelper::MockBrowsingDataIndexedDBHelper() {
}

MockBrowsingDataIndexedDBHelper::~MockBrowsingDataIndexedDBHelper() {
}

void MockBrowsingDataIndexedDBHelper::StartFetching(
    Callback1<const std::list<IndexedDBInfo>& >::Type* callback) {
  callback_.reset(callback);
}

void MockBrowsingDataIndexedDBHelper::CancelNotification() {
  callback_.reset(NULL);
}

void MockBrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const GURL& origin) {
  CHECK(origins_.find(origin) != origins_.end());
  origins_[origin] = false;
}

void MockBrowsingDataIndexedDBHelper::AddIndexedDBSamples() {
  const GURL kOrigin1("http://idbhost1:1/");
  const GURL kOrigin2("http://idbhost2:2/");
  response_.push_back(
      BrowsingDataIndexedDBHelper::IndexedDBInfo(
          kOrigin1, 1, base::Time()));
  origins_[kOrigin1] = true;
  response_.push_back(
      BrowsingDataIndexedDBHelper::IndexedDBInfo(
          kOrigin2, 2, base::Time()));
  origins_[kOrigin2] = true;
}

void MockBrowsingDataIndexedDBHelper::Notify() {
  CHECK(callback_.get());
  callback_->Run(response_);
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
