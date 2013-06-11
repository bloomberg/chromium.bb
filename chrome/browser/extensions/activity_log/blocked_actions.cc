// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/blocked_actions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

using api::activity_log_private::ExtensionActivity;
using api::activity_log_private::DomActivityDetail;
using api::activity_log_private::ChromeActivityDetail;
using api::activity_log_private::BlockedChromeActivityDetail;

const char* BlockedAction::kTableName = "activitylog_blocked";
const char* BlockedAction::kTableContentFields[] =
    {"api_call", "args", "reason", "extra"};
const char* BlockedAction::kTableFieldTypes[] =
    {"LONGVARCHAR", "LONGVARCHAR", "INTEGER", "LONGVARCHAR"};

BlockedAction::BlockedAction(const std::string& extension_id,
                             const base::Time& time,
                             const std::string& api_call,
                             const std::string& args,
                             const BlockedAction::Reason reason,
                             const std::string& extra)
    : Action(extension_id,
             time,
             ExtensionActivity::ACTIVITY_TYPE_BLOCKED_CHROME),
      api_call_(api_call),
      args_(args),
      reason_(reason),
      extra_(extra) { }

BlockedAction::BlockedAction(const sql::Statement& s)
    : Action(s.ColumnString(0),
             base::Time::FromInternalValue(s.ColumnInt64(1)),
             ExtensionActivity::ACTIVITY_TYPE_BLOCKED_CHROME),
      api_call_(s.ColumnString(2)),
      args_(s.ColumnString(3)),
      reason_(static_cast<Reason>(s.ColumnInt(4))),
      extra_(s.ColumnString(5)) { }

BlockedAction::~BlockedAction() {
}

scoped_ptr<ExtensionActivity> BlockedAction::ConvertToExtensionActivity() {
  scoped_ptr<ExtensionActivity> formatted_activity;
  formatted_activity.reset(new ExtensionActivity);
  formatted_activity->extension_id.reset(
      new std::string(extension_id()));
  formatted_activity->activity_type = activity_type();
  formatted_activity->time.reset(new double(time().ToJsTime()));
  BlockedChromeActivityDetail* details = new BlockedChromeActivityDetail;
  details->api_call.reset(new std::string(api_call_));
  details->args.reset(new std::string(args_));
  details->reason = BlockedChromeActivityDetail::ParseReason(
      ReasonAsString());
  details->extra.reset(new std::string(extra_));
  formatted_activity->blocked_chrome_activity_detail.reset(details);
  return formatted_activity.Pass();
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
  // We also now use INTEGER instead of VARCHAR for url_action_type.
  if (db->DoesColumnExist(kTableName, "reason")) {
    std::string select = base::StringPrintf(
        "SELECT reason FROM %s ORDER BY rowid LIMIT 1", kTableName);
    sql::Statement statement(db->GetUniqueStatement(select.c_str()));
    if (statement.DeclaredColumnType(0) != sql::COLUMN_TYPE_INTEGER) {
      std::string drop_table = base::StringPrintf("DROP TABLE %s", kTableName);
      if (!db->Execute(drop_table.c_str()))
        return false;
    }
  }
  return InitializeTableInternal(db,
                                 kTableName,
                                 kTableContentFields,
                                 kTableFieldTypes,
                                 arraysize(kTableContentFields));
}

bool BlockedAction::Record(sql::Connection* db) {
  std::string sql_str = "INSERT INTO " + std::string(kTableName)
    + " (extension_id, time, api_call, args, reason, extra)"
    "  VALUES (?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id());
  statement.BindInt64(1, time().ToInternalValue());
  statement.BindString(2, api_call_);
  statement.BindString(3, args_);
  statement.BindInt(4, static_cast<int>(reason_));
  statement.BindString(5, extra_);
  if (!statement.Run()) {
    LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
    statement.Clear();
    return false;
  } else {
    return true;
  }
}

std::string BlockedAction::PrintForDebug() {
  return "ID: " + extension_id() + ", blocked action " + api_call_ +
      ", reason: " + ReasonAsString();
}

std::string BlockedAction::ReasonAsString() const {
  if (reason_ == ACCESS_DENIED)
    return std::string("access_denied");
  else if (reason_ == QUOTA_EXCEEDED)
    return std::string("quota_exceeded");
  else
    return std::string("unknown_reason_type");  // To avoid Win header name.
}

}  // namespace extensions

