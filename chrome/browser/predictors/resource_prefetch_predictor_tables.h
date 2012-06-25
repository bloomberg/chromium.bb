// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
#pragma once

#include "chrome/browser/predictors/predictor_table_base.h"

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"
#include "webkit/glue/resource_type.h"

namespace predictors {

// Interface for database tables used by the ResourcePrefetchPredictor.
// All methods except the constructor and destructor need to be called on the DB
// thread.
// NOTE: This class is named in plural as it will hold other tables shortly.
//
// TODO(shishir): Move to composition model instead of implemented inheritance.
class ResourcePrefetchPredictorTables : public PredictorTableBase {
 public:
  struct UrlTableRow {
    UrlTableRow();
    UrlTableRow(const UrlTableRow& other);
    UrlTableRow(const std::string& main_frame_url,
                const std::string& resource_url,
                ResourceType::Type resource_type,
                int number_of_hits,
                int number_of_misses,
                int consecutive_misses,
                double average_position);
    void UpdateScore();
    bool operator==(const UrlTableRow& rhs) const;

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
  typedef std::vector<UrlTableRow> UrlTableRows;

  // Sorts the UrlTableRows by score, descending.
  struct UrlTableRowSorter {
    bool operator()(const UrlTableRow& x, const UrlTableRow& y) const;
  };

  // |url_row_buffer| should be empty for GetAllRows.
  virtual void GetAllRows(UrlTableRows* url_row_buffer);
  virtual void UpdateRowsForUrl(const GURL& main_page_url,
                                const UrlTableRows& row_buffer);
  virtual void DeleteRowsForUrls(const std::vector<GURL>& urls);
  virtual void DeleteAllRows();

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
