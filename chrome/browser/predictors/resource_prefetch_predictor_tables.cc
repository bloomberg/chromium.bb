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
using RedirectData = predictors::RedirectData;

const char kMetadataTableName[] = "resource_prefetch_predictor_metadata";
const char kUrlResourceTableName[] = "resource_prefetch_predictor_url";
const char kUrlMetadataTableName[] = "resource_prefetch_predictor_url_metadata";
const char kUrlRedirectTableName[] = "resource_prefetch_predictor_url_redirect";
const char kHostResourceTableName[] = "resource_prefetch_predictor_host";
const char kHostMetadataTableName[] =
    "resource_prefetch_predictor_host_metadata";
const char kHostRedirectTableName[] =
    "resource_prefetch_predictor_host_redirect";

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
const char kCreateRedirectTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "main_page_url TEXT, "
    "proto BLOB, "
    "PRIMARY KEY(main_page_url))";

const char kInsertResourceTableStatementTemplate[] =
    "INSERT INTO %s (main_page_url, resource_url, proto) VALUES (?,?,?)";
const char kInsertRedirectTableStatementTemplate[] =
    "INSERT INTO %s (main_page_url, proto) VALUES (?,?)";
const char kInsertMetadataTableStatementTemplate[] =
    "INSERT INTO %s (main_page_url, last_visit_time) VALUES (?,?)";
const char kDeleteStatementTemplate[] = "DELETE FROM %s WHERE main_page_url=?";

void BindResourceDataToStatement(const ResourceData& data,
                                 const std::string& primary_key,
                                 Statement* statement) {
  int size = data.ByteSize();
  DCHECK_GT(size, 0);
  std::vector<char> proto_buffer(size);
  data.SerializeToArray(&proto_buffer[0], size);

  statement->BindString(0, primary_key);
  statement->BindString(1, data.resource_url());
  statement->BindBlob(2, &proto_buffer[0], size);
}

void BindRedirectDataToStatement(const RedirectData& data,
                                 Statement* statement) {
  int size = data.ByteSize();
  DCHECK_GT(size, 0);
  std::vector<char> proto_buffer(size);
  data.SerializeToArray(&proto_buffer[0], size);

  statement->BindString(0, data.primary_key());
  statement->BindBlob(1, &proto_buffer[0], size);
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

bool StepAndInitializeRedirectData(Statement* statement,
                                   RedirectData* data,
                                   std::string* primary_key) {
  if (!statement->Step())
    return false;

  *primary_key = statement->ColumnString(0);

  int size = statement->ColumnByteLength(1);
  const void* blob = statement->ColumnBlob(1);
  DCHECK(blob);
  data->ParseFromArray(blob, size);

  DCHECK(data->primary_key() == *primary_key);

  return true;
}

}  // namespace

namespace predictors {

// static
void ResourcePrefetchPredictorTables::SortResources(
    std::vector<ResourceData>* resources) {
  std::sort(resources->begin(), resources->end(),
            [](const ResourceData& x, const ResourceData& y) {
              // Decreasing score ordering.
              return ComputeResourceScore(x) > ComputeResourceScore(y);
            });
}

// static
void ResourcePrefetchPredictorTables::SortRedirects(
    std::vector<RedirectStat>* redirects) {
  std::sort(redirects->begin(), redirects->end(),
            [](const RedirectStat& x, const RedirectStat& y) {
              // Decreasing score ordering.
              return ComputeRedirectScore(x) > ComputeRedirectScore(y);
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
    PrefetchDataMap* host_data_map,
    RedirectDataMap* url_redirect_data_map,
    RedirectDataMap* host_redirect_data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DCHECK(url_data_map);
  DCHECK(host_data_map);
  DCHECK(url_redirect_data_map);
  DCHECK(host_redirect_data_map);
  url_data_map->clear();
  host_data_map->clear();
  url_redirect_data_map->clear();
  host_redirect_data_map->clear();

  std::vector<std::string> urls_to_delete, hosts_to_delete;
  GetAllResourceDataHelper(PREFETCH_KEY_TYPE_URL, url_data_map,
                           &urls_to_delete);
  GetAllResourceDataHelper(PREFETCH_KEY_TYPE_HOST, host_data_map,
                           &hosts_to_delete);
  GetAllRedirectDataHelper(PREFETCH_KEY_TYPE_URL, url_redirect_data_map);
  GetAllRedirectDataHelper(PREFETCH_KEY_TYPE_HOST, host_redirect_data_map);

  if (!urls_to_delete.empty() || !hosts_to_delete.empty())
    DeleteResourceData(urls_to_delete, hosts_to_delete);
}

void ResourcePrefetchPredictorTables::UpdateData(
    const PrefetchData& url_data,
    const PrefetchData& host_data,
    const RedirectData& url_redirect_data,
    const RedirectData& host_redirect_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DCHECK(!url_data.is_host() && host_data.is_host());
  DCHECK(!url_data.primary_key.empty() || !host_data.primary_key.empty() ||
         url_redirect_data.has_primary_key() ||
         host_redirect_data.has_primary_key());

  DB()->BeginTransaction();

  bool success =
      (url_data.primary_key.empty() ||
       UpdateResourceDataHelper(PREFETCH_KEY_TYPE_URL, url_data)) &&
      (host_data.primary_key.empty() ||
       UpdateResourceDataHelper(PREFETCH_KEY_TYPE_HOST, host_data)) &&
      (!url_redirect_data.has_primary_key() ||
       UpdateRedirectDataHelper(PREFETCH_KEY_TYPE_URL, url_redirect_data)) &&
      (!host_redirect_data.has_primary_key() ||
       UpdateRedirectDataHelper(PREFETCH_KEY_TYPE_HOST, host_redirect_data));
  if (!success)
    DB()->RollbackTransaction();
  else
    DB()->CommitTransaction();
}

void ResourcePrefetchPredictorTables::DeleteResourceData(
    const std::vector<std::string>& urls,
    const std::vector<std::string>& hosts) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DCHECK(!urls.empty() || !hosts.empty());

  if (!urls.empty())
    DeleteDataHelper(PREFETCH_KEY_TYPE_URL, PrefetchDataType::RESOURCE, urls);
  if (!hosts.empty())
    DeleteDataHelper(PREFETCH_KEY_TYPE_HOST, PrefetchDataType::RESOURCE, hosts);
}

void ResourcePrefetchPredictorTables::DeleteSingleResourceDataPoint(
    const std::string& key,
    PrefetchKeyType key_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DeleteDataHelper(key_type, PrefetchDataType::RESOURCE, {key});
}

void ResourcePrefetchPredictorTables::DeleteRedirectData(
    const std::vector<std::string>& urls,
    const std::vector<std::string>& hosts) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DCHECK(!urls.empty() || !hosts.empty());

  if (!urls.empty())
    DeleteDataHelper(PREFETCH_KEY_TYPE_URL, PrefetchDataType::REDIRECT, urls);
  if (!hosts.empty())
    DeleteDataHelper(PREFETCH_KEY_TYPE_HOST, PrefetchDataType::REDIRECT, hosts);
}

void ResourcePrefetchPredictorTables::DeleteSingleRedirectDataPoint(
    const std::string& key,
    PrefetchKeyType key_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DeleteDataHelper(key_type, PrefetchDataType::REDIRECT, {key});
}

void ResourcePrefetchPredictorTables::DeleteAllData() {
  if (CantAccessDatabase())
    return;

  Statement deleter;
  for (const char* table_name :
       {kUrlResourceTableName, kUrlMetadataTableName, kUrlRedirectTableName,
        kHostResourceTableName, kHostMetadataTableName,
        kHostRedirectTableName}) {
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

void ResourcePrefetchPredictorTables::GetAllResourceDataHelper(
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
  const char* metadata_table_name =
      is_host ? kHostMetadataTableName : kUrlMetadataTableName;
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

void ResourcePrefetchPredictorTables::GetAllRedirectDataHelper(
    PrefetchKeyType key_type,
    RedirectDataMap* redirect_map) {
  bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;

  const char* redirect_table_name =
      is_host ? kHostRedirectTableName : kUrlRedirectTableName;
  Statement redirect_reader(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT * FROM %s", redirect_table_name).c_str()));

  RedirectData data;
  std::string primary_key;
  while (StepAndInitializeRedirectData(&redirect_reader, &data, &primary_key)) {
    auto result = redirect_map->insert(std::make_pair(primary_key, data));
    DCHECK(result.second);
  }
}

bool ResourcePrefetchPredictorTables::UpdateResourceDataHelper(
    PrefetchKeyType key_type,
    const PrefetchData& data) {
  DCHECK(!data.primary_key.empty());

  if (!StringsAreSmallerThanDBLimit(data)) {
    UMA_HISTOGRAM_BOOLEAN("ResourcePrefetchPredictor.DbStringTooLong", true);
    return false;
  }

  // Delete the older data from both the tables.
  std::unique_ptr<Statement> deleter(GetTableUpdateStatement(
      key_type, PrefetchDataType::RESOURCE, TableOperationType::REMOVE));
  deleter->BindString(0, data.primary_key);
  if (!deleter->Run())
    return false;

  deleter = GetTableUpdateStatement(key_type, PrefetchDataType::METADATA,
                                    TableOperationType::REMOVE);
  deleter->BindString(0, data.primary_key);
  if (!deleter->Run())
    return false;

  // Add the new data to the tables.
  for (const ResourceData& resource : data.resources) {
    std::unique_ptr<Statement> resource_inserter(GetTableUpdateStatement(
        key_type, PrefetchDataType::RESOURCE, TableOperationType::INSERT));
    BindResourceDataToStatement(resource, data.primary_key,
                                resource_inserter.get());
    if (!resource_inserter->Run())
      return false;
  }

  std::unique_ptr<Statement> metadata_inserter(GetTableUpdateStatement(
      key_type, PrefetchDataType::METADATA, TableOperationType::INSERT));
  metadata_inserter->BindString(0, data.primary_key);
  metadata_inserter->BindInt64(1, data.last_visit.ToInternalValue());
  return metadata_inserter->Run();
}

bool ResourcePrefetchPredictorTables::UpdateRedirectDataHelper(
    PrefetchKeyType key_type,
    const RedirectData& data) {
  DCHECK(data.has_primary_key());

  if (!StringsAreSmallerThanDBLimit(data)) {
    UMA_HISTOGRAM_BOOLEAN("ResourcePrefetchPredictor.DbStringTooLong", true);
    return false;
  }

  // Delete the older data from the table.
  std::unique_ptr<Statement> deleter(GetTableUpdateStatement(
      key_type, PrefetchDataType::REDIRECT, TableOperationType::REMOVE));
  deleter->BindString(0, data.primary_key());
  if (!deleter->Run())
    return false;

  // Add the new data to the table.
  std::unique_ptr<Statement> inserter(GetTableUpdateStatement(
      key_type, PrefetchDataType::REDIRECT, TableOperationType::INSERT));
  BindRedirectDataToStatement(data, inserter.get());
  return inserter->Run();
}

void ResourcePrefetchPredictorTables::DeleteDataHelper(
    PrefetchKeyType key_type,
    PrefetchDataType data_type,
    const std::vector<std::string>& keys) {
  bool is_resource = data_type == PrefetchDataType::RESOURCE;

  for (const std::string& key : keys) {
    std::unique_ptr<Statement> deleter(GetTableUpdateStatement(
        key_type, data_type, TableOperationType::REMOVE));
    deleter->BindString(0, key);
    deleter->Run();

    if (is_resource) {
      // Delete corresponding resource metadata as well.
      deleter = GetTableUpdateStatement(key_type, PrefetchDataType::METADATA,
                                        TableOperationType::REMOVE);
      deleter->BindString(0, key);
      deleter->Run();
    }
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

bool ResourcePrefetchPredictorTables::StringsAreSmallerThanDBLimit(
    const RedirectData& data) {
  if (data.primary_key().length() > kMaxStringLength)
    return false;

  for (const RedirectStat& redirect : data.redirect_endpoints()) {
    if (redirect.url().length() > kMaxStringLength)
      return false;
  }
  return true;
}

// static
float ResourcePrefetchPredictorTables::ComputeResourceScore(
    const ResourceData& data) {
  // The ranking is done by considering, in this order:
  // 1. Resource Priority
  // 2. Request resource type
  // 3. Finally, the average position, giving a higher priotity to earlier
  //    resources.

  int priority_multiplier;
  switch (data.priority()) {
    case ResourceData::REQUEST_PRIORITY_HIGHEST:
      priority_multiplier = 3;
      break;
    case ResourceData::REQUEST_PRIORITY_MEDIUM:
      priority_multiplier = 2;
      break;
    case ResourceData::REQUEST_PRIORITY_LOW:
    case ResourceData::REQUEST_PRIORITY_LOWEST:
    case ResourceData::REQUEST_PRIORITY_IDLE:
    default:
      priority_multiplier = 1;
      break;
  }

  int type_multiplier;
  switch (data.resource_type()) {
    case ResourceData::RESOURCE_TYPE_STYLESHEET:
    case ResourceData::RESOURCE_TYPE_SCRIPT:
      type_multiplier = 3;
      break;
    case ResourceData::RESOURCE_TYPE_FONT_RESOURCE:
      type_multiplier = 2;
      break;
    case ResourceData::RESOURCE_TYPE_IMAGE:
    default:
      type_multiplier = 1;
  }

  constexpr int kMaxResourcesPerType = 100;
  return kMaxResourcesPerType *
             (priority_multiplier * 100 + type_multiplier * 10) -
         data.average_position();
}

// static
float ResourcePrefetchPredictorTables::ComputeRedirectScore(
    const RedirectStat& data) {
  // TODO(alexilin): Invent some scoring.
  return 0.0;
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
          kUrlRedirectTableName, kHostRedirectTableName, kUrlMetadataTableName,
          kHostMetadataTableName}) {
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
      (db->DoesTableExist(kUrlRedirectTableName) ||
       db->Execute(base::StringPrintf(kCreateRedirectTableStatementTemplate,
                                      kUrlRedirectTableName)
                       .c_str())) &&
      (db->DoesTableExist(kHostResourceTableName) ||
       db->Execute(base::StringPrintf(kCreateResourceTableStatementTemplate,
                                      kHostResourceTableName)
                       .c_str())) &&
      (db->DoesTableExist(kHostMetadataTableName) ||
       db->Execute(base::StringPrintf(kCreateMetadataTableStatementTemplate,
                                      kHostMetadataTableName)
                       .c_str())) &&
      (db->DoesTableExist(kHostRedirectTableName) ||
       db->Execute(base::StringPrintf(kCreateRedirectTableStatementTemplate,
                                      kHostRedirectTableName)
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

std::unique_ptr<Statement>
ResourcePrefetchPredictorTables::GetTableUpdateStatement(
    PrefetchKeyType key_type,
    PrefetchDataType data_type,
    TableOperationType op_type) {
  sql::StatementID id(__FILE__, key_type | (static_cast<int>(data_type) << 1) |
                                    (static_cast<int>(op_type) << 3));
  const char* statement_template =
      GetTableUpdateStatementTemplate(op_type, data_type);
  const char* table_name =
      GetTableUpdateStatementTableName(key_type, data_type);
  return base::MakeUnique<Statement>(DB()->GetCachedStatement(
      id, base::StringPrintf(statement_template, table_name).c_str()));
}

// static
const char* ResourcePrefetchPredictorTables::GetTableUpdateStatementTemplate(
    TableOperationType op_type,
    PrefetchDataType data_type) {
  switch (op_type) {
    case TableOperationType::REMOVE:
      return kDeleteStatementTemplate;
    case TableOperationType::INSERT:
      switch (data_type) {
        case PrefetchDataType::RESOURCE:
          return kInsertResourceTableStatementTemplate;
        case PrefetchDataType::REDIRECT:
          return kInsertRedirectTableStatementTemplate;
        case PrefetchDataType::METADATA:
          return kInsertMetadataTableStatementTemplate;
      }
  }

  NOTREACHED();
  return nullptr;
}

// static
const char* ResourcePrefetchPredictorTables::GetTableUpdateStatementTableName(
    PrefetchKeyType key_type,
    PrefetchDataType data_type) {
  DCHECK(key_type == PREFETCH_KEY_TYPE_URL ||
         key_type == PREFETCH_KEY_TYPE_HOST);
  bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;
  switch (data_type) {
    case PrefetchDataType::RESOURCE:
      return is_host ? kHostResourceTableName : kUrlResourceTableName;
    case PrefetchDataType::REDIRECT:
      return is_host ? kHostRedirectTableName : kUrlRedirectTableName;
    case PrefetchDataType::METADATA:
      return is_host ? kHostMetadataTableName : kUrlMetadataTableName;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace predictors
