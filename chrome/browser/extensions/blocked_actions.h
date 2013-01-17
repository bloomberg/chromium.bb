// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTIONS_H_
#define CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTIONS_H_

#include <string>
#include "base/time.h"
#include "chrome/browser/extensions/activity_actions.h"

namespace extensions {

// This class describes API calls that ran into permissions or quota problems.
// See APIActions for API calls that succeeded.
class BlockedAction : public Action {
 public:
  static const char* kTableName;
  static const char* kTableStructure;

  BlockedAction(const std::string& extension_id,
                const base::Time& time,
                const std::string& blocked_action,    // the blocked API call
                const std::string& reason,            // the reason it's blocked
                const std::string& extra);            // any extra logging info

  // Record the action in the database.
  virtual void Record(sql::Connection* db) OVERRIDE;

  // Print a BlockedAction with il8n substitutions for display.
  virtual std::string PrettyPrintFori18n() OVERRIDE;

  // Print a BlockedAction as a regular string for debugging purposes.
  virtual std::string PrettyPrintForDebug() OVERRIDE;

  // Helper methods for recording the values into the db.
  const std::string& extension_id() const { return extension_id_; }
  const base::Time& time() const { return time_; }
  const std::string& reason() const { return reason_; }
  const std::string& blocked_action() const { return blocked_action_; }
  const std::string& extra() const { return extra_; }

 protected:
  virtual ~BlockedAction();

 private:
  std::string extension_id_;
  base::Time time_;
  std::string blocked_action_;
  std::string reason_;
  std::string extra_;

  DISALLOW_COPY_AND_ASSIGN(BlockedAction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLOCKED_ACTIONS_H_

