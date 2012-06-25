// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "sql/statement.h"

using content::BrowserThread;

namespace {

const char kResourcePredictorUrlTableName[] = "resource_prefetch_predictor_url";

// The maximum length allowed for strings in the database.
const size_t kMaxURLLength = 2048;

void BindUrlTableRowToStatement(
    const predictors::ResourcePrefetchPredictorTables::UrlTableRow& row,
    sql::Statement* statement) {
  statement->BindString(0, row.main_frame_url.spec().substr(0, kMaxURLLength));
  statement->BindString(1, row.resource_url.spec().substr(0, kMaxURLLength));
  statement->BindInt(2, static_cast<int>(row.resource_type));
  statement->BindInt(3, row.number_of_hits);
  statement->BindInt(4, row.number_of_misses);
  statement->BindInt(5, row.consecutive_misses);
  statement->BindDouble(6, row.average_position);
}

bool StepAndInitializeUrlTableRow(
    sql::Statement* statement,
    predictors::ResourcePrefetchPredictorTables::UrlTableRow* row) {
  if (!statement->Step())
    return false;

  row->main_frame_url = GURL(statement->ColumnString(0));
  row->resource_url = GURL(statement->ColumnString(1));
  row->resource_type = ResourceType::FromInt(statement->ColumnInt(2));
  row->number_of_hits = statement->ColumnInt(3);
  row->number_of_misses = statement->ColumnInt(4);
  row->consecutive_misses = statement->ColumnInt(5);
  row->average_position = statement->ColumnDouble(6);
  return true;
}

}  // namespace

namespace predictors {

ResourcePrefetchPredictorTables::UrlTableRow::UrlTableRow()
    : resource_type(ResourceType::LAST_TYPE),
      number_of_hits(0),
      number_of_misses(0),
      consecutive_misses(0),
      average_position(0.0),
      score(0.0) {
}

ResourcePrefetchPredictorTables::UrlTableRow::UrlTableRow(
    const UrlTableRow& other)
        : main_frame_url(other.main_frame_url),
          resource_url(other.resource_url),
          resource_type(other.resource_type),
          number_of_hits(other.number_of_hits),
          number_of_misses(other.number_of_misses),
          consecutive_misses(other.consecutive_misses),
          average_position(other.average_position),
          score(other.score) {
}

ResourcePrefetchPredictorTables::UrlTableRow::UrlTableRow(
    const std::string& i_main_frame_url,
    const std::string& i_resource_url,
    ResourceType::Type i_resource_type,
    int i_number_of_hits,
    int i_number_of_misses,
    int i_consecutive_misses,
    double i_average_position)
        : main_frame_url(i_main_frame_url),
          resource_url(i_resource_url),
          resource_type(i_resource_type),
          number_of_hits(i_number_of_hits),
          number_of_misses(i_number_of_misses),
          consecutive_misses(i_consecutive_misses),
          average_position(i_average_position) {
  UpdateScore();
}

void ResourcePrefetchPredictorTables::UrlTableRow::UpdateScore() {
  // The score is calculated so that when the rows are sorted, the stylesheets
  // and scripts appear first, sorted by position(ascending) and then the rest
  // of the resources sorted by position(ascending).
  static const int kMaxResourcesPerType = 100;
  switch (resource_type) {
    case ResourceType::STYLESHEET:
    case ResourceType::SCRIPT:
      score = (2 * kMaxResourcesPerType) - average_position;
      break;

    case ResourceType::IMAGE:
      score = kMaxResourcesPerType - average_position;
      break;

    default:
      score = kMaxResourcesPerType - average_position;
      break;
  }
}

bool ResourcePrefetchPredictorTables::UrlTableRow::operator==(
    const UrlTableRow& rhs) const {
  return main_frame_url == rhs.main_frame_url &&
      resource_url == rhs.resource_url &&
      resource_type == rhs.resource_type &&
      number_of_hits == rhs.number_of_hits &&
      number_of_misses == rhs.number_of_misses &&
      consecutive_misses == rhs.consecutive_misses &&
      average_position == rhs.average_position &&
      score == rhs.score;
}

bool ResourcePrefetchPredictorTables::UrlTableRowSorter::operator()(
    const UrlTableRow& x,
    const UrlTableRow& y) const {
  return x.score > y.score;
}

ResourcePrefetchPredictorTables::ResourcePrefetchPredictorTables()
    : PredictorTableBase() {
}

ResourcePrefetchPredictorTables::~ResourcePrefetchPredictorTables() {
}

void ResourcePrefetchPredictorTables::GetAllRows(UrlTableRows* url_row_buffer) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  CHECK(url_row_buffer && url_row_buffer->empty());
  sql::Statement url_statement(DB()->GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("SELECT * FROM %s",
                         kResourcePredictorUrlTableName).c_str()));

  UrlTableRow url_row;
  while (StepAndInitializeUrlTableRow(&url_statement, &url_row)) {
    url_row.UpdateScore();
    url_row_buffer->push_back(url_row);
  }
}

void ResourcePrefetchPredictorTables::UpdateRowsForUrl(
    const GURL& main_page_url,
    const UrlTableRows& row_buffer) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  DB()->BeginTransaction();

  sql::Statement delete_statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                         kResourcePredictorUrlTableName).c_str()));
  delete_statement.BindString(0, main_page_url.spec());
  if (!delete_statement.Run()) {
    DB()->RollbackTransaction();
    return;
  }

  for (UrlTableRows::const_iterator it = row_buffer.begin();
       it != row_buffer.end(); ++it) {
    sql::Statement add_statement(
        DB()->GetCachedStatement(
            SQL_FROM_HERE,
            base::StringPrintf(
                "INSERT INTO %s "
                "(main_page_url, resource_url, resource_type, number_of_hits, "
                "number_of_misses, consecutive_misses, average_position) "
                "VALUES (?,?,?,?,?,?,?)",
                kResourcePredictorUrlTableName).c_str()));
    BindUrlTableRowToStatement(*it, &add_statement);
    if (!add_statement.Run()) {
      DB()->RollbackTransaction();
      return;
    }
  }

  DB()->CommitTransaction();
}

void ResourcePrefetchPredictorTables::DeleteRowsForUrls(
    const std::vector<GURL>& urls) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  // These do not need to be a transaction.
  for (std::vector<GURL>::const_iterator it = urls.begin(); it != urls.end();
       ++it) {
    sql::Statement delete_statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                           kResourcePredictorUrlTableName).c_str()));
    delete_statement.BindString(0, it->spec());
    delete_statement.Run();
  }
}

void ResourcePrefetchPredictorTables::DeleteAllRows() {
  if (CantAccessDatabase())
    return;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s",
                         kResourcePredictorUrlTableName).c_str()));
  statement.Run();
}

void ResourcePrefetchPredictorTables::CreateTableIfNonExistent() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  std::string url_table_creation_statement = base::StringPrintf(
      "CREATE TABLE %s ( "
      "main_page_url TEXT, "
      "resource_url TEXT, "
      "resource_type INTEGER, "
      "number_of_hits INTEGER, "
      "number_of_misses INTEGER, "
      "consecutive_misses INTEGER, "
      "average_position DOUBLE, "
      "PRIMARY KEY(main_page_url, resource_url))",
      kResourcePredictorUrlTableName);

  if (!DB()->DoesTableExist(kResourcePredictorUrlTableName) &&
      !DB()->Execute(url_table_creation_statement.c_str()))
    ResetDB();
}

void ResourcePrefetchPredictorTables::LogDatabaseStats()  {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  sql::Statement url_statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT count(*) FROM %s",
                         kResourcePredictorUrlTableName).c_str()));
  if (url_statement.Step())
    UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.UrlTableRowCount",
                         url_statement.ColumnInt(0));
}

}  // namespace predictors
