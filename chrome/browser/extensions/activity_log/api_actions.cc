// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/activity_log/api_actions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char* APIAction::kTableName = "activitylog_apis";
const char* APIAction::kTableContentFields[] =
    {"api_type", "api_call", "args", "extra"};

// We should log the arguments to these API calls, even if argument logging is
// disabled by default.
const char* APIAction::kAlwaysLog[] =
    {"extension.connect", "extension.sendMessage",
     "tabs.executeScript", "tabs.insertCSS" };
const int APIAction::kSizeAlwaysLog = arraysize(kAlwaysLog);

APIAction::APIAction(const std::string& extension_id,
                     const base::Time& time,
                     const Type type,
                     const std::string& api_call,
                     const std::string& args,
                     const std::string& extra)
    : Action(extension_id, time),
      type_(type),
      api_call_(api_call),
      args_(args),
      extra_(extra) { }

APIAction::APIAction(const sql::Statement& s)
    : Action(s.ColumnString(0),
          base::Time::FromInternalValue(s.ColumnInt64(1))),
      type_(StringAsType(s.ColumnString(2))),
      api_call_(s.ColumnString(3)),
      args_(s.ColumnString(4)),
      extra_(s.ColumnString(5)) { }

APIAction::~APIAction() {
}

// static
bool APIAction::InitializeTable(sql::Connection* db) {
  // The original table schema was different than the existing one.
  // We no longer want the api_action_type or target_type columns.
  // Sqlite doesn't let you delete or modify existing columns, so we drop it.
  // Any data loss incurred here doesn't matter since these fields existed
  // before we started using the AL for anything.
  if (db->DoesColumnExist(kTableName, "api_action_type")) {
    std::string drop_table = base::StringPrintf("DROP TABLE %s", kTableName);
    if (!db->Execute(drop_table.c_str()))
      return false;
  }
  // Now initialize the table.
  return InitializeTableInternal(db,
                                 kTableName,
                                 kTableContentFields,
                                 arraysize(kTableContentFields));
}

void APIAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName)
      + " (extension_id, time, api_type, api_call, args, extra) VALUES"
      " (?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindString(2, TypeAsString());
  statement.BindString(3, api_call_);
  statement.BindString(4, args_);
  statement.BindString(5, extra_);
  if (!statement.Run())
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
}

std::string APIAction::PrintForDebug() {
  return "ID: " + extension_id() + ", CATEGORY: " + TypeAsString() +
      ", API: " + api_call_ + ", ARGS: " + args_;
}

std::string APIAction::TypeAsString() const {
  switch (type_) {
    case CALL:
      return "CALL";
    case EVENT_CALLBACK:
      return "EVENT_CALLBACK";
    default:
      return "UNKNOWN_TYPE";
  }
}

APIAction::Type APIAction::StringAsType(
    const std::string& str) {
  if (str == "CALL") {
    return CALL;
  } else if (str == "EVENT_CALLBACK") {
    return EVENT_CALLBACK;
  } else {
    return UNKNOWN_TYPE;
  }
}

}  // namespace extensions

