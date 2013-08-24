// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/dom_action_types.h"
#include "chrome/common/extensions/extension.h"
#include "sql/error_delegate_util.h"
#include "sql/transaction.h"
#include "url/gurl.h"

using base::Callback;
using base::FilePath;
using base::Time;
using base::Unretained;
using content::BrowserThread;

namespace constants = activity_log_constants;

namespace extensions {

const char* FullStreamUIPolicy::kTableName = "activitylog_full";
const char* FullStreamUIPolicy::kTableContentFields[] = {
  "extension_id", "time", "action_type", "api_name", "args", "page_url",
  "page_title", "arg_url", "other"
};
const char* FullStreamUIPolicy::kTableFieldTypes[] = {
  "LONGVARCHAR NOT NULL", "INTEGER", "INTEGER", "LONGVARCHAR", "LONGVARCHAR",
  "LONGVARCHAR", "LONGVARCHAR", "LONGVARCHAR", "LONGVARCHAR"
};
const int FullStreamUIPolicy::kTableFieldCount =
    arraysize(FullStreamUIPolicy::kTableContentFields);

FullStreamUIPolicy::FullStreamUIPolicy(Profile* profile)
    : ActivityLogDatabasePolicy(
          profile,
          FilePath(chrome::kExtensionActivityLogFilename)) {}

FullStreamUIPolicy::~FullStreamUIPolicy() {}

bool FullStreamUIPolicy::InitDatabase(sql::Connection* db) {
  if (!Util::DropObsoleteTables(db))
    return false;

  // Create the unified activity log entry table.
  return ActivityDatabase::InitializeTable(db,
                                           kTableName,
                                           kTableContentFields,
                                           kTableFieldTypes,
                                           arraysize(kTableContentFields));
}

bool FullStreamUIPolicy::FlushDatabase(sql::Connection* db) {
  if (queued_actions_.empty())
    return true;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  std::string sql_str =
      "INSERT INTO " + std::string(FullStreamUIPolicy::kTableName) +
      " (extension_id, time, action_type, api_name, args, "
      "page_url, page_title, arg_url, other) VALUES (?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));

  Action::ActionVector::size_type i;
  for (i = 0; i != queued_actions_.size(); ++i) {
    const Action& action = *queued_actions_[i];
    statement.Reset(true);
    statement.BindString(0, action.extension_id());
    statement.BindInt64(1, action.time().ToInternalValue());
    statement.BindInt(2, static_cast<int>(action.action_type()));
    statement.BindString(3, action.api_name());
    if (action.args()) {
      statement.BindString(4, Util::Serialize(action.args()));
    }
    std::string page_url_string = action.SerializePageUrl();
    if (!page_url_string.empty()) {
      statement.BindString(5, page_url_string);
    }
    if (!action.page_title().empty()) {
      statement.BindString(6, action.page_title());
    }
    std::string arg_url_string = action.SerializeArgUrl();
    if (!arg_url_string.empty()) {
      statement.BindString(7, arg_url_string);
    }
    if (action.other()) {
      statement.BindString(8, Util::Serialize(action.other()));
    }

    if (!statement.Run()) {
      LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
      return false;
    }
  }

  if (!transaction.Commit())
    return false;

  queued_actions_.clear();
  return true;
}

scoped_ptr<Action::ActionVector> FullStreamUIPolicy::DoReadData(
    const std::string& extension_id,
    const int days_ago) {
  // Ensure data is flushed to the database first so that we query over all
  // data.
  activity_database()->AdviseFlush(ActivityDatabase::kFlushImmediately);

  DCHECK_GE(days_ago, 0);
  scoped_ptr<Action::ActionVector> actions(new Action::ActionVector());

  sql::Connection* db = GetDatabaseConnection();
  if (!db) {
    return actions.Pass();
  }

  int64 early_bound;
  int64 late_bound;
  Util::ComputeDatabaseTimeBounds(Now(), days_ago, &early_bound, &late_bound);
  std::string query_str = base::StringPrintf(
      "SELECT time, action_type, api_name, args, page_url, page_title, "
      "arg_url, other "
      "FROM %s WHERE extension_id=? AND time>? AND time<=? "
      "ORDER BY time DESC",
      kTableName);
  sql::Statement query(db->GetCachedStatement(SQL_FROM_HERE,
                                              query_str.c_str()));
  query.BindString(0, extension_id);
  query.BindInt64(1, early_bound);
  query.BindInt64(2, late_bound);
  while (query.is_valid() && query.Step()) {
    scoped_refptr<Action> action =
        new Action(extension_id,
                   base::Time::FromInternalValue(query.ColumnInt64(0)),
                   static_cast<Action::ActionType>(query.ColumnInt(1)),
                   query.ColumnString(2));

    if (query.ColumnType(3) != sql::COLUMN_TYPE_NULL) {
      scoped_ptr<Value> parsed_value(
          base::JSONReader::Read(query.ColumnString(3)));
      if (parsed_value && parsed_value->IsType(Value::TYPE_LIST)) {
        action->set_args(
            make_scoped_ptr(static_cast<ListValue*>(parsed_value.release())));
      } else {
        LOG(WARNING) << "Unable to parse args: '" << query.ColumnString(3)
                     << "'";
      }
    }

    action->ParsePageUrl(query.ColumnString(4));
    action->set_page_title(query.ColumnString(5));
    action->ParseArgUrl(query.ColumnString(6));

    if (query.ColumnType(7) != sql::COLUMN_TYPE_NULL) {
      scoped_ptr<Value> parsed_value(
          base::JSONReader::Read(query.ColumnString(7)));
      if (parsed_value && parsed_value->IsType(Value::TYPE_DICTIONARY)) {
        action->set_other(make_scoped_ptr(
            static_cast<DictionaryValue*>(parsed_value.release())));
      } else {
        LOG(WARNING) << "Unable to parse other: '" << query.ColumnString(7)
                     << "'";
      }
    }

    actions->push_back(action);
  }
  return actions.Pass();
}

void FullStreamUIPolicy::OnDatabaseFailure() {
  queued_actions_.clear();
}

void FullStreamUIPolicy::OnDatabaseClose() {
  delete this;
}

void FullStreamUIPolicy::Close() {
  // The policy object should have never been created if there's no DB thread.
  DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::DB));
  ScheduleAndForget(activity_database(), &ActivityDatabase::Close);
}

// Get data as a set of key-value pairs.  The keys are policy-specific.
void FullStreamUIPolicy::ReadData(
    const std::string& extension_id,
    const int day,
    const Callback<void(scoped_ptr<Action::ActionVector>)>& callback) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&FullStreamUIPolicy::DoReadData,
                 base::Unretained(this),
                 extension_id,
                 day),
      callback);
}

scoped_refptr<Action> FullStreamUIPolicy::ProcessArguments(
    scoped_refptr<Action> action) const {
  return action;
}

void FullStreamUIPolicy::ProcessAction(scoped_refptr<Action> action) {
  // TODO(mvrable): Right now this argument stripping updates the Action object
  // in place, which isn't good if there are other users of the object.  When
  // database writing is moved to policy class, the modifications should be
  // made locally.
  action = ProcessArguments(action);
  ScheduleAndForget(this, &FullStreamUIPolicy::QueueAction, action);
}

void FullStreamUIPolicy::QueueAction(scoped_refptr<Action> action) {
  if (activity_database()->is_db_valid()) {
    queued_actions_.push_back(action);
    activity_database()->AdviseFlush(queued_actions_.size());
  }
}

}  // namespace extensions
