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
#include "sql/statement.h"

using google::protobuf::MessageLite;

namespace {

const char kMetadataTableName[] = "resource_prefetch_predictor_metadata";
const char kUrlResourceTableName[] = "resource_prefetch_predictor_url";
const char kUrlRedirectTableName[] = "resource_prefetch_predictor_url_redirect";
const char kHostResourceTableName[] = "resource_prefetch_predictor_host";
const char kHostRedirectTableName[] =
    "resource_prefetch_predictor_host_redirect";
const char kOriginTableName[] = "resource_prefetch_predictor_origin";

const char kCreateGlobalMetadataStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, value INTEGER, "
    "PRIMARY KEY (key))";
const char kCreateProtoTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, "
    "proto BLOB, "
    "PRIMARY KEY(key))";

int GetResourceTypeMultiplier(
    predictors::ResourceData::ResourceType resource_type) {
  switch (resource_type) {
    case predictors::ResourceData::RESOURCE_TYPE_STYLESHEET:
      return 4;
    case predictors::ResourceData::RESOURCE_TYPE_SCRIPT:
      return 3;
    case predictors::ResourceData::RESOURCE_TYPE_FONT_RESOURCE:
      return 2;
    case predictors::ResourceData::RESOURCE_TYPE_IMAGE:
    default:
      return 1;
  }
}

}  // namespace

namespace predictors {

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

// static
void ResourcePrefetchPredictorTables::TrimOrigins(
    OriginData* data,
    size_t max_consecutive_misses) {
  auto* origins = data->mutable_origins();
  auto new_end = std::remove_if(
      origins->begin(), origins->end(), [=](const OriginStat& x) {
        return x.consecutive_misses() >= max_consecutive_misses;
      });
  origins->erase(new_end, origins->end());
}

// static
void ResourcePrefetchPredictorTables::SortOrigins(
    OriginData* data,
    const std::string& main_frame_origin) {
  auto* origins = data->mutable_origins();
  auto it = std::find_if(origins->begin(), origins->end(),
                         [&main_frame_origin](const OriginStat& x) {
                           return x.origin() == main_frame_origin;
                         });
  int iterator_offset = 0;
  if (it != origins->end()) {
    origins->SwapElements(0, it - origins->begin());
    iterator_offset = 1;
  }
  std::sort(origins->begin() + iterator_offset, origins->end(),
            [](const OriginStat& x, const OriginStat& y) {
              // Decreasing score ordering.
              return ComputeOriginScore(x) > ComputeOriginScore(y);
            });
}

ResourcePrefetchPredictorTables::ResourcePrefetchPredictorTables(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : PredictorTableBase(db_task_runner) {
  url_resource_table_ = base::MakeUnique<GlowplugKeyValueTable<PrefetchData>>(
      kUrlResourceTableName);
  url_redirect_table_ = base::MakeUnique<GlowplugKeyValueTable<RedirectData>>(
      kUrlRedirectTableName);
  host_resource_table_ = base::MakeUnique<GlowplugKeyValueTable<PrefetchData>>(
      kHostResourceTableName);
  host_redirect_table_ = base::MakeUnique<GlowplugKeyValueTable<RedirectData>>(
      kHostRedirectTableName);
  origin_table_ =
      base::MakeUnique<GlowplugKeyValueTable<OriginData>>(kOriginTableName);
}

ResourcePrefetchPredictorTables::~ResourcePrefetchPredictorTables() = default;

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

  int type_multiplier = GetResourceTypeMultiplier(data.resource_type());

  constexpr int kMaxResourcesPerType = 100;
  return kMaxResourcesPerType *
             (priority_multiplier * 100 + type_multiplier * 10) -
         data.average_position();
}

// static
float ResourcePrefetchPredictorTables::ComputeOriginScore(
    const OriginStat& origin) {
  // The ranking is done by considering, in this order:
  // 1. High confidence resources (>75% and more than 10 hits)
  // 2. Mandatory network access
  // 3. Network accessed
  // 4. Average position (decreasing)
  float score = 0;
  float confidence = static_cast<float>(origin.number_of_hits()) /
                     (origin.number_of_hits() + origin.number_of_misses());

  bool is_high_confidence = confidence > .75 && origin.number_of_hits() > 10;
  score += is_high_confidence * 1e6;

  score += origin.always_access_network() * 1e4;
  score += origin.accessed_network() * 1e2;
  score += 1e2 - origin.average_position();

  return score;
}

void ResourcePrefetchPredictorTables::ScheduleDBTask(
    const base::Location& from_here,
    DBTask task) {
  GetTaskRunner()->PostTask(
      from_here,
      base::BindOnce(
          &ResourcePrefetchPredictorTables::ExecuteDBTaskOnDBSequence, this,
          std::move(task)));
}

void ResourcePrefetchPredictorTables::ExecuteDBTaskOnDBSequence(DBTask task) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  std::move(task).Run(DB());
}

GlowplugKeyValueTable<PrefetchData>*
ResourcePrefetchPredictorTables::url_resource_table() {
  return url_resource_table_.get();
}
GlowplugKeyValueTable<RedirectData>*
ResourcePrefetchPredictorTables::url_redirect_table() {
  return url_redirect_table_.get();
}
GlowplugKeyValueTable<PrefetchData>*
ResourcePrefetchPredictorTables::host_resource_table() {
  return host_resource_table_.get();
}
GlowplugKeyValueTable<RedirectData>*
ResourcePrefetchPredictorTables::host_redirect_table() {
  return host_redirect_table_.get();
}
GlowplugKeyValueTable<OriginData>*
ResourcePrefetchPredictorTables::origin_table() {
  return origin_table_.get();
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
  static const char kManifestTableName[] =
      "resource_prefetch_predictor_manifest";

  if (incompatible_version) {
    for (const char* table_name :
         {kMetadataTableName, kUrlResourceTableName, kHostResourceTableName,
          kUrlRedirectTableName, kHostRedirectTableName, kManifestTableName,
          kUrlMetadataTableName, kHostMetadataTableName, kOriginTableName}) {
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
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  // Database initialization is all-or-nothing.
  sql::Connection* db = DB();
  bool success = db->BeginTransaction();
  success = success && DropTablesIfOutdated(db);

  for (const char* table_name :
       {kUrlResourceTableName, kHostResourceTableName, kUrlRedirectTableName,
        kHostRedirectTableName, kOriginTableName}) {
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
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
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

}  // namespace predictors
