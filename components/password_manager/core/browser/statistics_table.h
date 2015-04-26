// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STATISTICS_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STATISTICS_TABLE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace sql {
class Connection;
}

namespace password_manager {

// The statistics containing user interactions with a site.
struct InteractionsStats {
  // The domain of the site.
  GURL origin_domain;

  // Number of times the user clicked "Don't save the password".
  int nopes_count;

  // Number of times the user dismissed the bubble.
  int dismissal_count;

  // The beginning date of the measurements.
  base::Time start_date;
};

// Represents 'stats' table in the Login Database.
class StatisticsTable {
 public:
  StatisticsTable();
  ~StatisticsTable();

  // Initializes |db_| and creates the statistics table if it doesn't exist.
  bool Init(sql::Connection* db);

  // Adds or replaces the statistics about |stats.origin_domain|.
  bool AddRow(const InteractionsStats& stats);

  // Removes the statistics for |domain|. Returns true if the SQL completed
  // successfully.
  bool RemoveRow(const GURL& domain);

  // Returns the statistics for |domain| if it exists.
  scoped_ptr<InteractionsStats> GetRow(const GURL& domain);

 private:
  sql::Connection* db_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsTable);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STATISTICS_TABLE_H_
