// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_

#include "chrome/browser/predictors/predictor_table_base.h"

#include <string>
#include <vector>

#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/resource_type.h"

namespace predictors {

// Interface for database tables used by the ResourcePrefetchPredictor.
// All methods except the constructor and destructor need to be called on the DB
// thread.
//
// Currently manages two tables:
//  - UrlResourceTable - Keeps track of resources per Urls.
//  - UrlMetadataTable - Keeps misc data for Urls (like last visit time).
class ResourcePrefetchPredictorTables : public PredictorTableBase {
 public:
  struct UrlResourceRow {
    UrlResourceRow();
    UrlResourceRow(const UrlResourceRow& other);
    UrlResourceRow(const std::string& main_frame_url,
                   const std::string& resource_url,
                   ResourceType::Type resource_type,
                   int number_of_hits,
                   int number_of_misses,
                   int consecutive_misses,
                   double average_position);
    void UpdateScore();
    bool operator==(const UrlResourceRow& rhs) const;

    GURL main_frame_url;
    GURL resource_url;
    ResourceType::Type resource_type;
    int number_of_hits;
    int number_of_misses;
    int consecutive_misses;
    double average_position;

    // Not stored.
    float score;
  };
  typedef std::vector<UrlResourceRow> UrlResourceRows;

  // Sorts the UrlResourceRows by score, descending.
  struct UrlResourceRowSorter {
    bool operator()(const UrlResourceRow& x, const UrlResourceRow& y) const;
  };

  // Data from both the tables for a particular main frame url.
  struct UrlData {
    explicit UrlData(const GURL& main_frame_url);
    UrlData(const UrlData& other);
    ~UrlData();
    bool operator==(const UrlData& rhs) const;

    GURL main_frame_url;
    base::Time last_visit;
    UrlResourceRows resources;
  };

  // |url_data_buffer| should be empty for GetAllRows.
  virtual void GetAllUrlData(std::vector<UrlData>* url_data_buffer);
  virtual void UpdateDataForUrl(const UrlData& url_data);
  virtual void DeleteDataForUrls(const std::vector<GURL>& urls);
  virtual void DeleteAllUrlData();

 private:
  friend class PredictorDatabaseInternal;
  friend class MockResourcePrefetchPredictorTables;

  ResourcePrefetchPredictorTables();
  virtual ~ResourcePrefetchPredictorTables();

  // PredictorTableBase methods.
  virtual void CreateTableIfNonExistent() OVERRIDE;
  virtual void LogDatabaseStats() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorTables);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
