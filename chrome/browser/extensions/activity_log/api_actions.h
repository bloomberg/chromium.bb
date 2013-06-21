// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_API_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_API_ACTIONS_H_

#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

// This class describes API calls that did not run into permissions or quota
// problems.  See BlockedActions for API calls that did not succeed.
class APIAction : public Action {
 public:
  // These values should not be changed. Append any additional values to the
  // end with sequential numbers.
  enum Type {
    CALL = 0,
    EVENT_CALLBACK = 1,
    UNKNOWN_TYPE = 2,
  };

  static const char* kTableName;
  static const char* kTableContentFields[];
  static const char* kTableFieldTypes[];
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
  virtual bool Record(sql::Connection* db) OVERRIDE;

  virtual scoped_ptr<api::activity_log_private::ExtensionActivity>
      ConvertToExtensionActivity() OVERRIDE;

  // Used to associate tab IDs with URLs. It will swap out the int in args with
  // a URL as a string. If the tab is in incognito mode, we leave it alone as
  // the original int. There is a small chance that the URL translation could
  // be wrong, if the tab has already been navigated by the time of invocation.
  static void LookupTabId(const std::string& api_call,
                          base::ListValue* args,
                          Profile* profile);

  // Print a APIAction as a regular string for debugging purposes.
  virtual std::string PrintForDebug() OVERRIDE;

  // Helper methods for recording the values into the db.
  const std::string& api_call() const { return api_call_; }
  const std::string& args() const { return args_; }
  std::string TypeAsString() const;
  std::string extra() const { return extra_; }

 protected:
  virtual ~APIAction();

 private:
  Type type_;
  std::string api_call_;
  std::string args_;
  std::string extra_;

  DISALLOW_COPY_AND_ASSIGN(APIAction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_API_ACTIONS_H_
