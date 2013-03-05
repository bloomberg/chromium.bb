// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_ACTIONS_H_

#include <string>
#include "base/memory/ref_counted_memory.h"
#include "base/time.h"
#include "base/values.h"
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
  virtual void Record(sql::Connection* db) = 0;

  // Print an action with il8n substitutions for display.
  virtual std::string PrettyPrintFori18n() = 0;

  // Print an action as a regular string for debugging purposes.
  virtual std::string PrettyPrintForDebug() = 0;

  const std::string& extension_id() const { return extension_id_; }
  const base::Time& time() const { return time_; }

 protected:
  Action(const std::string& extension_id, const base::Time& time);
  virtual ~Action() {}

  // Initialize the table for a given action type.
  static bool InitializeTableInternal(sql::Connection* db,
                                      const char* table_name,
                                      const char* content_fields[],
                                      const int num_content_fields);

 private:
  friend class base::RefCountedThreadSafe<Action>;

  std::string extension_id_;
  base::Time time_;

  DISALLOW_COPY_AND_ASSIGN(Action);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_ACTIONS_H_
