// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/predictors/glowplug_key_value_table.h"
#include "chrome/browser/predictors/predictor_table_base.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.pb.h"

namespace tracked_objects {
class Location;
}

namespace predictors {

// Interface for database tables used by the ResourcePrefetchPredictor.
// All methods except the ExecuteDBTaskOnDBSequence need to be called on the UI
// thread.
//
// Currently manages:
//  - UrlResourceTable - key: url, value: PrefetchData
//  - UrlRedirectTable - key: url, value: RedirectData
//  - HostResourceTable - key: host, value: PrefetchData
//  - HostRedirectTable - key: host, value: RedirectData
//  - OriginTable - key: host, value: OriginData
class ResourcePrefetchPredictorTables : public PredictorTableBase {
 public:
  typedef base::OnceCallback<void(sql::Connection*)> DBTask;

  virtual void ScheduleDBTask(const tracked_objects::Location& from_here,
                              DBTask task);

  virtual void ExecuteDBTaskOnDBSequence(DBTask task);

  virtual GlowplugKeyValueTable<PrefetchData>* url_resource_table();
  virtual GlowplugKeyValueTable<RedirectData>* url_redirect_table();
  virtual GlowplugKeyValueTable<PrefetchData>* host_resource_table();
  virtual GlowplugKeyValueTable<RedirectData>* host_redirect_table();
  virtual GlowplugKeyValueTable<OriginData>* origin_table();

  // Removes the resources with more than |max_consecutive_misses| consecutive
  // misses from |data|.
  static void TrimResources(PrefetchData* data, size_t max_consecutive_misses);

  // Sorts the resources by score, decreasing.
  static void SortResources(PrefetchData* data);

  // Computes score of |data|.
  static float ComputeResourceScore(const ResourceData& data);

  // Removes the redirects with more than |max_consecutive_misses| consecutive
  // misses from |data|.
  static void TrimRedirects(RedirectData* data, size_t max_consecutive_misses);

  // Removes the origins with more than |max_consecutive_misses| consecutive
  // misses from |data|.
  static void TrimOrigins(OriginData* data, size_t max_consecutive_misses);

  // Sorts the origins by score, decreasing.
  static void SortOrigins(OriginData* data);

  // Computes score of |origin|.
  static float ComputeOriginScore(const OriginStat& origin);

  // The maximum length of the string that can be stored in the DB.
  static constexpr size_t kMaxStringLength = 1024;

 protected:
  // Protected for testing. Use PredictorDatabase::resource_prefetch_tables()
  // instead of this constructor.
  ResourcePrefetchPredictorTables(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);
  ~ResourcePrefetchPredictorTables() override;

 private:
  friend class PredictorDatabaseInternal;
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTablesTest,
                           DatabaseVersionIsSet);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTablesTest,
                           DatabaseIsResetWhenIncompatible);

  // Database version. Always increment it when any change is made to the data
  // schema (including the .proto).
  static constexpr int kDatabaseVersion = 10;

  // PredictorTableBase:
  void CreateTableIfNonExistent() override;
  void LogDatabaseStats() override;

  static bool DropTablesIfOutdated(sql::Connection* db);
  static int GetDatabaseVersion(sql::Connection* db);
  static bool SetDatabaseVersion(sql::Connection* db, int version);

  std::unique_ptr<GlowplugKeyValueTable<PrefetchData>> url_resource_table_;
  std::unique_ptr<GlowplugKeyValueTable<RedirectData>> url_redirect_table_;
  std::unique_ptr<GlowplugKeyValueTable<PrefetchData>> host_resource_table_;
  std::unique_ptr<GlowplugKeyValueTable<RedirectData>> host_redirect_table_;
  std::unique_ptr<GlowplugKeyValueTable<OriginData>> origin_table_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorTables);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
