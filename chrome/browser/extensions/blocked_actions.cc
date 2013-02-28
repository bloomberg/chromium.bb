// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/blocked_actions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char* BlockedAction::kTableName = "activitylog_blocked";
const char* BlockedAction::kTableContentFields[] =
    {"api_call", "args", "reason", "extra"};

BlockedAction::BlockedAction(const std::string& extension_id,
                             const base::Time& time,
                             const std::string& api_call,
                             const std::string& args,
                             const std::string& reason,
                             const std::string& extra)
    : Action(extension_id, time),
      api_call_(api_call),
      args_(args),
      reason_(reason),
      extra_(extra) { }

BlockedAction::BlockedAction(const sql::Statement& s)
    : Action(s.ColumnString(0),
          base::Time::FromInternalValue(s.ColumnInt64(1))),
      api_call_(s.ColumnString(2)),
      args_(s.ColumnString(3)),
      reason_(s.ColumnString(4)),
      extra_(s.ColumnString(5)) { }

BlockedAction::~BlockedAction() {
}

// static
bool BlockedAction::InitializeTable(sql::Connection* db) {
  // The original table schema was different than the existing one.
  // Sqlite doesn't let you delete or modify existing columns, so we drop it.
  // The old version can be identified because it had a field named
  // blocked_action. Any data loss incurred here doesn't matter since these
  // fields existed before we started using the AL for anything.
  if (db->DoesColumnExist(kTableName, "blocked_action")) {
    std::string drop_table = base::StringPrintf("DROP TABLE %s", kTableName);
    if (!db->Execute(drop_table.c_str()))
      return false;
  }
  return InitializeTableInternal(db,
                                 kTableName,
                                 kTableContentFields,
                                 arraysize(kTableContentFields));
}

void BlockedAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName)
    + " (extension_id, time, api_call, args, reason, extra)"
    "  VALUES (?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindString(2, api_call_);
  statement.BindString(3, args_);
  statement.BindString(4, reason_);
  statement.BindString(5, extra_);
  if (!statement.Run())
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
}

std::string BlockedAction::PrettyPrintFori18n() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return PrettyPrintForDebug();
}

std::string BlockedAction::PrettyPrintForDebug() {
  // TODO(felt): implement this for real when the UI is redesigned.
  return "ID: " + extension_id() + ", blocked action " + api_call_ +
      ", reason: " + reason_;
}

}  // namespace extensions

