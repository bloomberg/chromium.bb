// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_DATABASE_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_DATABASE_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/synchronization/lock.h"
#include "base/timer.h"
#include "chrome/browser/extensions/api_actions.h"
#include "chrome/browser/extensions/blocked_actions.h"
#include "chrome/browser/extensions/dom_actions.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "sql/connection.h"
#include "sql/init_status.h"

namespace base {
class Clock;
class FilePath;
}

namespace extensions {

// Encapsulates the SQL connection for the activity log database.
// This class holds the database connection and has methods for writing.
// All of the methods except constructor and SetErrorDelegate need to be
// called on the DB thread. For this reason, the ActivityLog calls Close from
// its destructor instead of destructing its ActivityDatabase object.
class ActivityDatabase {
 public:
  // Need to call Init to actually use the ActivityDatabase.
  ActivityDatabase();

  // Sets up an optional error delegate.
  // Should be the only thing done before Init.
  void SetErrorDelegate(sql::ErrorDelegate* error_delegate);

  // Opens the DB and creates tables as necessary.
  void Init(const base::FilePath& db_name);

  // The ActivityLog should call this to kill the ActivityDatabase.
  void Close();

  void LogInitFailure();

  // Record a DOMction in the database.
  void RecordDOMAction(scoped_refptr<DOMAction> action);

  // Record a APIAction in the database.
  void RecordAPIAction(scoped_refptr<APIAction> action);

  // Record a BlockedAction in the database.
  void RecordBlockedAction(scoped_refptr<BlockedAction> action);

  // Record an Action in the database.
  void RecordAction(scoped_refptr<Action> action);

  // Gets all actions for a given extension for the specified day. 0 = today,
  // 1 = yesterday, etc. Only returns 1 day at a time. Actions are sorted from
  // newest to oldest.
  scoped_ptr<std::vector<scoped_refptr<Action> > > GetActions(
      const std::string& extension_id, const int days_ago);

  // Break any outstanding transactions, raze the database, and close
  // it.  Future calls on the current database handle will fail, when
  // next opened the database will be empty. This is the ugly version of Close.
  void KillDatabase();

  bool initialized() const { return initialized_; }

  // Standard db operation wrappers.
  void BeginTransaction();
  void CommitTransaction();
  void RollbackTransaction();
  bool Raze();

  // For unit testing only.
  void SetBatchModeForTesting(bool batch_mode);
  void SetClockForTesting(base::Clock* clock);
  void SetTimerForTesting(int milliseconds);

 private:
  // This should never be invoked by another class. Use Close() to order a
  // suicide.
  virtual ~ActivityDatabase();

  sql::InitStatus InitializeTable(const char* table_name,
                                  const char* table_structure);

  // When we're in batched mode (which is on by default), we write to the db
  // every X minutes instead of on every API call. This prevents the annoyance
  // of writing to disk multiple times a second.
  void StartTimer();
  void RecordBatchedActions();

  // For unit testing only.
  void RecordBatchedActionsWhileTesting();

  base::Clock* testing_clock_;
  sql::Connection db_;
  bool initialized_;
  bool batch_mode_;
  std::vector<scoped_refptr<Action> > batched_actions_;
  base::RepeatingTimer<ActivityDatabase> timer_;

  DISALLOW_COPY_AND_ASSIGN(ActivityDatabase);
};

}  // namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_DATABASE_H_
