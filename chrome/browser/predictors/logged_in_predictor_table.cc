// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/logged_in_predictor_table.h"

#include <algorithm>
#include <utility>
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sql/statement.h"

using content::BrowserThread;
using sql::Statement;
using std::string;

namespace {

const char kTableName[] = "logged_in_predictor";

}  // namespace

namespace predictors {

LoggedInPredictorTable::LoggedInPredictorTable()
    : PredictorTableBase() {
}

LoggedInPredictorTable::~LoggedInPredictorTable() {
}

// static
string LoggedInPredictorTable::GetKey(const GURL& url) {
  return GetKeyFromDomain(url.host());
}

// static
string LoggedInPredictorTable::GetKeyFromDomain(const std::string& domain) {
  string effective_domain(
      net::registry_controlled_domains::GetDomainAndRegistry(
          domain,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES));
  if (effective_domain.empty())
    effective_domain = domain;

  // Strip off a preceding ".", if present.
  if (!effective_domain.empty() && effective_domain[0] == '.')
    return effective_domain.substr(1);
  return effective_domain;
}

void LoggedInPredictorTable::AddDomainFromURL(const GURL& url) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  Statement statement(DB()->GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("INSERT OR IGNORE INTO %s (domain, time) VALUES (?,?)",
                         kTableName).c_str()));

  statement.BindString(0, GetKey(url));
  statement.BindInt64(1, base::Time::Now().ToInternalValue());

  statement.Run();
}

void LoggedInPredictorTable::DeleteDomainFromURL(const GURL& url) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  Statement statement(DB()->GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE domain=?", kTableName).c_str()));

  statement.BindString(0, GetKey(url));

  statement.Run();
}

void LoggedInPredictorTable::DeleteDomain(const std::string& domain) {
  DeleteDomainFromURL(GURL("http://" + domain));
}

void LoggedInPredictorTable::HasUserLoggedIn(const GURL& url, bool* is_present,
                                             bool* lookup_succeeded) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  *lookup_succeeded = false;
  if (CantAccessDatabase())
    return;

  Statement statement(DB()->GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("SELECT count(*) FROM %s WHERE domain=?",
                         kTableName).c_str()));

  statement.BindString(0, GetKey(url));

  if (statement.Step()) {
    *is_present = (statement.ColumnInt(0) > 0);
    *lookup_succeeded = true;
  }
}

void LoggedInPredictorTable::DeleteAllCreatedBetween(
    const base::Time& delete_begin, const base::Time& delete_end) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  Statement statement(DB()->GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE time >= ? AND time <= ?",
                         kTableName).c_str()));

  statement.BindInt64(0, delete_begin.ToInternalValue());
  statement.BindInt64(1, delete_end.ToInternalValue());

  statement.Run();
}

void LoggedInPredictorTable::GetAllData(
    LoggedInPredictorTable::LoggedInStateMap* state_map) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(state_map != NULL);
  state_map->clear();
  if (CantAccessDatabase())
    return;

  Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT * FROM %s", kTableName).c_str()));

  while (statement.Step()) {
    string domain = statement.ColumnString(0);
    int64 value = statement.ColumnInt64(1);
    (*state_map)[domain] = value;
  }
}

void LoggedInPredictorTable::CreateTableIfNonExistent() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  sql::Connection* db = DB();
  if (db->DoesTableExist(kTableName))
    return;

  const char* table_creator =
      "CREATE TABLE %s (domain TEXT, time INTEGER, PRIMARY KEY(domain))";

  if (!db->Execute(base::StringPrintf(table_creator, kTableName).c_str()))
    ResetDB();
}

void LoggedInPredictorTable::LogDatabaseStats()  {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  Statement statement(DB()->GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("SELECT count(*) FROM %s", kTableName).c_str()));
  if (statement.Step())
    UMA_HISTOGRAM_COUNTS("LoggedInPredictor.TableRowCount",
                         statement.ColumnInt(0));
}

}  // namespace predictors
