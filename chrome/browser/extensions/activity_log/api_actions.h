// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_API_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_API_ACTIONS_H_

#include "chrome/browser/extensions/activity_log/activity_actions.h"

namespace extensions {

// This class describes API calls that did not run into permissions or quota
// problems.  See BlockedActions for API calls that did not succeed.
class APIAction : public Action {
 public:
  enum Type {
    CALL,
    EVENT_CALLBACK,
    UNKNOWN_TYPE
  };

  static const char* kTableName;
  static const char* kTableContentFields[];
  static const char* kAlwaysLog[];
  static const int kSizeAlwaysLog;

  // Create the database table for storing APIActions, or update the schema if
  // it is out of date.  Any existing data is preserved.
  static bool InitializeTable(sql::Connection* db);

  // Create a new APIAction to describe a successful API call.  All
  // parameters are required.
  APIAction(const std::string& extension_id,
            const base::Time& time,
            const Type type,              // e.g. "CALL"
            const std::string& api_call,  // full method name
            const std::string& args,      // the argument list
            const std::string& extra);    // any extra logging info

  // Create a new APIAction from a database row.
  explicit APIAction(const sql::Statement& s);

  // Record the action in the database.
  virtual void Record(sql::Connection* db) OVERRIDE;

  // Print a APIAction as a regular string for debugging purposes.
  virtual std::string PrintForDebug() OVERRIDE;

  // Helper methods for recording the values into the db.
  const std::string& api_call() const { return api_call_; }
  const std::string& args() const { return args_; }
  std::string TypeAsString() const;
  std::string extra() const { return extra_; }

  // Helper method(s) for creating a APIAction.
  static Type StringAsType(const std::string& str);

 protected:
  virtual ~APIAction();

 private:
  Type type_;
  std::string api_call_;
  std::string args_;
  std::string extra_;

  DISALLOW_COPY_AND_ASSIGN(APIAction);
};

}  // namespace

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_API_ACTIONS_H_
