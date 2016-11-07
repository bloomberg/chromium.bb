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
  // Map from primary key to PrefetchData for the key.
  typedef std::map<std::string, PrefetchData> PrefetchDataMap;

  // Map from primary key to RedirectData for the key.
  typedef std::map<std::string, RedirectData> RedirectDataMap;

  // Returns data for all Urls and Hosts.
  virtual void GetAllData(PrefetchDataMap* url_data_map,
                          PrefetchDataMap* host_data_map,
                          RedirectDataMap* url_redirect_data_map,
                          RedirectDataMap* host_redirect_data_map);

  // Updates data for a Url and a host. If either of the |url_data| or
  // |host_data| or |url_redirect_data| or |host_redirect_data| has an empty
  // primary key, it will be ignored.
  // Note that the Urls and primary key in |url_data|, |host_data|,
  // |url_redirect_data| and |host_redirect_data| should be less than
  // |kMaxStringLength| in length.
  virtual void UpdateData(const PrefetchData& url_data,
                          const PrefetchData& host_data,
                          const RedirectData& url_redirect_data,
                          const RedirectData& host_redirect_data);

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

  // The maximum length of the string that can be stored in the DB.
  static constexpr size_t kMaxStringLength = 1024;

 protected:
  // Protected for testing. Use PredictorDatabase::resource_prefetch_tables()
  // instead of this constructor.
  ResourcePrefetchPredictorTables();
  ~ResourcePrefetchPredictorTables() override;

 private:
  // Represents the type of information that is stored in prefetch database.
  enum class PrefetchDataType { RESOURCE, REDIRECT };

  enum class TableOperationType { INSERT, REMOVE };

  friend class PredictorDatabaseInternal;
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTablesTest,
                           DatabaseVersionIsSet);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTablesTest,
                           DatabaseIsResetWhenIncompatible);

  // Database version. Always increment it when any change is made to the data
  // schema (including the .proto).
  static constexpr int kDatabaseVersion = 5;

  // Helper functions below help perform functions on the Url and host table
  // using the same code.
  void GetAllResourceDataHelper(PrefetchKeyType key_type,
                                PrefetchDataMap* data_map);
  void GetAllRedirectDataHelper(PrefetchKeyType key_type,
                                RedirectDataMap* redirect_map);
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
