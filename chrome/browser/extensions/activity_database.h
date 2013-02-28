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
#include "chrome/browser/extensions/api_actions.h"
#include "chrome/browser/extensions/blocked_actions.h"
#include "chrome/browser/extensions/dom_actions.h"
#include "chrome/common/extensions/extension.h"
#include "sql/connection.h"
#include "sql/init_status.h"

namespace base {
class FilePath;
}

namespace extensions {

// Encapsulates the SQL connection for the activity log database.
// This class holds the database connection and has methods for writing.
class ActivityDatabase : public base::RefCountedThreadSafe<ActivityDatabase> {
 public:
  // Need to call Init to actually use the ActivityDatabase.
  ActivityDatabase();

  // Sets up an optional error delegate.
  // Should be the only thing done before Init.
  void SetErrorDelegate(sql::ErrorDelegate* error_delegate);

  // Opens the DB and creates tables as necessary.
  void Init(const base::FilePath& db_name);
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
  // next opened the database will be empty.
  void KillDatabase();

  bool initialized() const { return initialized_; }

  // Standard db operation wrappers.
  void BeginTransaction();
  void CommitTransaction();
  void RollbackTransaction();
  bool Raze();
  void Close();

 private:
  friend class base::RefCountedThreadSafe<ActivityDatabase>;

  virtual ~ActivityDatabase();

  sql::InitStatus InitializeTable(const char* table_name,
                                  const char* table_structure);

  sql::Connection db_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ActivityDatabase);
};

}  // namespace extensions
#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_DATABASE_H_
