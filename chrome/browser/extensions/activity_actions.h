// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_ACTIONS_H_

#include <string>
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "chrome/browser/history/url_database.h"
#include "sql/connection.h"
#include "sql/transaction.h"

namespace extensions {

// This is the interface for extension actions that are to be recorded in
// the activity log.
class Action : public base::RefCountedThreadSafe<Action> {
 public:
  // Record the action in the database.
  virtual void Record(sql::Connection* db) = 0;

  // Print an action with il8n substitutions for display.
  virtual std::string PrettyPrintFori18n() = 0;

  // Print an action as a regular string for debugging purposes.
  virtual std::string PrettyPrintForDebug() = 0;

 protected:
  Action() {}
  virtual ~Action() {}

 private:
  friend class base::RefCountedThreadSafe<Action>;

  DISALLOW_COPY_AND_ASSIGN(Action);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_ACTIONS_H_

