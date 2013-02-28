// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTIONS_H_

#include "chrome/browser/extensions/activity_actions.h"

namespace extensions {

// This class describes API calls that ran into permissions or quota problems.
// See APIActions for API calls that succeeded.
class BlockedAction : public Action {
 public:
  static const char* kTableName;
  static const char* kTableContentFields[];

  // Create a new database table for storing BlockedActions, or update the
  // schema if it is out of date. Any existing data is preserved.
  static bool InitializeTable(sql::Connection* db);

  // You must supply the id, time, api_call, and reason.
  BlockedAction(const std::string& extension_id,
                const base::Time& time,
                const std::string& api_call,          // the blocked API call
                const std::string& args,              // the arguments
                const std::string& reason,            // the reason it's blocked
                const std::string& extra);            // any extra logging info

  // Create a new BlockedAction from a database row.
  explicit BlockedAction(const sql::Statement& s);

  // Record the action in the database.
  virtual void Record(sql::Connection* db) OVERRIDE;

  // Print a BlockedAction with il8n substitutions for display.
  virtual std::string PrettyPrintFori18n() OVERRIDE;

  // Print a BlockedAction as a regular string for debugging purposes.
  virtual std::string PrettyPrintForDebug() OVERRIDE;

  // Helper methods for recording the values into the db.
  const std::string& reason() const { return reason_; }
  const std::string& api_call() const { return api_call_; }
  const std::string& args() const { return args_; }
  const std::string& extra() const { return extra_; }

 protected:
  virtual ~BlockedAction();

 private:
  std::string api_call_;
  std::string args_;
  std::string reason_;
  std::string extra_;

  DISALLOW_COPY_AND_ASSIGN(BlockedAction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTIONS_H_

