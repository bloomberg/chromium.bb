// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mock_browsing_data_database_helper.h"

#include "base/callback.h"

MockBrowsingDataDatabaseHelper::MockBrowsingDataDatabaseHelper(
    Profile* profile)
    : BrowsingDataDatabaseHelper(profile),
      profile_(profile) {
}

MockBrowsingDataDatabaseHelper::~MockBrowsingDataDatabaseHelper() {
}

void MockBrowsingDataDatabaseHelper::StartFetching(
    Callback1<const std::vector<DatabaseInfo>& >::Type* callback) {
  callback_.reset(callback);
}

void MockBrowsingDataDatabaseHelper::CancelNotification() {
  callback_.reset(NULL);
}

void MockBrowsingDataDatabaseHelper::DeleteDatabase(
    const std::string& origin,
    const std::string& name) {
  std::string key = origin + ":" + name;
  CHECK(databases_.find(key) != databases_.end());
  last_deleted_origin_ = origin;
  last_deleted_db_ = name;
  databases_[key] = false;
}

void MockBrowsingDataDatabaseHelper::AddDatabaseSamples() {
  response_.push_back(BrowsingDataDatabaseHelper::DatabaseInfo(
      "gdbhost1", "db1", "http_gdbhost1_1", "description 1",
      "http://gdbhost1:1/", 1, base::Time()));
  databases_["http_gdbhost1_1:db1"] = true;
  response_.push_back(BrowsingDataDatabaseHelper::DatabaseInfo(
      "gdbhost2", "db2", "http_gdbhost2_2", "description 2",
      "http://gdbhost2:2/", 2, base::Time()));
  databases_["http_gdbhost2_2:db2"] = true;
}

void MockBrowsingDataDatabaseHelper::Notify() {
  CHECK(callback_.get());
  callback_->Run(response_);
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
