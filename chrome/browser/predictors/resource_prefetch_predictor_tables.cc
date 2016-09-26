// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

using content::BrowserThread;
using sql::Statement;

namespace {

using ResourceData = predictors::ResourceData;

const char kMetadataTableName[] = "resource_prefetch_predictor_metadata";
const char kUrlResourceTableName[] = "resource_prefetch_predictor_url";
const char kUrlMetadataTableName[] = "resource_prefetch_predictor_url_metadata";
const char kHostResourceTableName[] = "resource_prefetch_predictor_host";
const char kHostMetadataTableName[] =
    "resource_prefetch_predictor_host_metadata";

const char kCreateGlobalMetadataStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, value INTEGER, "
    "PRIMARY KEY (key))";
const char kCreateResourceTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "main_page_url TEXT, "
    "resource_url TEXT, "
    "proto BLOB, "
    "PRIMARY KEY(main_page_url, resource_url))";
const char kCreateMetadataTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "main_page_url TEXT, "
    "last_visit_time INTEGER, "
    "PRIMARY KEY(main_page_url))";

const char kInsertResourceTableStatementTemplate[] =
    "INSERT INTO %s (main_page_url, resource_url, proto) VALUES (?,?,?)";
const char kInsertMetadataTableStatementTemplate[] =
    "INSERT INTO %s (main_page_url, last_visit_time) VALUES (?,?)";
const char kDeleteStatementTemplate[] = "DELETE FROM %s WHERE main_page_url=?";

void BindResourceDataToStatement(const ResourceData& data,
                                 const std::string& primary_key,
                                 Statement* statement) {
  int size = data.ByteSize();
  DCHECK(size > 0);
  std::vector<char> proto_buffer(size);
  data.SerializeToArray(&proto_buffer[0], size);

  statement->BindString(0, primary_key);
  statement->BindString(1, data.resource_url());
  statement->BindBlob(2, &proto_buffer[0], size);
}

bool StepAndInitializeResourceData(Statement* statement,
                                   ResourceData* data,
                                   std::string* primary_key) {
  if (!statement->Step())
    return false;

  *primary_key = statement->ColumnString(0);

  int size = statement->ColumnByteLength(2);
  const void* blob = statement->ColumnBlob(2);
  DCHECK(blob);
  data->ParseFromArray(blob, size);

  std::string resource_url = statement->ColumnString(1);
  DCHECK(resource_url == data->resource_url());

  return true;
}

}  // namespace

namespace predictors {

// static
void ResourcePrefetchPredictorTables::SortResources(
    std::vector<ResourceData>* resources) {
  // Sort indices instead of ResourceData objects and then apply resulting
  // permutation to the resources.
  std::sort(resources->begin(), resources->end(),
            [](const ResourceData& x, const ResourceData& y) {
              return ComputeScore(x) > ComputeScore(y);
            });
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

  Statement deleter;
  for (const char* table_name :
       {kUrlResourceTableName, kUrlMetadataTableName, kHostResourceTableName,
        kHostMetadataTableName}) {
    deleter.Assign(DB()->GetUniqueStatement(
        base::StringPrintf("DELETE FROM %s", table_name).c_str()));
    deleter.Run();
  }
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

  ResourceData resource;
  std::string primary_key;
  while (StepAndInitializeResourceData(&resource_reader, &resource,
                                       &primary_key)) {
    PrefetchDataMap::iterator it = data_map->find(primary_key);
    if (it == data_map->end()) {
      it = data_map->insert(std::make_pair(
          primary_key, PrefetchData(key_type, primary_key))).first;
    }
    it->second.resources.push_back(resource);
  }

  // Sort each of the resource row vectors by score.
  for (auto& kv : *data_map)
    SortResources(&(kv.second.resources));

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
      int64_t last_visit = metadata_reader.ColumnInt64(1);
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
  std::unique_ptr<Statement> deleter(data.is_host()
                                         ? GetHostResourceDeleteStatement()
                                         : GetUrlResourceDeleteStatement());
  deleter->BindString(0, data.primary_key);
  if (!deleter->Run())
    return false;

  deleter.reset(data.is_host() ? GetHostMetadataDeleteStatement() :
      GetUrlMetadataDeleteStatement());
  deleter->BindString(0, data.primary_key);
  if (!deleter->Run())
    return false;

  // Add the new data to the tables.
  for (const ResourceData& resource : data.resources) {
    std::unique_ptr<Statement> resource_inserter(
        data.is_host() ? GetHostResourceUpdateStatement()
                       : GetUrlResourceUpdateStatement());
    BindResourceDataToStatement(resource, data.primary_key,
                                resource_inserter.get());
    if (!resource_inserter->Run())
      return false;
  }

  std::unique_ptr<Statement> metadata_inserter(
      data.is_host() ? GetHostMetadataUpdateStatement()
                     : GetUrlMetadataUpdateStatement());
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

  for (const std::string& key : keys) {
    std::unique_ptr<Statement> deleter(is_host
                                           ? GetHostResourceDeleteStatement()
                                           : GetUrlResourceDeleteStatement());
    deleter->BindString(0, key);
    deleter->Run();

    deleter.reset(is_host ? GetHostMetadataDeleteStatement() :
        GetUrlMetadataDeleteStatement());
    deleter->BindString(0, key);
    deleter->Run();
  }
}

// static
bool ResourcePrefetchPredictorTables::StringsAreSmallerThanDBLimit(
    const PrefetchData& data) {
  if (data.primary_key.length() > kMaxStringLength)
    return false;

  for (const ResourceData& resource : data.resources) {
    if (resource.resource_url().length() > kMaxStringLength)
      return false;
  }
  return true;
}

// static
float ResourcePrefetchPredictorTables::ComputeScore(const ResourceData& data) {
  // The score is calculated so that when the rows are sorted, stylesheets,
  // scripts and fonts appear first, sorted by position(ascending) and then the
  // rest of the resources sorted by position (ascending).
  static const int kMaxResourcesPerType = 100;
  switch (data.resource_type()) {
    case ResourceData::RESOURCE_TYPE_STYLESHEET:
    case ResourceData::RESOURCE_TYPE_SCRIPT:
    case ResourceData::RESOURCE_TYPE_FONT_RESOURCE:
      return (2 * kMaxResourcesPerType) - data.average_position();

    case ResourceData::RESOURCE_TYPE_IMAGE:
    default:
      return kMaxResourcesPerType - data.average_position();
  }
  // TODO(lizeb): Take priority into account.
}

// static
bool ResourcePrefetchPredictorTables::DropTablesIfOutdated(
    sql::Connection* db) {
  int version = GetDatabaseVersion(db);
  bool success = true;
  // Too new is also a problem.
  bool incompatible_version = version != kDatabaseVersion;

  if (incompatible_version) {
    for (const char* table_name :
         {kMetadataTableName, kUrlResourceTableName, kHostResourceTableName,
          kUrlMetadataTableName, kHostMetadataTableName}) {
      success =
          success &&
          db->Execute(base::StringPrintf("DROP TABLE IF EXISTS %s", table_name)
                          .c_str());
    }
  }

  if (incompatible_version) {
    success =
        success &&
        db->Execute(base::StringPrintf(kCreateGlobalMetadataStatementTemplate,
                                       kMetadataTableName)
                        .c_str());
    success = success && SetDatabaseVersion(db, kDatabaseVersion);
  }

  return success;
}

// static
int ResourcePrefetchPredictorTables::GetDatabaseVersion(sql::Connection* db) {
  int version = 0;
  if (db->DoesTableExist(kMetadataTableName)) {
    sql::Statement statement(db->GetUniqueStatement(
        base::StringPrintf("SELECT value FROM %s WHERE key='version'",
                           kMetadataTableName)
            .c_str()));
    if (statement.Step())
      version = statement.ColumnInt(0);
  }
  return version;
}

// static
bool ResourcePrefetchPredictorTables::SetDatabaseVersion(sql::Connection* db,
                                                         int version) {
  sql::Statement statement(db->GetUniqueStatement(
      base::StringPrintf(
          "INSERT OR REPLACE INTO %s (key,value) VALUES ('version',%d)",
          kMetadataTableName, version)
          .c_str()));
  return statement.Run();
}

void ResourcePrefetchPredictorTables::CreateTableIfNonExistent() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  // Database initialization is all-or-nothing.
  sql::Connection* db = DB();
  sql::Transaction transaction{db};
  bool success = transaction.Begin();

  success = success && DropTablesIfOutdated(db);

  success =
      success &&
      (db->DoesTableExist(kUrlResourceTableName) ||
       db->Execute(base::StringPrintf(kCreateResourceTableStatementTemplate,
                                      kUrlResourceTableName)
                       .c_str())) &&
      (db->DoesTableExist(kUrlMetadataTableName) ||
       db->Execute(base::StringPrintf(kCreateMetadataTableStatementTemplate,
                                      kUrlMetadataTableName)
                       .c_str())) &&
      (db->DoesTableExist(kHostResourceTableName) ||
       db->Execute(base::StringPrintf(kCreateResourceTableStatementTemplate,
                                      kHostResourceTableName)
                       .c_str())) &&
      (db->DoesTableExist(kHostMetadataTableName) ||
       db->Execute(base::StringPrintf(kCreateMetadataTableStatementTemplate,
                                      kHostMetadataTableName)
                       .c_str()));

  if (success)
    success = transaction.Commit();
  else
    transaction.Rollback();

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
      base::StringPrintf(kDeleteStatementTemplate, kUrlResourceTableName)
          .c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetUrlResourceUpdateStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf(kInsertResourceTableStatementTemplate,
                                        kUrlResourceTableName)
                         .c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetUrlMetadataDeleteStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(kDeleteStatementTemplate, kUrlMetadataTableName)
          .c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetUrlMetadataUpdateStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf(kInsertMetadataTableStatementTemplate,
                                        kUrlMetadataTableName)
                         .c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetHostResourceDeleteStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(kDeleteStatementTemplate, kHostResourceTableName)
          .c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetHostResourceUpdateStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf(kInsertResourceTableStatementTemplate,
                                        kHostResourceTableName)
                         .c_str()));
}

Statement*
    ResourcePrefetchPredictorTables::GetHostMetadataDeleteStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(kDeleteStatementTemplate, kHostMetadataTableName)
          .c_str()));
}

Statement* ResourcePrefetchPredictorTables::GetHostMetadataUpdateStatement() {
  return new Statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf(kInsertMetadataTableStatementTemplate,
                                        kHostMetadataTableName)
                         .c_str()));
}

}  // namespace predictors
