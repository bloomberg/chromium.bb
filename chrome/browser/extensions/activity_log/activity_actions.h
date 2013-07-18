// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTIONS_H_

#include <string>
#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/extensions/api/activity_log_private.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace extensions {

// This is the interface for extension actions that are to be recorded in
// the activity log.
class Action : public base::RefCountedThreadSafe<Action> {
 public:
  // Types of log entries that can be stored.  The numeric values are stored in
  // the database, so keep them stable.  Append values only.
  enum ActionType {
    ACTION_API_CALL = 0,
    ACTION_API_EVENT = 1,
    ACTION_API_BLOCKED = 2,
    ACTION_CONTENT_SCRIPT = 3,
    ACTION_DOM_ACCESS = 4,
    ACTION_DOM_EVENT = 5,
    ACTION_DOM_XHR = 6,
    ACTION_WEB_REQUEST = 7,
  };

  // Record the action in the database.
  virtual bool Record(sql::Connection* db) = 0;

  // Flatten the activity's type-specific fields into an ExtensionActivity.
  virtual scoped_ptr<api::activity_log_private::ExtensionActivity>
      ConvertToExtensionActivity() = 0;

  // Print an action as a regular string for debugging purposes.
  virtual std::string PrintForDebug() = 0;

  const std::string& extension_id() const { return extension_id_; }
  const base::Time& time() const { return time_; }
  api::activity_log_private::ExtensionActivity::ActivityType activity_type()
      const { return activity_type_; }

 protected:
  Action(const std::string& extension_id,
         const base::Time& time,
         api::activity_log_private::ExtensionActivity::ActivityType type);
  virtual ~Action() {}

 private:
  friend class base::RefCountedThreadSafe<Action>;

  std::string extension_id_;
  base::Time time_;
  api::activity_log_private::ExtensionActivity::ActivityType activity_type_;

  DISALLOW_COPY_AND_ASSIGN(Action);
};

// TODO(mvrable): This is a temporary class used to represent Actions which
// have been loaded from the SQLite database.  Soon the entire Action hierarchy
// will be flattened out as the type-specific classes are eliminated, at which
// time some of the logic here will be moved.
class WatchdogAction : public Action {
 public:
  WatchdogAction(const std::string& extension_id,
                 const base::Time& time,
                 const ActionType action_type,
                 const std::string& api_name,  // full method name
                 scoped_ptr<ListValue> args,  // the argument list
                 const GURL& page_url,  // page the action occurred on
                 const GURL& arg_url,  // URL extracted from the argument list
                 scoped_ptr<DictionaryValue> other);  // any extra logging info

  virtual bool Record(sql::Connection* db) OVERRIDE;
  virtual scoped_ptr<api::activity_log_private::ExtensionActivity>
      ConvertToExtensionActivity() OVERRIDE;
  virtual std::string PrintForDebug() OVERRIDE;

 protected:
  virtual ~WatchdogAction();

 private:
  ActionType action_type_;
  std::string api_name_;
  scoped_ptr<ListValue> args_;
  GURL page_url_;
  GURL arg_url_;
  scoped_ptr<DictionaryValue> other_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTIONS_H_

