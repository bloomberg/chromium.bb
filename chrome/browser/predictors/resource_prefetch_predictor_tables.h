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
#include "chrome/browser/predictors/predictor_table_base.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.pb.h"
#include "components/precache/core/proto/precache.pb.h"

namespace sql {
class Statement;
}

namespace predictors {

// From resource_prefetch_predictor.proto.
using RedirectStat = RedirectData_RedirectStat;

// Interface for database tables used by the ResourcePrefetchPredictor.
// All methods except the constructor and destructor need to be called on the DB
// thread.
//
// Currently manages:
//  - UrlResourceTable - resources per Urls.
//  - UrlRedirectTable - redirects per Urls.
//  - HostResourceTable - resources per host.
//  - HostRedirectTable - redirects per host.
class ResourcePrefetchPredictorTables : public PredictorTableBase {
 public:
  typedef std::map<std::string, PrefetchData> PrefetchDataMap;
  typedef std::map<std::string, RedirectData> RedirectDataMap;
  typedef std::map<std::string, precache::PrecacheManifest> ManifestDataMap;
  typedef std::map<std::string, OriginData> OriginDataMap;

  // Returns data for all Urls and Hosts.
  virtual void GetAllData(PrefetchDataMap* url_data_map,
                          PrefetchDataMap* host_data_map,
                          RedirectDataMap* url_redirect_data_map,
                          RedirectDataMap* host_redirect_data_map,
                          ManifestDataMap* manifest_map,
                          OriginDataMap* origin_data_map);

  // Updates data for a Url and a host. If any of the arguments has an empty
  // primary key, it will be ignored.
  // Note that all the keys should be less |kMaxStringLength| in length.
  virtual void UpdateData(const PrefetchData& url_data,
                          const PrefetchData& host_data,
                          const RedirectData& url_redirect_data,
                          const RedirectData& host_redirect_data);

  // Updates manifest data for the input |host|.
  virtual void UpdateManifestData(
      const std::string& host,
      const precache::PrecacheManifest& manifest_data);

  // Updates a given origin data point.
  virtual void UpdateOriginData(const OriginData& origin_data);

  // Delete data for the input |urls| and |hosts|.
  virtual void DeleteResourceData(const std::vector<std::string>& urls,
                                  const std::vector<std::string>& hosts);

  // Wrapper over DeleteResourceData for convenience.
  virtual void DeleteSingleResourceDataPoint(const std::string& key,
                                             PrefetchKeyType key_type);

  // Delete data for the input |urls| and |hosts|.
  virtual void DeleteRedirectData(const std::vector<std::string>& urls,
                                  const std::vector<std::string>& hosts);

  // Wrapper over DeleteRedirectData for convenience.
  virtual void DeleteSingleRedirectDataPoint(const std::string& key,
                                             PrefetchKeyType key_type);

  // Delete data for the input |hosts|.
  virtual void DeleteManifestData(const std::vector<std::string>& hosts);

  // Deletes the origin data for a list of |hosts|.
  virtual void DeleteOriginData(const std::vector<std::string>& hosts);

  // Deletes all data in all the tables.
  virtual void DeleteAllData();

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

  // Computes score of |data|.
  static float ComputeOriginScore(const OriginStat& origin);

  // The maximum length of the string that can be stored in the DB.
  static constexpr size_t kMaxStringLength = 1024;

 protected:
  // Protected for testing. Use PredictorDatabase::resource_prefetch_tables()
  // instead of this constructor.
  ResourcePrefetchPredictorTables();
  ~ResourcePrefetchPredictorTables() override;

 private:
  // Represents the type of information that is stored in prefetch database.
  enum class PrefetchDataType { RESOURCE, REDIRECT, MANIFEST, ORIGIN };

  enum class TableOperationType { INSERT, REMOVE };

  friend class PredictorDatabaseInternal;
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTablesTest,
                           DatabaseVersionIsSet);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTablesTest,
                           DatabaseIsResetWhenIncompatible);

  // Database version. Always increment it when any change is made to the data
  // schema (including the .proto).
  static constexpr int kDatabaseVersion = 7;

  // Helper functions below help perform functions on the Url and host table
  // using the same code.
  void GetAllResourceDataHelper(PrefetchKeyType key_type,
                                PrefetchDataMap* data_map);
  void GetAllRedirectDataHelper(PrefetchKeyType key_type,
                                RedirectDataMap* redirect_map);
  void GetAllManifestDataHelper(ManifestDataMap* manifest_map);
  void GetAllOriginDataHelper(OriginDataMap* manifest_map);

  bool UpdateDataHelper(PrefetchKeyType key_type,
                        PrefetchDataType data_type,
                        const std::string& key,
                        const google::protobuf::MessageLite& data);
  void DeleteDataHelper(PrefetchKeyType key_type,
                        PrefetchDataType data_type,
                        const std::vector<std::string>& keys);

  // PredictorTableBase:
  void CreateTableIfNonExistent() override;
  void LogDatabaseStats() override;

  static bool DropTablesIfOutdated(sql::Connection* db);
  static int GetDatabaseVersion(sql::Connection* db);
  static bool SetDatabaseVersion(sql::Connection* db, int version);

  // Helper to return cached Statements.
  std::unique_ptr<sql::Statement> GetTableUpdateStatement(
      PrefetchKeyType key_type,
      PrefetchDataType data_type,
      TableOperationType op_type);

  static const char* GetTableName(PrefetchKeyType key_type,
                                  PrefetchDataType data_type);

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorTables);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
