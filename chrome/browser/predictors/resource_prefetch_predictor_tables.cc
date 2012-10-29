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

const char kUrlResourceTableName[] = "resource_prefetch_predictor_url";
const char kUrlMetadataTableName[] = "resource_prefetch_predictor_url_metadata";

// The maximum length allowed for strings in the database.
const size_t kMaxURLLength = 2048;

std::string MaybeTrimUrl(const GURL& url, bool is_main_frame_url) {
  std::string spec = url.spec();
  if (spec.length() > kMaxURLLength) {
    if (is_main_frame_url) {
      UMA_HISTOGRAM_BOOLEAN("ResourcePrefetchPredictor.MainFrameUrlTooLong",
                            true);
    } else {
      UMA_HISTOGRAM_BOOLEAN("ResourcePrefetchPredictor.ResourceUrlTooLong",
                            true);
    }
    return spec.substr(0, kMaxURLLength);
  }
  return spec;
}

void BindUrlResourceRowToStatement(
    const predictors::ResourcePrefetchPredictorTables::UrlResourceRow& row,
    sql::Statement* statement) {
  statement->BindString(0, MaybeTrimUrl(row.main_frame_url, true));
  statement->BindString(1, MaybeTrimUrl(row.resource_url, false));
  statement->BindInt(2, static_cast<int>(row.resource_type));
  statement->BindInt(3, row.number_of_hits);
  statement->BindInt(4, row.number_of_misses);
  statement->BindInt(5, row.consecutive_misses);
  statement->BindDouble(6, row.average_position);
}

bool StepAndInitializeUrlResourceRow(
    sql::Statement* statement,
    predictors::ResourcePrefetchPredictorTables::UrlResourceRow* row) {
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

ResourcePrefetchPredictorTables::UrlResourceRow::UrlResourceRow()
    : resource_type(ResourceType::LAST_TYPE),
      number_of_hits(0),
      number_of_misses(0),
      consecutive_misses(0),
      average_position(0.0),
      score(0.0) {
}

ResourcePrefetchPredictorTables::UrlResourceRow::UrlResourceRow(
    const UrlResourceRow& other)
        : main_frame_url(other.main_frame_url),
          resource_url(other.resource_url),
          resource_type(other.resource_type),
          number_of_hits(other.number_of_hits),
          number_of_misses(other.number_of_misses),
          consecutive_misses(other.consecutive_misses),
          average_position(other.average_position),
          score(other.score) {
}

ResourcePrefetchPredictorTables::UrlResourceRow::UrlResourceRow(
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

void ResourcePrefetchPredictorTables::UrlResourceRow::UpdateScore() {
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

bool ResourcePrefetchPredictorTables::UrlResourceRow::operator==(
    const UrlResourceRow& rhs) const {
  return main_frame_url == rhs.main_frame_url &&
      resource_url == rhs.resource_url &&
      resource_type == rhs.resource_type &&
      number_of_hits == rhs.number_of_hits &&
      number_of_misses == rhs.number_of_misses &&
      consecutive_misses == rhs.consecutive_misses &&
      average_position == rhs.average_position &&
      score == rhs.score;
}

bool ResourcePrefetchPredictorTables::UrlResourceRowSorter::operator()(
    const UrlResourceRow& x, const UrlResourceRow& y) const {
  return x.score > y.score;
}

ResourcePrefetchPredictorTables::UrlData::UrlData(const GURL& i_main_frame_url)
    : main_frame_url(i_main_frame_url) {
}

ResourcePrefetchPredictorTables::UrlData::UrlData(const UrlData& other)
    : main_frame_url(other.main_frame_url),
      last_visit(other.last_visit),
      resources(other.resources) {
}

ResourcePrefetchPredictorTables::UrlData::~UrlData() {
}

bool ResourcePrefetchPredictorTables::UrlData::operator==(
    const UrlData& rhs) const {
  return main_frame_url == rhs.main_frame_url &&
      resources == rhs.resources;
}

ResourcePrefetchPredictorTables::ResourcePrefetchPredictorTables()
    : PredictorTableBase() {
}

ResourcePrefetchPredictorTables::~ResourcePrefetchPredictorTables() {
}

void ResourcePrefetchPredictorTables::GetAllUrlData(
    std::vector<UrlData>* url_data_buffer) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  CHECK(url_data_buffer && url_data_buffer->empty());

  // First read the resources table and organize it per main_frame_url.
  sql::Statement resource_reader(DB()->GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("SELECT * FROM %s", kUrlResourceTableName).c_str()));
  std::map<GURL, size_t> url_index;
  UrlResourceRow url_row;
  while (StepAndInitializeUrlResourceRow(&resource_reader, &url_row)) {
    url_row.UpdateScore();

    if (url_index.find(url_row.main_frame_url) == url_index.end()) {
      url_index[url_row.main_frame_url] = url_data_buffer->size();
      url_data_buffer->push_back(UrlData(url_row.main_frame_url));
    }
    url_data_buffer->at(url_index[url_row.main_frame_url]).resources.push_back(
        url_row);
  }

  // Read the metadata and keep track of Urls that have metadata, but no
  // resource entries, so they can be deleted.
  std::vector<GURL> urls_to_delete;

  sql::Statement metadata_reader(DB()->GetCachedStatement(SQL_FROM_HERE,
      base::StringPrintf("SELECT * FROM %s", kUrlMetadataTableName).c_str()));
  while (metadata_reader.Step()) {
    GURL main_frame_url = GURL(metadata_reader.ColumnString(0));
    if (url_index.find(main_frame_url) != url_index.end()) {
      int64 last_visit = metadata_reader.ColumnInt64(1);
      url_data_buffer->at(url_index[main_frame_url]).last_visit =
          base::Time::FromInternalValue(last_visit);
    } else {
      urls_to_delete.push_back(main_frame_url);
    }
  }

  // Delete rows in the UrlMetadataTable for which there are no resources.
  if (!urls_to_delete.empty())
    DeleteDataForUrls(urls_to_delete);
}

void ResourcePrefetchPredictorTables::UpdateDataForUrl(
    const UrlData& url_data) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  // Sanitize the url.
  const std::string main_frame_url = MaybeTrimUrl(url_data.main_frame_url,
                                                  true);

  DB()->BeginTransaction();

  // Delete the older data from both the tables.
  sql::Statement resource_deleter(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                         kUrlResourceTableName).c_str()));
  resource_deleter.BindString(0, main_frame_url);
  sql::Statement url_deleter(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                         kUrlMetadataTableName).c_str()));
  url_deleter.BindString(0, main_frame_url);
  if (!resource_deleter.Run() || !url_deleter.Run()) {
    DB()->RollbackTransaction();
    return;
  }

  // Add the new data to the tables.
  const UrlResourceRows& resources = url_data.resources;
  for (UrlResourceRows::const_iterator it = resources.begin();
       it != resources.end(); ++it) {
    sql::Statement resource_inserter(
        DB()->GetCachedStatement(
            SQL_FROM_HERE,
            base::StringPrintf(
                "INSERT INTO %s "
                "(main_page_url, resource_url, resource_type, number_of_hits, "
                "number_of_misses, consecutive_misses, average_position) "
                "VALUES (?,?,?,?,?,?,?)",
                kUrlResourceTableName).c_str()));
    BindUrlResourceRowToStatement(*it, &resource_inserter);
    if (!resource_inserter.Run()) {
      DB()->RollbackTransaction();
      return;
    }
  }

  sql::Statement metadata_inserter(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("INSERT INTO %s (main_page_url, last_visit_time) "
                         "VALUES (?,?)", kUrlMetadataTableName).c_str()));
  metadata_inserter.BindString(0,main_frame_url);
  metadata_inserter.BindInt64(1, url_data.last_visit.ToInternalValue());
  if (!metadata_inserter.Run()) {
    DB()->RollbackTransaction();
    return;
  }

  DB()->CommitTransaction();
}

void ResourcePrefetchPredictorTables::DeleteDataForUrls(
    const std::vector<GURL>& urls) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  for (std::vector<GURL>::const_iterator it = urls.begin(); it != urls.end();
       ++it) {
    sql::Statement resource_deleter(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                           kUrlResourceTableName).c_str()));
    resource_deleter.BindString(0, it->spec());
    resource_deleter.Run();

    sql::Statement url_deleter(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                           kUrlMetadataTableName).c_str()));
    url_deleter.BindString(0, it->spec());
    url_deleter.Run();
  }
}

void ResourcePrefetchPredictorTables::DeleteAllUrlData() {
  if (CantAccessDatabase())
    return;

  sql::Statement resource_deleter(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s", kUrlResourceTableName).c_str()));
  resource_deleter.Run();

  sql::Statement url_deleter(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s", kUrlMetadataTableName).c_str()));
  url_deleter.Run();
}

void ResourcePrefetchPredictorTables::CreateTableIfNonExistent() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  std::string url_resource_table_creation_statement = base::StringPrintf(
      "CREATE TABLE %s ( "
      "main_page_url TEXT, "
      "resource_url TEXT, "
      "resource_type INTEGER, "
      "number_of_hits INTEGER, "
      "number_of_misses INTEGER, "
      "consecutive_misses INTEGER, "
      "average_position DOUBLE, "
      "PRIMARY KEY(main_page_url, resource_url))",
      kUrlResourceTableName);

  std::string url_metadata_table_creation_statement = base::StringPrintf(
      "CREATE TABLE %s ( "
      "main_page_url TEXT, "
      "last_visit_time INTEGER, "
      "PRIMARY KEY(main_page_url))",
      kUrlMetadataTableName);

  if ((!DB()->DoesTableExist(kUrlResourceTableName) &&
       !DB()->Execute(url_resource_table_creation_statement.c_str())) ||
      (!DB()->DoesTableExist(kUrlMetadataTableName) &&
       !DB()->Execute(url_metadata_table_creation_statement.c_str()))) {
    ResetDB();
  }
}

void ResourcePrefetchPredictorTables::LogDatabaseStats()  {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (CantAccessDatabase())
    return;

  sql::Statement url_statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT count(*) FROM %s",
                         kUrlResourceTableName).c_str()));
  if (url_statement.Step())
    UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.UrlTableRowCount",
                         url_statement.ColumnInt(0));
}

}  // namespace predictors
