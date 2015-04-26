// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/statistics_table.h"

#include "sql/connection.h"
#include "sql/statement.h"

namespace password_manager {
namespace {

// Convenience enum for interacting with SQL queries that use all the columns.
enum LoginTableColumns {
  COLUMN_ORIGIN_DOMAIN = 0,
  COLUMN_NOPES,
  COLUMN_DISMISSALS,
  COLUMN_DATE,
};

}  // namespace

StatisticsTable::StatisticsTable() : db_(nullptr) {
}

StatisticsTable::~StatisticsTable() {
}

bool StatisticsTable::Init(sql::Connection* db) {
  db_ = db;
  if (!db_->DoesTableExist("stats")) {
    const char query[] =
        "CREATE TABLE stats ("
        "origin_domain VARCHAR NOT NULL PRIMARY KEY, "
        "nopes_count INTEGER, "
        "dismissal_count INTEGER, "
        "start_date INTEGER NOT NULL)";
    if (!db_->Execute(query))
      return false;
  }
  return true;
}

bool StatisticsTable::AddRow(const InteractionsStats& stats) {
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO stats "
      "(origin_domain, nopes_count, dismissal_count, start_date) "
      "VALUES (?, ?, ?, ?)"));
  s.BindString(COLUMN_ORIGIN_DOMAIN, stats.origin_domain.spec());
  s.BindInt(COLUMN_NOPES, stats.nopes_count);
  s.BindInt(COLUMN_DISMISSALS, stats.dismissal_count);
  s.BindInt64(COLUMN_DATE, stats.start_date.ToInternalValue());
  return s.Run();
}

bool StatisticsTable::RemoveRow(const GURL& domain) {
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE,
                                           "DELETE FROM stats WHERE "
                                           "origin_domain = ? "));
  s.BindString(0, domain.spec());
  return s.Run();
}

scoped_ptr<InteractionsStats> StatisticsTable::GetRow(const GURL& domain) {
  const char query[] =
      "SELECT origin_domain, nopes_count, "
      "dismissal_count, start_date FROM stats WHERE origin_domain == ?";
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE, query));
  s.BindString(0, domain.spec());
  if (s.Step()) {
    scoped_ptr<InteractionsStats> stats(new InteractionsStats);
    stats->origin_domain = GURL(s.ColumnString(COLUMN_ORIGIN_DOMAIN));
    stats->nopes_count = s.ColumnInt(COLUMN_NOPES);
    stats->dismissal_count = s.ColumnInt(COLUMN_DISMISSALS);
    stats->start_date =
        base::Time::FromInternalValue(s.ColumnInt64(COLUMN_DATE));
    return stats.Pass();
  }
  return scoped_ptr<InteractionsStats>();
}

}  // namespace password_manager
