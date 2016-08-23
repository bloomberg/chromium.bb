// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "sql/statement.h"

using content::BrowserThread;
using sql::Statement;

namespace {

using ResourceRow = predictors::ResourcePrefetchPredictorTables::ResourceRow;

const char kUrlResourceTableName[] = "resource_prefetch_predictor_url";
const char kUrlMetadataTableName[] = "resource_prefetch_predictor_url_metadata";
const char kHostResourceTableName[] = "resource_prefetch_predictor_host";
const char kHostMetadataTableName[] =
    "resource_prefetch_predictor_host_metadata";

const char kInsertResourceTableStatementTemplate[] =
    "INSERT INTO %s (main_page_url, resource_url, proto) VALUES (?,?,?)";
const char kInsertMetadataTableStatementTemplate[] =
    "INSERT INTO %s (main_page_url, last_visit_time) VALUES (?,?)";
const char kDeleteStatementTemplate[] = "DELETE FROM %s WHERE main_page_url=?";

void BindResourceRowToStatement(const ResourceRow& row,
                                const std::string& primary_key,
                                Statement* statement) {
  chrome_browser_predictors::ResourceData proto;
  row.ToProto(&proto);
  int size = proto.ByteSize();
  std::vector<char> proto_buffer(size);
  proto.SerializeToArray(&proto_buffer[0], size);

  statement->BindString(0, primary_key);
  statement->BindString(1, row.resource_url.spec());
  statement->BindBlob(2, &proto_buffer[0], size);
}

bool StepAndInitializeResourceRow(Statement* statement, ResourceRow* row) {
  if (!statement->Step())
    return false;

  int size = statement->ColumnByteLength(2);
  const void* data = statement->ColumnBlob(2);
  DCHECK(data);
  chrome_browser_predictors::ResourceData proto;
  proto.ParseFromArray(data, size);
  ResourceRow::FromProto(proto, row);

  row->primary_key = statement->ColumnString(0);
  row->resource_url = GURL(statement->ColumnString(1));
  return true;
}

}  // namespace

namespace predictors {

// static
const size_t ResourcePrefetchPredictorTables::kMaxStringLength = 1024;

ResourceRow::ResourceRow()
    : resource_type(content::RESOURCE_TYPE_LAST_TYPE),
      number_of_hits(0),
      number_of_misses(0),
      consecutive_misses(0),
      average_position(0.0),
      priority(net::IDLE),
      has_validators(false),
      always_revalidate(false),
      score(0.0) {}

ResourceRow::ResourceRow(const ResourceRow& other)
    : primary_key(other.primary_key),
      resource_url(other.resource_url),
      resource_type(other.resource_type),
      number_of_hits(other.number_of_hits),
      number_of_misses(other.number_of_misses),
      consecutive_misses(other.consecutive_misses),
      average_position(other.average_position),
      priority(other.priority),
      has_validators(other.has_validators),
      always_revalidate(other.always_revalidate),
      score(other.score) {}

ResourceRow::ResourceRow(const std::string& i_primary_key,
                         const std::string& i_resource_url,
                         content::ResourceType i_resource_type,
                         int i_number_of_hits,
                         int i_number_of_misses,
                         int i_consecutive_misses,
                         double i_average_position,
                         net::RequestPriority i_priority,
                         bool i_has_validators,
                         bool i_always_revalidate)
    : primary_key(i_primary_key),
      resource_url(i_resource_url),
      resource_type(i_resource_type),
      number_of_hits(i_number_of_hits),
      number_of_misses(i_number_of_misses),
      consecutive_misses(i_consecutive_misses),
      average_position(i_average_position),
      priority(i_priority),
      has_validators(i_has_validators),
      always_revalidate(i_always_revalidate) {
  UpdateScore();
}

void ResourceRow::UpdateScore() {
  // The score is calculated so that when the rows are sorted, stylesheets,
  // scripts and fonts appear first, sorted by position(ascending) and then the
  // rest of the resources sorted by position (ascending).
  static const int kMaxResourcesPerType = 100;
  switch (resource_type) {
    case content::RESOURCE_TYPE_STYLESHEET:
    case content::RESOURCE_TYPE_SCRIPT:
    case content::RESOURCE_TYPE_FONT_RESOURCE:
      score = (2 * kMaxResourcesPerType) - average_position;
      break;

    case content::RESOURCE_TYPE_IMAGE:
    default:
      score = kMaxResourcesPerType - average_position;
      break;
  }
  // TODO(lizeb): Take priority into account.
}

bool ResourceRow::operator==(const ResourceRow& rhs) const {
  return primary_key == rhs.primary_key && resource_url == rhs.resource_url &&
         resource_type == rhs.resource_type &&
         number_of_hits == rhs.number_of_hits &&
         number_of_misses == rhs.number_of_misses &&
         consecutive_misses == rhs.consecutive_misses &&
         average_position == rhs.average_position && priority == rhs.priority &&
         has_validators == rhs.has_validators &&
         always_revalidate == rhs.always_revalidate && score == rhs.score;
}

void ResourceRow::ToProto(ResourceData* resource_data) const {
  using chrome_browser_predictors::ResourceData_Priority;
  using chrome_browser_predictors::ResourceData_ResourceType;

  resource_data->set_primary_key(primary_key);
  resource_data->set_resource_url(resource_url.spec());
  resource_data->set_resource_type(
      static_cast<ResourceData_ResourceType>(resource_type));
  resource_data->set_number_of_hits(number_of_hits);
  resource_data->set_number_of_misses(number_of_misses);
  resource_data->set_consecutive_misses(consecutive_misses);
  resource_data->set_average_position(average_position);
  resource_data->set_priority(static_cast<ResourceData_Priority>(priority));
  resource_data->set_has_validators(has_validators);
  resource_data->set_always_revalidate(always_revalidate);
}

// static
void ResourceRow::FromProto(const ResourceData& proto, ResourceRow* row) {
  DCHECK(proto.has_primary_key());
  row->primary_key = proto.primary_key();
  row->resource_url = GURL(proto.resource_url());
  row->resource_type =
      static_cast<content::ResourceType>(proto.resource_type());
  row->number_of_hits = proto.number_of_hits();
  row->number_of_misses = proto.number_of_misses();
  row->consecutive_misses = proto.consecutive_misses();
  row->average_position = proto.average_position();
  row->priority = static_cast<net::RequestPriority>(proto.priority());
  row->has_validators = proto.has_validators();
  row->always_revalidate = proto.always_revalidate();
}

// static
void ResourcePrefetchPredictorTables::SortResourceRows(ResourceRows* rows) {
  std::sort(rows->begin(), rows->end(),
            [](const ResourceRow& x, const ResourceRow& y) {
              return x.score > y.score;
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
  for (auto& kv : *data_map)
    SortResourceRows(&(kv.second.resources));

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
  const ResourceRows& resources = data.resources;
  for (const ResourceRow& resource : resources) {
    std::unique_ptr<Statement> resource_inserter(
        data.is_host() ? GetHostResourceUpdateStatement()
                       : GetUrlResourceUpdateStatement());
    BindResourceRowToStatement(resource, data.primary_key,
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

  for (std::vector<std::string>::const_iterator it = keys.begin();
       it != keys.end(); ++it) {
    std::unique_ptr<Statement> deleter(is_host
                                           ? GetHostResourceDeleteStatement()
                                           : GetUrlResourceDeleteStatement());
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

bool ResourcePrefetchPredictorTables::DropTablesIfOutdated(
    sql::Connection* db) {
  bool success = true;
  for (const char* table_name :
       {kUrlResourceTableName, kHostResourceTableName}) {
    if (db->DoesTableExist(table_name) &&
        !db->DoesColumnExist(table_name, "proto")) {
      success &=
          db->Execute(base::StringPrintf("DROP TABLE %s", table_name).c_str());
    }
  }
  return success;
}

void ResourcePrefetchPredictorTables::CreateTableIfNonExistent() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  if (CantAccessDatabase())
    return;

  const char resource_table_creator[] =
      "CREATE TABLE %s ( "
      "main_page_url TEXT, "
      "resource_url TEXT, "
      "proto BLOB, "
      "PRIMARY KEY(main_page_url, resource_url))";
  const char* metadata_table_creator =
      "CREATE TABLE %s ( "
      "main_page_url TEXT, "
      "last_visit_time INTEGER, "
      "PRIMARY KEY(main_page_url))";

  sql::Connection* db = DB();
  bool success = DropTablesIfOutdated(db) &&
                 (db->DoesTableExist(kUrlResourceTableName) ||
                  db->Execute(base::StringPrintf(resource_table_creator,
                                                 kUrlResourceTableName)
                                  .c_str())) &&
                 (db->DoesTableExist(kUrlMetadataTableName) ||
                  db->Execute(base::StringPrintf(metadata_table_creator,
                                                 kUrlMetadataTableName)
                                  .c_str())) &&
                 (db->DoesTableExist(kHostResourceTableName) ||
                  db->Execute(base::StringPrintf(resource_table_creator,
                                                 kHostResourceTableName)
                                  .c_str())) &&
                 (db->DoesTableExist(kHostMetadataTableName) ||
                  db->Execute(base::StringPrintf(metadata_table_creator,
                                                 kHostMetadataTableName)
                                  .c_str()));

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
