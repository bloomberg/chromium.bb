// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "sql/statement.h"

using content::BrowserThread;
using sql::Statement;

namespace {

const char kUrlResourceTableName[] = "resource_prefetch_predictor_url";
const char kUrlMetadataTableName[] = "resource_prefetch_predictor_url_metadata";
const char kHostResourceTableName[] = "resource_prefetch_predictor_host";
const char kHostMetadataTableName[] =
    "resource_prefetch_predictor_host_metadata";

void BindResourceRowToStatement(
    const predictors::ResourcePrefetchPredictorTables::ResourceRow& row,
    const std::string& primary_key,
    Statement* statement) {
  statement->BindString(0, primary_key);
  statement->BindString(1, row.resource_url.spec());
  statement->BindInt(2, static_cast<int>(row.resource_type));
  statement->BindInt(3, row.number_of_hits);
  statement->BindInt(4, row.number_of_misses);
  statement->BindInt(5, row.consecutive_misses);
  statement->BindDouble(6, row.average_position);
}

bool StepAndInitializeResourceRow(
    Statement* statement,
    predictors::ResourcePrefetchPredictorTables::ResourceRow* row) {
  if (!statement->Step())
    return false;

  row->primary_key = statement->ColumnString(0);
  row->resource_url = GURL(statement->ColumnString(1));
  row->resource_type = static_cast<content::ResourceType>(
      statement->ColumnInt(2));
  row->number_of_hits = statement->ColumnInt(3);
  row->number_of_misses = statement->ColumnInt(4);
  row->consecutive_misses = statement->ColumnInt(5);
  row->average_position = statement->ColumnDouble(6);
  return true;
}

}  // namespace

namespace predictors {

// static
const size_t ResourcePrefetchPredictorTables::kMaxStringLength = 1024;

ResourcePrefetchPredictorTables::ResourceRow::ResourceRow()
    : resource_type(content::RESOURCE_TYPE_LAST_TYPE),
      number_of_hits(0),
      number_of_misses(0),
      consecutive_misses(0),
      average_position(0.0),
      score(0.0) {
}

ResourcePrefetchPredictorTables::ResourceRow::ResourceRow(
    const ResourceRow& other)
        : primary_key(other.primary_key),
          resource_url(other.resource_url),
          resource_type(other.resource_type),
          number_of_hits(other.number_of_hits),
          number_of_misses(other.number_of_misses),
          consecutive_misses(other.consecutive_misses),
          average_position(other.average_position),
          score(other.score) {
}

ResourcePrefetchPredictorTables::ResourceRow::ResourceRow(
    const std::string& i_primary_key,
    const std::string& i_resource_url,
    content::ResourceType i_resource_type,
    int i_number_of_hits,
    int i_number_of_misses,
    int i_consecutive_misses,
    double i_average_position)
        : primary_key(i_primary_key),
          resource_url(i_resource_url),
          resource_type(i_resource_type),
          number_of_hits(i_number_of_hits),
          number_of_misses(i_number_of_misses),
          consecutive_misses(i_consecutive_misses),
          average_position(i_average_position) {
  UpdateScore();
}

void ResourcePrefetchPredictorTables::ResourceRow::UpdateScore() {
  // The score is calculated so that when the rows are sorted, the stylesheets
  // and scripts appear first, sorted by position(ascending) and then the rest
  // of the resources sorted by position(ascending).
  static const int kMaxResourcesPerType = 100;
  switch (resource_type) {
    case content::RESOURCE_TYPE_STYLESHEET:
    case content::RESOURCE_TYPE_SCRIPT:
      score = (2 * kMaxResourcesPerType) - average_position;
      break;

    case content::RESOURCE_TYPE_IMAGE:
    default:
      score = kMaxResourcesPerType - average_position;
      break;
  }
}

bool ResourcePrefetchPredictorTables::ResourceRow::operator==(
    const ResourceRow& rhs) const {
  return primary_key == rhs.primary_key &&
      resource_url == rhs.resource_url &&
      resource_type == rhs.resource_type &&
      number_of_hits == rhs.number_of_hits &&
      number_of_misses == rhs.number_of_misses &&
      consecutive_misses == rhs.consecutive_misses &&
      average_position == rhs.average_position &&
      score == rhs.score;
}

bool ResourcePrefetchPredictorTables::ResourceRowSorter::operator()(
    const ResourceRow& x, const ResourceRow& y) const {
  return x.score > y.score;
}

ResourcePrefetchPredictorTables::PrefetchData::PrefetchData(
    PrefetchKeyType i_key_type,
    const std::string& i_primary_key)
    : key_type(i_key_type),
      primary_key(i_primary_key) {
}

ResourcePrefetchPredictorTables::PrefetchData::PrefetchData(
    const PrefetchData& other)
    : key_type(other.key_type),
      primary_key(other.primary_key),
      last_visit(other.last_visit),
      resources(other.resources) {
}

ResourcePrefetchPredictorTables::PrefetchData::~PrefetchData() {
}

bool ResourcePrefetchPredictorTables::PrefetchData::operator==(
    const PrefetchData& rhs) const {
  return key_type == rhs.key_type && primary_key == rhs.primary_key &&
      resources == rhs.resources;
}

void ResourcePrefetchPredictorTables::GetAllData(
    PrefetchDataMap* url_data_map,
    PrefetchDataMap* host_data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DCHECK(url_data_map);
  DCHECK(host_data_map);
  url_data_map->clear();
  host_data_map->clear();

  std::vector<std::string> urls_to_delete, hosts_to_delete;
  GetAllDataHelper(PREFETCH_KEY_TYPE_URL, url_data_map, &urls_to_delete);
  GetAllDataHelper(PREFETCH_KEY_TYPE_HOST, host_data_map, &hosts_to_delete);

  if (!urls_to_delete.empty() || !hosts_to_delete.empty())
    DeleteData(urls_to_delete, hosts_to_delete);
}

void ResourcePrefetchPredictorTables::UpdateData(
    const PrefetchData& url_data,
    const PrefetchData& host_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DCHECK(!url_data.is_host() && host_data.is_host());
  DCHECK(!url_data.primary_key.empty() || !host_data.primary_key.empty());

  DB()->BeginTransaction();

  bool success = (url_data.primary_key.empty() || UpdateDataHelper(url_data)) &&
      (host_data.primary_key.empty() || UpdateDataHelper(host_data));
  if (!success)
    DB()->RollbackTransaction();

  DB()->CommitTransaction();
}

void ResourcePrefetchPredictorTables::DeleteData(
    const std::vector<std::string>& urls,
    const std::vector<std::string>& hosts) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DCHECK(!urls.empty() || !hosts.empty());

  if (!urls.empty())
    DeleteDataHelper(PREFETCH_KEY_TYPE_URL, urls);
  if (!hosts.empty())
    DeleteDataHelper(PREFETCH_KEY_TYPE_HOST, hosts);
}

void ResourcePrefetchPredictorTables::DeleteSingleDataPoint(
    const std::string& key,
    PrefetchKeyType key_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DeleteDataHelper(key_type, std::vector<std::string>(1, key));
}

void ResourcePrefetchPredictorTables::DeleteAllData() {
  if (CantAccessDatabase())
    return;

  Statement deleter(DB()->GetUniqueStatement(
      base::StringPrintf("DELETE FROM %s", kUrlResourceTableName).c_str()));
  deleter.Run();
  deleter.Assign(DB()->GetUniqueStatement(
      base::StringPrintf("DELETE FROM %s", kUrlMetadataTableName).c_str()));
  deleter.Run();
  deleter.Assign(DB()->GetUniqueStatement(
      base::StringPrintf("DELETE FROM %s", kHostResourceTableName).c_str()));
  deleter.Run();
  deleter.Assign(DB()->GetUniqueStatement(
      base::StringPrintf("DELETE FROM %s", kHostMetadataTableName).c_str()));
  deleter.Run();
}

ResourcePrefetchPredictorTables::ResourcePrefetchPredictorTables()
    : PredictorTableBase() {
}

ResourcePrefetchPredictorTables::~ResourcePrefetchPredictorTables() {
}

void ResourcePrefetchPredictorTables::GetAllDataHelper(
    PrefetchKeyType key_type,
    PrefetchDataMap* data_map,
    std::vector<std::string>* to_delete) {
  bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;

  // Read the resources table and organize it per primary key.
  const char* resource_table_name = is_host ? kHostResourceTableName :
      kUrlResourceTableName;
  Statement resource_reader(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT * FROM %s", resource_table_name).c_str()));

  ResourceRow row;
  while (StepAndInitializeResourceRow(&resource_reader, &row)) {
    row.UpdateScore();
    std::string primary_key = row.primary_key;
    // Don't need to store primary key since the data is grouped by primary key.
    row.primary_key.clear();

    PrefetchDataMap::iterator it = data_map->find(primary_key);
    if (it == data_map->end()) {
      it = data_map->insert(std::make_pair(
          primary_key, PrefetchData(key_type, primary_key))).first;
    }
    it->second.resources.push_back(row);
  }

  // Sort each of the resource row vectors by score.
  for (PrefetchDataMap::iterator it = data_map->begin(); it != data_map->end();
       ++it) {
    std::sort(it->second.resources.begin(),
              it->second.resources.end(),
              ResourceRowSorter());
  }

  // Read the metadata and keep track of entries that have metadata, but no
  // resource entries, so they can be deleted.
  const char* metadata_table_name = is_host ? kHostMetadataTableName :
      kUrlMetadataTableName;
  Statement metadata_reader(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT * FROM %s", metadata_table_name).c_str()));

  while (metadata_reader.Step()) {
    std::string primary_key = metadata_reader.ColumnString(0);

    PrefetchDataMap::iterator it = data_map->find(primary_key);
    if (it != data_map->end()) {
      int64 last_visit = metadata_reader.ColumnInt64(1);
      it->second.last_visit = base::Time::FromInternalValue(last_visit);
    } else {
      to_delete->push_back(primary_key);
    }
  }
}

bool ResourcePrefetchPredictorTables::UpdateDataHelper(
    const PrefetchData& data) {
  DCHECK(!data.primary_key.empty());

  if (!StringsAreSmallerThanDBLimit(data)) {
    UMA_HISTOGRAM_BOOLEAN("ResourcePrefetchPredictor.DbStringTooLong", true);
    return false;
  }

  // Delete the older data from both the tables.
  scoped_ptr<Statement> deleter(data.is_host() ?
      GetHostResourceDeleteStatement() : GetUrlResourceDeleteStatement());
  deleter->BindString(0, data.primary_key);
  if (!deleter->Run())
    return false;

  deleter.reset(data.is_host() ? GetHostMetadataDeleteStatement() :
      GetUrlMetadataDeleteStatement());
  deleter->BindString(0, data.primary_key);
  if (!deleter->Run())
    return false;

  // Add the new data to the tables.
  const ResourceRows& resources = data.resources;
  for (ResourceRows::const_iterator it = resources.begin();
       it != resources.end(); ++it) {
    scoped_ptr<Statement> resource_inserter(data.is_host() ?
        GetHostResourceUpdateStatement() : GetUrlResourceUpdateStatement());
    BindResourceRowToStatement(*it, data.primary_key, resource_inserter.get());
    if (!resource_inserter->Run())
      return false;
  }

  scoped_ptr<Statement> metadata_inserter(data.is_host() ?
      GetHostMetadataUpdateStatement() : GetUrlMetadataUpdateStatement());
  metadata_inserter->BindString(0, data.primary_key);
  metadata_inserter->BindInt64(1, data.last_visit.ToInternalValue());
  if (!metadata_inserter->Run())
    return false;

  return true;
}

void ResourcePrefetchPredictorTables::DeleteDataHelper(
    PrefetchKeyType key_type,
    const std::vector<std::string>& keys) {
  bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;

  for (std::vector<std::string>::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    scoped_ptr<Statement> deleter(is_host ? GetHostResourceDeleteStatement() :
        GetUrlResourceDeleteStatement());
    deleter->BindString(0, *it);
    deleter->Run();

    deleter.reset(is_host ? GetHostMetadataDeleteStatement() :
        GetUrlMetadataDeleteStatement());
    deleter->BindString(0, *it);
    deleter->Run();
  }
}

bool ResourcePrefetchPredictorTables::StringsAreSmallerThanDBLimit(
    const PrefetchData& data) const {
  if (data.primary_key.length() > kMaxStringLength)
    return false;

  for (ResourceRows::const_iterator it = data.resources.begin();
       it != data.resources.end(); ++it) {
    if (it->resource_url.spec().length() > kMaxStringLength)
      return false;
  }
  return true;
}

void ResourcePrefetchPredictorTables::CreateTableIfNonExistent() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  const char resource_table_creator[] =
      "CREATE TABLE %s ( "
      "main_page_url TEXT, "
      "resource_url TEXT, "
      "resource_type INTEGER, "
      "number_of_hits INTEGER, "
      "number_of_misses INTEGER, "
      "consecutive_misses INTEGER, "
      "average_position DOUBLE, "
      "PRIMARY KEY(main_page_url, resource_url))";
  const char* metadata_table_creator =
      "CREATE TABLE %s ( "
      "main_page_url TEXT, "
      "last_visit_time INTEGER, "
      "PRIMARY KEY(main_page_url))";

  sql::Connection* db = DB();
  bool success =
      (db->DoesTableExist(kUrlResourceTableName) ||
       db->Execute(base::StringPrintf(resource_table_creator,
                                      kUrlResourceTableName).c_str())) &&
      (db->DoesTableExist(kUrlMetadataTableName) ||
       db->Execute(base::StringPrintf(metadata_table_creator,
                                      kUrlMetadataTableName).c_str())) &&
      (db->DoesTableExist(kHostResourceTableName) ||
       db->Execute(base::StringPrintf(resource_table_creator,
                                      kHostResourceTableName).c_str())) &&
      (db->DoesTableExist(kHostMetadataTableName) ||
       db->Execute(base::StringPrintf(metadata_table_creator,
                                      kHostMetadataTableName).c_str()));

  if (!success)
    ResetDB();
}

void ResourcePrefetchPredictorTables::LogDatabaseStats()  {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT count(*) FROM %s",
                         kUrlResourceTableName).c_str()));
  if (statement.Step())
    UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.UrlTableRowCount",
                         statement.ColumnInt(0));

  statement.Assign(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT count(*) FROM %s",
                         kHostResourceTableName).c_str()));
  if (statement.Step())
    UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HostTableRowCount",
                         statement.ColumnInt(0));
}

Statement*
    ResourcePrefetchPredictorTables::GetUrlResourceDeleteStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                         kUrlResourceTableName).c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetUrlResourceUpdateStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "INSERT INTO %s "
          "(main_page_url, resource_url, resource_type, number_of_hits, "
          "number_of_misses, consecutive_misses, average_position) "
          "VALUES (?,?,?,?,?,?,?)", kUrlResourceTableName).c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetUrlMetadataDeleteStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                         kUrlMetadataTableName).c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetUrlMetadataUpdateStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "INSERT INTO %s (main_page_url, last_visit_time) VALUES (?,?)",
          kUrlMetadataTableName).c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetHostResourceDeleteStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                         kHostResourceTableName).c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetHostResourceUpdateStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "INSERT INTO %s "
          "(main_page_url, resource_url, resource_type, number_of_hits, "
          "number_of_misses, consecutive_misses, average_position) "
          "VALUES (?,?,?,?,?,?,?)", kHostResourceTableName).c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetHostMetadataDeleteStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE main_page_url=?",
                         kHostMetadataTableName).c_str()));
}

Statement* ResourcePrefetchPredictorTables::GetHostMetadataUpdateStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "INSERT INTO %s (main_page_url, last_visit_time) VALUES (?,?)",
          kHostMetadataTableName).c_str()));
}

}  // namespace predictors
