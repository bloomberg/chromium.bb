// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_


#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/predictors/predictor_table_base.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace sql {
class Statement;
}

namespace predictors {

// Interface for database tables used by the ResourcePrefetchPredictor.
// All methods except the constructor and destructor need to be called on the DB
// thread.
//
// Currently manages:
//  - UrlResourceTable - resources per Urls.
//  - UrlMetadataTable - misc data for Urls (like last visit time).
//  - HostResourceTable - resources per host.
//  - HostMetadataTable - misc data for hosts.
class ResourcePrefetchPredictorTables : public PredictorTableBase {
 public:
  // Used in the UrlResourceTable and HostResourceTable to store resources
  // required for the page or host.
  struct ResourceRow {
    ResourceRow();
    ResourceRow(const ResourceRow& other);
    ResourceRow(const std::string& main_frame_url,
                const std::string& resource_url,
                content::ResourceType resource_type,
                int number_of_hits,
                int number_of_misses,
                int consecutive_misses,
                double average_position);
    void UpdateScore();
    bool operator==(const ResourceRow& rhs) const;

    // Stores the host for host based data, main frame Url for the Url based
    // data. This field is cleared for efficiency reasons and the code outside
    // this class should not assume it is set.
    std::string primary_key;

    GURL resource_url;
    content::ResourceType resource_type;
    size_t number_of_hits;
    size_t number_of_misses;
    size_t consecutive_misses;
    double average_position;

    // Not stored.
    float score;
  };
  typedef std::vector<ResourceRow> ResourceRows;

  // Sorts the ResourceRows by score, descending.
  struct ResourceRowSorter {
    bool operator()(const ResourceRow& x, const ResourceRow& y) const;
  };

  // Aggregated data for a Url or Host. Although the data differs slightly, we
  // store them in the same structure, because most of the fields are common and
  // it allows us to use the same functions.
  struct PrefetchData {
    PrefetchData(PrefetchKeyType key_type, const std::string& primary_key);
    PrefetchData(const PrefetchData& other);
    ~PrefetchData();
    bool operator==(const PrefetchData& rhs) const;

    bool is_host() const { return key_type == PREFETCH_KEY_TYPE_HOST; }

    // Is the data a host as opposed to a Url?
    PrefetchKeyType key_type;  // Not const to be able to assign.
    std::string primary_key;  // is_host() ? main frame url : host.

    base::Time last_visit;
    ResourceRows resources;
  };
  // Map from primary key to PrefetchData for the key.
  typedef std::map<std::string, PrefetchData> PrefetchDataMap;

  // Returns data for all Urls and Hosts.
  virtual void GetAllData(PrefetchDataMap* url_data_map,
                          PrefetchDataMap* host_data_map);

  // Updates data for a Url and a host. If either of the |url_data| or
  // |host_data| has an empty primary key, it will be ignored.
  // Note that the Urls and primary key in |url_data| and |host_data| should be
  // less than |kMaxStringLength| in length.
  virtual void UpdateData(const PrefetchData& url_data,
                          const PrefetchData& host_data);

  // Delete data for the input |urls| and |hosts|.
  virtual void DeleteData(const std::vector<std::string>& urls,
                  const std::vector<std::string>& hosts);

  // Wrapper over DeleteData for convenience.
  virtual void DeleteSingleDataPoint(const std::string& key,
                                     PrefetchKeyType key_type);

  // Deletes all data in all the tables.
  virtual void DeleteAllData();

  // The maximum length of the string that can be stored in the DB.
  static const size_t kMaxStringLength;

 private:
  friend class PredictorDatabaseInternal;
  friend class MockResourcePrefetchPredictorTables;

  ResourcePrefetchPredictorTables();
  virtual ~ResourcePrefetchPredictorTables();

  // Helper functions below help perform functions on the Url and host table
  // using the same code.
  void GetAllDataHelper(PrefetchKeyType key_type,
                        PrefetchDataMap* data_map,
                        std::vector<std::string>* to_delete);
  bool UpdateDataHelper(const PrefetchData& data);
  void DeleteDataHelper(PrefetchKeyType key_type,
                        const std::vector<std::string>& keys);

  // Returns true if the strings in the |data| are less than |kMaxStringLength|
  // in length.
  bool StringsAreSmallerThanDBLimit(const PrefetchData& data) const;

  // PredictorTableBase methods.
  virtual void CreateTableIfNonExistent() OVERRIDE;
  virtual void LogDatabaseStats() OVERRIDE;

  // Helpers to return Statements for cached Statements. The caller must take
  // ownership of the return Statements.
  sql::Statement* GetUrlResourceDeleteStatement();
  sql::Statement* GetUrlResourceUpdateStatement();
  sql::Statement* GetUrlMetadataDeleteStatement();
  sql::Statement* GetUrlMetadataUpdateStatement();

  sql::Statement* GetHostResourceDeleteStatement();
  sql::Statement* GetHostResourceUpdateStatement();
  sql::Statement* GetHostMetadataDeleteStatement();
  sql::Statement* GetHostMetadataUpdateStatement();

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorTables);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
