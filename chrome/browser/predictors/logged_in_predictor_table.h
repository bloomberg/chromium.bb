// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOGGED_IN_PREDICTOR_TABLE_H_
#define CHROME_BROWSER_PREDICTORS_LOGGED_IN_PREDICTOR_TABLE_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/predictor_table_base.h"
#include "url/gurl.h"

namespace sql {
class Statement;
}

namespace predictors {

// Interface for database table to keep track of what sites a user is logged
// in to.
// All methods except the constructor and destructor need to be called on the DB
// thread.
// Manages one table { domain (primary key), added_timestamp }.
class LoggedInPredictorTable : public PredictorTableBase {
 public:
  typedef base::hash_map<std::string, int64> LoggedInStateMap;

  // Adds the relevant part of the domain of the URL provided to the database
  // as the user having performed a login action.
  void AddDomainFromURL(const GURL& url);
  // Deletes a record for the domain corresponding to the URL provided.
  void DeleteDomainFromURL(const GURL& url);
  // Deletes a record for the domain provided.
  void DeleteDomain(const std::string& domain);
  // Checks whether for the relevant part of the domain of the URL provided,
  // the user has performed a login action in the past.
  void HasUserLoggedIn(const GURL& url, bool* is_present,
                       bool* lookup_succeeded);
  void DeleteAllCreatedBetween(const base::Time& delete_begin,
                               const base::Time& delete_end);
  void GetAllData(LoggedInStateMap* state_map);

  static std::string GetKey(const GURL& url);
  static std::string GetKeyFromDomain(const std::string& domain);

 private:
  friend class PredictorDatabaseInternal;

  LoggedInPredictorTable();
  virtual ~LoggedInPredictorTable();

  // PredictorTableBase methods.
  virtual void CreateTableIfNonExistent() OVERRIDE;
  virtual void LogDatabaseStats() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(LoggedInPredictorTable);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOGGED_IN_PREDICTOR_TABLE_H_
