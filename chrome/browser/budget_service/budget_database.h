// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BUDGET_SERVICE_BUDGET_DATABASE_H_
#define CHROME_BROWSER_BUDGET_SERVICE_BUDGET_DATABASE_H_

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/proto_database.h"

namespace base {
class Clock;
class SequencedTaskRunner;
class Time;
}

namespace budget_service {
class Budget;
}

class GURL;

// A class used to asynchronously read and write details of the budget
// assigned to an origin. The class uses an underlying LevelDB.
class BudgetDatabase {
 public:
  // Data structure for returing the budget decay expectations to the caller.
  using BudgetExpectation = std::list<std::pair<double, base::Time>>;

  // Callback for setting a budget value.
  using StoreBudgetCallback = base::Callback<void(bool success)>;

  // Callback for getting a list of all budget chunks.
  using GetBudgetDetailsCallback = base::Callback<
      void(bool success, double debt, const BudgetExpectation& expectation)>;

  // The database_dir specifies the location of the budget information on
  // disk. The task_runner is used by the ProtoDatabase to handle all blocking
  // calls and disk access.
  BudgetDatabase(const base::FilePath& database_dir,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~BudgetDatabase();

  // Get the full budget expectation for the origin. This will return any
  // debt as well as a sequence of time points and the expected budget at
  // those times.
  void GetBudgetDetails(const GURL& origin,
                        const GetBudgetDetailsCallback& callback);

  // Add budget for an origin. The caller specifies the amount, and the method
  // adds the amount to the cache with the correct expiration. Callback is
  // invoked only after the newly cached value is written to storage. This
  // should only be called after the budget has been read from the database.
  void AddBudget(const GURL& origin,
                 double amount,
                 const StoreBudgetCallback& callback);

 private:
  friend class BudgetDatabaseTest;

  // Used to allow tests to change time for testing.
  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

  // Data structure for caching budget information.
  using BudgetChunks = std::vector<std::pair<double, base::Time>>;
  using BudgetInfo = std::pair<double, BudgetChunks>;

  using AddToCacheCallback = base::Callback<void(bool success)>;

  void OnDatabaseInit(bool success);

  bool IsCached(const GURL& origin) const;

  void AddToCache(const GURL& origin,
                  const AddToCacheCallback& callback,
                  bool success,
                  std::unique_ptr<budget_service::Budget> budget);

  void DidGetBudget(const GURL& origin,
                    const GetBudgetDetailsCallback& callback,
                    bool success);

  void WriteCachedValuesToDatabase(const GURL& origin,
                                   const StoreBudgetCallback& callback);

  void CleanupExpiredBudget(const GURL& origin);

  // The database for storing budget information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<budget_service::Budget>> db_;

  // Cached data for the origins which have been loaded.
  std::unordered_map<std::string, BudgetInfo> budget_map_;

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

  base::WeakPtrFactory<BudgetDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BudgetDatabase);
};

#endif  // CHROME_BROWSER_BUDGET_SERVICE_BUDGET_DATABASE_H_
