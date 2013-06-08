// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTIONS_H_

#include <string>
#include "base/memory/ref_counted_memory.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/common/extensions/api/activity_log_private.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace extensions {

// This is the interface for extension actions that are to be recorded in
// the activity log.
class Action : public base::RefCountedThreadSafe<Action> {
 public:
  static const char* kTableBasicFields;

  // Initialize the table for a given action type.
  static bool InitializeTableInternal(sql::Connection* db);

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

  // Initialize the table for a given action type.
  // The content_fields array should list the names of all of the columns in
  // the database. The field_types should specify the types of the corresponding
  // columns (e.g., INTEGER or LONGVARCHAR). There should be the same number of
  // field_types as content_fields, since the two arrays should correspond.
  static bool InitializeTableInternal(sql::Connection* db,
                                      const char* table_name,
                                      const char* content_fields[],
                                      const char* field_types[],
                                      const int num_content_fields);

 private:
  friend class base::RefCountedThreadSafe<Action>;

  std::string extension_id_;
  base::Time time_;
  api::activity_log_private::ExtensionActivity::ActivityType activity_type_;

  DISALLOW_COPY_AND_ASSIGN(Action);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTIONS_H_

