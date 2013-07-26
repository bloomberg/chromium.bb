// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/common/chrome_switches.h"
#include "sql/error_delegate_util.h"
#include "sql/transaction.h"
#include "third_party/sqlite/sqlite3.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::BrowserThread;

namespace extensions {

ActivityDatabase::ActivityDatabase(ActivityDatabase::Delegate* delegate)
    : delegate_(delegate),
      testing_clock_(NULL),
      valid_db_(false),
      already_closed_(false),
      did_init_(false) {
  // We don't batch commits when in testing mode.
  batch_mode_ = !(CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableExtensionActivityLogTesting));
}

ActivityDatabase::~ActivityDatabase() {}

void ActivityDatabase::Init(const base::FilePath& db_name) {
  if (did_init_) return;
  did_init_ = true;
  if (BrowserThread::IsMessageLoopValid(BrowserThread::DB))
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_.set_histogram_tag("Activity");
  db_.set_error_callback(
      base::Bind(&ActivityDatabase::DatabaseErrorCallback,
                 base::Unretained(this)));
  db_.set_page_size(4096);
  db_.set_cache_size(32);

  if (!db_.Open(db_name)) {
    LOG(ERROR) << db_.GetErrorMessage();
    return LogInitFailure();
  }

  // Wrap the initialization in a transaction so that the db doesn't
  // get corrupted if init fails/crashes.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return LogInitFailure();

#if defined(OS_MACOSX)
  // Exclude the database from backups.
  base::mac::SetFileBackupExclusion(db_name);
#endif

  if (!delegate_->OnDatabaseInit(&db_))
    return LogInitFailure();

  sql::InitStatus stat = committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
  if (stat != sql::INIT_OK)
    return LogInitFailure();

  // Pre-loads the first <cache-size> pages into the cache.
  // Doesn't do anything if the database is new.
  db_.Preload();

  valid_db_ = true;
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMinutes(2),
               this,
               &ActivityDatabase::RecordBatchedActions);
}

void ActivityDatabase::LogInitFailure() {
  LOG(ERROR) << "Couldn't initialize the activity log database.";
  SoftFailureClose();
}

void ActivityDatabase::RecordAction(scoped_refptr<Action> action) {
  if (!valid_db_) return;
  if (batch_mode_) {
    batched_actions_.push_back(action);
  } else {
    if (!action->Record(&db_)) SoftFailureClose();
  }
}

void ActivityDatabase::RecordBatchedActions() {
  if (!valid_db_) return;
  bool failure = false;
  std::vector<scoped_refptr<Action> >::size_type i;
  for (i = 0; i != batched_actions_.size(); ++i) {
    if (!batched_actions_.at(i)->Record(&db_)) {
      failure = true;
      break;
    }
  }
  batched_actions_.clear();
  if (failure) SoftFailureClose();
}

void ActivityDatabase::SetBatchModeForTesting(bool batch_mode) {
  if (batch_mode && !batch_mode_) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMinutes(2),
                 this,
                 &ActivityDatabase::RecordBatchedActions);
  } else if (!batch_mode && batch_mode_) {
    timer_.Stop();
    RecordBatchedActions();
  }
  batch_mode_ = batch_mode;
}

scoped_ptr<ActivityDatabase::ActionVector> ActivityDatabase::GetActions(
    const std::string& extension_id, const int days_ago) {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::DB))
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK_GE(days_ago, 0);

  scoped_ptr<ActionVector> actions(new ActionVector());
  if (!valid_db_)
    return actions.Pass();
  // Compute the time bounds for that day.
  base::Time morning_midnight = testing_clock_ ?
      testing_clock_->Now().LocalMidnight() :
      base::Time::Now().LocalMidnight();
  int64 early_bound = 0;
  int64 late_bound = 0;
  if (days_ago == 0) {
      early_bound = morning_midnight.ToInternalValue();
      late_bound = base::Time::Max().ToInternalValue();
  } else {
      base::Time early_time = morning_midnight -
          base::TimeDelta::FromDays(days_ago);
      base::Time late_time = morning_midnight -
          base::TimeDelta::FromDays(days_ago-1);
      early_bound = early_time.ToInternalValue();
      late_bound = late_time.ToInternalValue();
  }
  std::string query_str = base::StringPrintf(
      "SELECT time, action_type, api_name, args, page_url, page_title, "
      "arg_url, other "
      "FROM %s WHERE extension_id=? AND time>? AND time<=? "
      "ORDER BY time DESC",
      FullStreamUIPolicy::kTableName);
  sql::Statement query(db_.GetCachedStatement(SQL_FROM_HERE,
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

    GURL page_url(query.ColumnString(4));
    action->set_page_url(page_url);

    action->set_page_title(query.ColumnString(5));

    GURL arg_url(query.ColumnString(6));
    action->set_arg_url(arg_url);

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

void ActivityDatabase::Close() {
  timer_.Stop();
  if (!already_closed_) {
    RecordBatchedActions();
    db_.reset_error_callback();
  }
  valid_db_ = false;
  already_closed_ = true;
  // Call DatabaseCloseCallback() just before deleting the ActivityDatabase
  // itself--these two objects should have the same lifetime.
  delegate_->OnDatabaseClose();
  delete this;
}

void ActivityDatabase::HardFailureClose() {
  if (already_closed_) return;
  valid_db_ = false;
  timer_.Stop();
  db_.reset_error_callback();
  db_.RazeAndClose();
  already_closed_ = true;
}

void ActivityDatabase::SoftFailureClose() {
  valid_db_ = false;
  timer_.Stop();
}

void ActivityDatabase::DatabaseErrorCallback(int error, sql::Statement* stmt) {
  if (sql::IsErrorCatastrophic(error)) {
    LOG(ERROR) << "Killing the ActivityDatabase due to catastrophic error.";
    HardFailureClose();
  } else if (error != SQLITE_BUSY) {
    // We ignore SQLITE_BUSY errors because they are presumably transient.
    LOG(ERROR) << "Closing the ActivityDatabase due to error.";
    SoftFailureClose();
  }
}

void ActivityDatabase::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

void ActivityDatabase::RecordBatchedActionsWhileTesting() {
  RecordBatchedActions();
  timer_.Stop();
}

void ActivityDatabase::SetTimerForTesting(int ms) {
  timer_.Stop();
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(ms),
               this,
               &ActivityDatabase::RecordBatchedActionsWhileTesting);
}

// static
bool ActivityDatabase::InitializeTable(sql::Connection* db,
                                       const char* table_name,
                                       const char* content_fields[],
                                       const char* field_types[],
                                       const int num_content_fields) {
  if (!db->DoesTableExist(table_name)) {
    std::string table_creator =
        base::StringPrintf("CREATE TABLE %s (", table_name);
    for (int i = 0; i < num_content_fields; i++) {
      table_creator += base::StringPrintf("%s%s %s",
                                          i == 0 ? "" : ", ",
                                          content_fields[i],
                                          field_types[i]);
    }
    table_creator += ")";
    if (!db->Execute(table_creator.c_str()))
      return false;
  } else {
    // In case we ever want to add new fields, this initializes them to be
    // empty strings.
    for (int i = 0; i < num_content_fields; i++) {
      if (!db->DoesColumnExist(table_name, content_fields[i])) {
        std::string table_updater = base::StringPrintf(
            "ALTER TABLE %s ADD COLUMN %s %s; ",
             table_name,
             content_fields[i],
             field_types[i]);
        if (!db->Execute(table_updater.c_str()))
          return false;
      }
    }
  }
  return true;
}

}  // namespace extensions
