// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_log_policy.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

ActivityLogPolicy::ActivityLogPolicy(Profile* profile) : testing_clock_(NULL) {}

ActivityLogPolicy::~ActivityLogPolicy() {}

base::Time ActivityLogPolicy::Now() const {
  if (testing_clock_)
    return testing_clock_->Now();
  else
    return base::Time::Now();
}

std::string ActivityLogPolicy::GetKey(KeyType) const {
  return std::string();
}

ActivityLogDatabasePolicy::ActivityLogDatabasePolicy(
    Profile* profile,
    const base::FilePath& database_name)
    : ActivityLogPolicy(profile) {
  CHECK(profile);
  base::FilePath profile_base_path = profile->GetPath();
  db_ = new ActivityDatabase(this);
  base::FilePath database_path = profile_base_path.Append(database_name);
  ScheduleAndForget(db_, &ActivityDatabase::Init, database_path);
}

sql::Connection* ActivityLogDatabasePolicy::GetDatabaseConnection() const {
  return db_->GetSqlConnection();
}

}  // namespace extensions
