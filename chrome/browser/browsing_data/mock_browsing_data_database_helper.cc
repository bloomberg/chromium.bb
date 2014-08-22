// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_database_helper.h"

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataDatabaseHelper::MockBrowsingDataDatabaseHelper(
    Profile* profile)
    : BrowsingDataDatabaseHelper(profile) {
}

MockBrowsingDataDatabaseHelper::~MockBrowsingDataDatabaseHelper() {
}

void MockBrowsingDataDatabaseHelper::StartFetching(
    const base::Callback<void(const std::list<DatabaseInfo>&)>& callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = callback;
}

void MockBrowsingDataDatabaseHelper::DeleteDatabase(
    const std::string& origin,
    const std::string& name) {
  ASSERT_FALSE(callback_.is_null());
  std::string key = origin + ":" + name;
  ASSERT_TRUE(databases_.find(key) != databases_.end());
  last_deleted_origin_ = origin;
  last_deleted_db_ = name;
  databases_[key] = false;
}

void MockBrowsingDataDatabaseHelper::AddDatabaseSamples() {
  storage::DatabaseIdentifier id1 =
      storage::DatabaseIdentifier::Parse("http_gdbhost1_1");
  response_.push_back(BrowsingDataDatabaseHelper::DatabaseInfo(
      id1, "db1", "description 1", 1, base::Time()));
  databases_["http_gdbhost1_1:db1"] = true;
  storage::DatabaseIdentifier id2 =
      storage::DatabaseIdentifier::Parse("http_gdbhost2_2");
  response_.push_back(BrowsingDataDatabaseHelper::DatabaseInfo(
      id2, "db2", "description 2", 2, base::Time()));
  databases_["http_gdbhost2_2:db2"] = true;
}

void MockBrowsingDataDatabaseHelper::Notify() {
  callback_.Run(response_);
}

void MockBrowsingDataDatabaseHelper::Reset() {
  for (std::map<const std::string, bool>::iterator i = databases_.begin();
       i != databases_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataDatabaseHelper::AllDeleted() {
  for (std::map<const std::string, bool>::const_iterator i = databases_.begin();
       i != databases_.end(); ++i)
    if (i->second)
      return false;
  return true;
}
