// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_BLOCKED_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_BLOCKED_ACTIONS_H_

#include "chrome/browser/extensions/activity_log/activity_actions.h"

namespace extensions {

// This class describes API calls that ran into permissions or quota problems.
// See APIActions for API calls that succeeded.
class BlockedAction : public Action {
 public:
  // These values should not be changed. Append any additional values to the
  // end with sequential numbers.
  enum Reason {
      UNKNOWN = 0,
      ACCESS_DENIED = 1,
      QUOTA_EXCEEDED = 2,
  };

  static const char* kTableName;
  static const char* kTableContentFields[];
  static const char* kTableFieldTypes[];

  // Create a new database table for storing BlockedActions, or update the
  // schema if it is out of date. Any existing data is preserved.
  static bool InitializeTable(sql::Connection* db);

  // You must supply the id, time, api_call, and reason.
  BlockedAction(const std::string& extension_id,
                const base::Time& time,
                const std::string& api_call,          // the blocked API call
                const std::string& args,              // the arguments
                const Reason reason,                  // the reason it's blocked
                const std::string& extra);            // any extra logging info

  // Create a new BlockedAction from a database row.
  explicit BlockedAction(const sql::Statement& s);

  // Record the action in the database.
  virtual bool Record(sql::Connection* db) OVERRIDE;

  virtual scoped_ptr<api::activity_log_private::ExtensionActivity>
      ConvertToExtensionActivity() OVERRIDE;

  // Print a BlockedAction as a string for debugging purposes.
  virtual std::string PrintForDebug() OVERRIDE;

  // Helper methods for recording the values into the db.
  const std::string& api_call() const { return api_call_; }
  const std::string& args() const { return args_; }
  const std::string& extra() const { return extra_; }

  // Helper method for debugging.
  std::string ReasonAsString() const;

 protected:
  virtual ~BlockedAction();

 private:
  std::string api_call_;
  std::string args_;
  Reason reason_;
  std::string extra_;

  DISALLOW_COPY_AND_ASSIGN(BlockedAction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_BLOCKED_ACTIONS_H_

