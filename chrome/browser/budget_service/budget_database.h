// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BUDGET_SERVICE_BUDGET_DATABASE_H_
#define CHROME_BROWSER_BUDGET_SERVICE_BUDGET_DATABASE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/proto_database.h"

namespace base {
class SequencedTaskRunner;
}

namespace budget_service {
class Budget;
}

class GURL;

// A class used to asynchronously read and write details of the budget
// assigned to an origin. The class uses an underlying LevelDB.
class BudgetDatabase {
 public:
  // Callback for the basic GetBudget call.
  using GetValueCallback =
      base::Callback<void(bool success,
                          std::unique_ptr<budget_service::Budget>)>;

  // Callback for setting a budget value.
  using SetValueCallback = base::Callback<void(bool success)>;

  // The database_dir specifies the location of the budget information on
  // disk. The task_runner is used by the ProtoDatabase to handle all blocking
  // calls and disk access.
  BudgetDatabase(const base::FilePath& database_dir,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~BudgetDatabase();

  void GetValue(const GURL& origin, const GetValueCallback& callback);
  void SetValue(const GURL& origin,
                const budget_service::Budget& budget,
                const SetValueCallback& callback);

 private:
  void OnDatabaseInit(bool success);

  // The database for storing budget information.
  std::unique_ptr<leveldb_proto::ProtoDatabase<budget_service::Budget>> db_;

  base::WeakPtrFactory<BudgetDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BudgetDatabase);
};

#endif  // CHROME_BROWSER_BUDGET_SERVICE_BUDGET_DATABASE_H_
