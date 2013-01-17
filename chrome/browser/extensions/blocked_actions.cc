// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/logging.h"
#include "chrome/browser/extensions/blocked_actions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char* BlockedAction::kTableName = "activitylog_blocked";
const char* BlockedAction::kTableStructure = "("
    "extension_id LONGVARCHAR NOT NULL, "
    "time INTEGER NOT NULL, "
    "blocked_action LONGVARCHAR NOT NULL, "
    "reason LONGVARCHAR NOT NULL, "
    "extra LONGVARCHAR NOT NULL)";

BlockedAction::BlockedAction(const std::string& extension_id,
                             const base::Time& time,
                             const std::string& blocked_action,
                             const std::string& reason,
                             const std::string& extra)
    : extension_id_(extension_id),
      time_(time),
      blocked_action_(blocked_action),
      reason_(reason),
      extra_(extra) { }

BlockedAction::~BlockedAction() {
}

void BlockedAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName)
    + " (extension_id, time, blocked_action, reason, extra) VALUES (?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id_);
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindString(2, blocked_action_);
  statement.BindString(3, reason_);
  statement.BindString(4, extra_);
  if (!statement.Run())
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
}

std::string BlockedAction::PrettyPrintFori18n() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return PrettyPrintForDebug();
}

std::string BlockedAction::PrettyPrintForDebug() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return "ID: " + extension_id_ + ", blocked action " + blocked_action_ +
      ", reason: " + reason_;
}

}  // namespace extensions

