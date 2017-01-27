// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

#include <algorithm>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "sql/statement.h"

using google::protobuf::MessageLite;

namespace {

const char kMetadataTableName[] = "resource_prefetch_predictor_metadata";
const char kUrlResourceTableName[] = "resource_prefetch_predictor_url";
const char kUrlRedirectTableName[] = "resource_prefetch_predictor_url_redirect";
const char kHostResourceTableName[] = "resource_prefetch_predictor_host";
const char kHostRedirectTableName[] =
    "resource_prefetch_predictor_host_redirect";

const char kCreateGlobalMetadataStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, value INTEGER, "
    "PRIMARY KEY (key))";
const char kCreateProtoTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, "
    "proto BLOB, "
    "PRIMARY KEY(key))";
const char kInsertProtoTableStatementTemplate[] =
    "INSERT INTO %s (key, proto) VALUES (?,?)";
const char kDeleteProtoTableStatementTemplate[] = "DELETE FROM %s WHERE key=?";

void BindProtoDataToStatement(const std::string& key,
                              const MessageLite& data,
                              sql::Statement* statement) {
  int size = data.ByteSize();
  DCHECK_GT(size, 0);
  std::vector<char> proto_buffer(size);
  data.SerializeToArray(&proto_buffer[0], size);

  statement->BindString(0, key);
  statement->BindBlob(1, &proto_buffer[0], size);
}

bool StepAndInitializeProtoData(sql::Statement* statement,
                                std::string* key,
                                MessageLite* data) {
  if (!statement->Step())
    return false;

  *key = statement->ColumnString(0);

  int size = statement->ColumnByteLength(1);
  const void* blob = statement->ColumnBlob(1);
  DCHECK(blob);
  data->ParseFromArray(blob, size);

  return true;
}

}  // namespace

namespace predictors {

using content::BrowserThread;

// static
void ResourcePrefetchPredictorTables::TrimResources(
    PrefetchData* data,
    size_t max_consecutive_misses) {
  auto new_end = std::remove_if(
      data->mutable_resources()->begin(), data->mutable_resources()->end(),
      [max_consecutive_misses](const ResourceData& x) {
        return x.consecutive_misses() >= max_consecutive_misses;
      });
  data->mutable_resources()->erase(new_end, data->mutable_resources()->end());
}

// static
void ResourcePrefetchPredictorTables::SortResources(PrefetchData* data) {
  std::sort(data->mutable_resources()->begin(),
            data->mutable_resources()->end(),
            [](const ResourceData& x, const ResourceData& y) {
              // Decreasing score ordering.
              return ComputeResourceScore(x) > ComputeResourceScore(y);
            });
}

// static
void ResourcePrefetchPredictorTables::TrimRedirects(
    RedirectData* data,
    size_t max_consecutive_misses) {
  auto new_end =
      std::remove_if(data->mutable_redirect_endpoints()->begin(),
                     data->mutable_redirect_endpoints()->end(),
                     [max_consecutive_misses](const RedirectStat& x) {
                       return x.consecutive_misses() >= max_consecutive_misses;
                     });
  data->mutable_redirect_endpoints()->erase(
      new_end, data->mutable_redirect_endpoints()->end());
}

void ResourcePrefetchPredictorTables::GetAllData(
    PrefetchDataMap* url_data_map,
    PrefetchDataMap* host_data_map,
    RedirectDataMap* url_redirect_data_map,
    RedirectDataMap* host_redirect_data_map) {
  TRACE_EVENT0("browser", "ResourcePrefetchPredictor::GetAllData");
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

  GetAllResourceDataHelper(PREFETCH_KEY_TYPE_URL, url_data_map);
  GetAllResourceDataHelper(PREFETCH_KEY_TYPE_HOST, host_data_map);
  GetAllRedirectDataHelper(PREFETCH_KEY_TYPE_URL, url_redirect_data_map);
  GetAllRedirectDataHelper(PREFETCH_KEY_TYPE_HOST, host_redirect_data_map);
}

void ResourcePrefetchPredictorTables::UpdateData(
    const PrefetchData& url_data,
    const PrefetchData& host_data,
    const RedirectData& url_redirect_data,
    const RedirectData& host_redirect_data) {
  TRACE_EVENT0("browser", "ResourcePrefetchPredictor::UpdateData");
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  DCHECK(url_data.has_primary_key() || host_data.has_primary_key() ||
         url_redirect_data.has_primary_key() ||
         host_redirect_data.has_primary_key());

  DB()->BeginTransaction();

  bool success =
      (!url_data.has_primary_key() ||
       UpdateDataHelper(PREFETCH_KEY_TYPE_URL, PrefetchDataType::RESOURCE,
                        url_data.primary_key(), url_data)) &&
      (!host_data.has_primary_key() ||
       UpdateDataHelper(PREFETCH_KEY_TYPE_HOST, PrefetchDataType::RESOURCE,
                        host_data.primary_key(), host_data)) &&
      (!url_redirect_data.has_primary_key() ||
       UpdateDataHelper(PREFETCH_KEY_TYPE_URL, PrefetchDataType::REDIRECT,
                        url_redirect_data.primary_key(), url_redirect_data)) &&
      (!host_redirect_data.has_primary_key() ||
       UpdateDataHelper(PREFETCH_KEY_TYPE_HOST, PrefetchDataType::REDIRECT,
                        host_redirect_data.primary_key(), host_redirect_data));
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
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  sql::Statement deleter;
  for (const char* table_name :
       {kUrlResourceTableName, kUrlRedirectTableName, kHostResourceTableName,
        kHostRedirectTableName}) {
    deleter.Assign(DB()->GetUniqueStatement(
        base::StringPrintf("DELETE FROM %s", table_name).c_str()));
    deleter.Run();
  }
}

ResourcePrefetchPredictorTables::ResourcePrefetchPredictorTables() {}

ResourcePrefetchPredictorTables::~ResourcePrefetchPredictorTables() {}

void ResourcePrefetchPredictorTables::GetAllResourceDataHelper(
    PrefetchKeyType key_type,
    PrefetchDataMap* data_map) {
  // Read the resources table and organize it per primary key.
  const char* table_name = GetTableName(key_type, PrefetchDataType::RESOURCE);
  sql::Statement resource_reader(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT * FROM %s", table_name).c_str()));

  PrefetchData data;
  std::string key;
  while (StepAndInitializeProtoData(&resource_reader, &key, &data)) {
    data_map->insert(std::make_pair(key, data));
    DCHECK_EQ(data.primary_key(), key);
  }

  // Sort each of the resource vectors by score.
  for (auto& kv : *data_map) {
    SortResources(&(kv.second));
  }
}

void ResourcePrefetchPredictorTables::GetAllRedirectDataHelper(
    PrefetchKeyType key_type,
    RedirectDataMap* data_map) {
  // Read the redirects table and organize it per primary key.
  const char* table_name = GetTableName(key_type, PrefetchDataType::REDIRECT);
  sql::Statement redirect_reader(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT * FROM %s", table_name).c_str()));

  RedirectData data;
  std::string key;
  while (StepAndInitializeProtoData(&redirect_reader, &key, &data)) {
    data_map->insert(std::make_pair(key, data));
    DCHECK_EQ(data.primary_key(), key);
  }
}

bool ResourcePrefetchPredictorTables::UpdateDataHelper(
    PrefetchKeyType key_type,
    PrefetchDataType data_type,
    const std::string& key,
    const MessageLite& data) {
  // Delete the older data from the table.
  std::unique_ptr<sql::Statement> deleter(
      GetTableUpdateStatement(key_type, data_type, TableOperationType::REMOVE));
  deleter->BindString(0, key);
  if (!deleter->Run())
    return false;

  // Add the new data to the table.
  std::unique_ptr<sql::Statement> inserter(
      GetTableUpdateStatement(key_type, data_type, TableOperationType::INSERT));
  BindProtoDataToStatement(key, data, inserter.get());
  return inserter->Run();
}

void ResourcePrefetchPredictorTables::DeleteDataHelper(
    PrefetchKeyType key_type,
    PrefetchDataType data_type,
    const std::vector<std::string>& keys) {
  for (const std::string& key : keys) {
    std::unique_ptr<sql::Statement> deleter(GetTableUpdateStatement(
        key_type, data_type, TableOperationType::REMOVE));
    deleter->BindString(0, key);
    deleter->Run();
  }
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
bool ResourcePrefetchPredictorTables::DropTablesIfOutdated(
    sql::Connection* db) {
  int version = GetDatabaseVersion(db);
  bool success = true;
  // Too new is also a problem.
  bool incompatible_version = version != kDatabaseVersion;

  // These are deprecated tables but they still have to be removed if present.
  static const char kUrlMetadataTableName[] =
      "resource_prefetch_predictor_url_metadata";
  static const char kHostMetadataTableName[] =
      "resource_prefetch_predictor_host_metadata";

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
  bool success = db->BeginTransaction();
  success = success && DropTablesIfOutdated(db);

  for (const char* table_name :
       {kUrlResourceTableName, kHostResourceTableName, kUrlRedirectTableName,
        kHostRedirectTableName}) {
    success = success &&
              (db->DoesTableExist(table_name) ||
               db->Execute(base::StringPrintf(
                               kCreateProtoTableStatementTemplate, table_name)
                               .c_str()));
  }

  if (success)
    success = db->CommitTransaction();
  else
    db->RollbackTransaction();

  if (!success)
    ResetDB();
}

void ResourcePrefetchPredictorTables::LogDatabaseStats() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT count(*) FROM %s", kUrlResourceTableName)
          .c_str()));
  if (statement.Step())
    UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.UrlTableRowCount2",
                         statement.ColumnInt(0));

  statement.Assign(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT count(*) FROM %s", kHostResourceTableName)
          .c_str()));
  if (statement.Step())
    UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HostTableRowCount2",
                         statement.ColumnInt(0));
}

std::unique_ptr<sql::Statement>
ResourcePrefetchPredictorTables::GetTableUpdateStatement(
    PrefetchKeyType key_type,
    PrefetchDataType data_type,
    TableOperationType op_type) {
  sql::StatementID id(__FILE__, key_type | (static_cast<int>(data_type) << 1) |
                                    (static_cast<int>(op_type) << 2));
  const char* statement_template = (op_type == TableOperationType::REMOVE
                                        ? kDeleteProtoTableStatementTemplate
                                        : kInsertProtoTableStatementTemplate);
  const char* table_name = GetTableName(key_type, data_type);
  return base::MakeUnique<sql::Statement>(DB()->GetCachedStatement(
      id, base::StringPrintf(statement_template, table_name).c_str()));
}

// static
const char* ResourcePrefetchPredictorTables::GetTableName(
    PrefetchKeyType key_type,
    PrefetchDataType data_type) {
  bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;
  switch (data_type) {
    case PrefetchDataType::RESOURCE:
      return is_host ? kHostResourceTableName : kUrlResourceTableName;
    case PrefetchDataType::REDIRECT:
      return is_host ? kHostRedirectTableName : kUrlRedirectTableName;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace predictors
