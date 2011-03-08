// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "app/sql/connection.h"
#include "base/format_macros.h"
#include "base/string_util.h"
#include "chrome/browser/history/history_unittest_base.h"

namespace history {

HistoryUnitTestBase::~HistoryUnitTestBase() {
}

void HistoryUnitTestBase::ExecuteSQLScript(const FilePath& sql_path,
                                           const FilePath& db_path) {
  std::string sql;
  ASSERT_TRUE(file_util::ReadFileToString(sql_path, &sql));

  // Replace the 'last_visit_time', 'visit_time', 'time_slot' values in this
  // SQL with the current time.
  int64 now = base::Time::Now().ToInternalValue();
  std::vector<std::string> sql_time;
  sql_time.push_back(StringPrintf("%" PRId64, now));  // last_visit_time
  sql_time.push_back(StringPrintf("%" PRId64, now));  // visit_time
  sql_time.push_back(StringPrintf("%" PRId64, now));  // time_slot
  sql = ReplaceStringPlaceholders(sql, sql_time, NULL);

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(db_path));
  ASSERT_TRUE(connection.Execute(sql.c_str()));
}

HistoryUnitTestBase::HistoryUnitTestBase() {
}

}  // namespace history
