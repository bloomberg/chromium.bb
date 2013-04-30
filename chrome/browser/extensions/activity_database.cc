// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "base/time/clock.h"
#include "chrome/browser/extensions/activity_database.h"
#include "chrome/common/chrome_switches.h"
#include "sql/transaction.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::BrowserThread;

namespace {

bool SortActionsByTime(const scoped_refptr<extensions::Action> a,
                       const scoped_refptr<extensions::Action> b) {
  return a->time() > b->time();
}

}  // namespace

namespace extensions {

ActivityDatabase::ActivityDatabase()
    : testing_clock_(NULL),
      initialized_(false) {
  // We don't batch commits when in testing mode.
  batch_mode_ = !(CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableExtensionActivityLogTesting));
}

ActivityDatabase::~ActivityDatabase() {}

void ActivityDatabase::SetErrorDelegate(sql::ErrorDelegate* error_delegate) {
  db_.set_error_delegate(error_delegate);
}

void ActivityDatabase::Init(const base::FilePath& db_name) {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::DB))
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
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

  db_.Preload();

  // Create the DOMAction database.
  if (!DOMAction::InitializeTable(&db_))
    return LogInitFailure();

  // Create the APIAction database.
  if (!APIAction::InitializeTable(&db_))
    return LogInitFailure();

  // Create the BlockedAction database.
  if (!BlockedAction::InitializeTable(&db_))
    return LogInitFailure();

  sql::InitStatus stat = committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
  if (stat != sql::INIT_OK)
    return LogInitFailure();

  initialized_ = true;
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMinutes(2),
               this,
               &ActivityDatabase::RecordBatchedActions);
}

void ActivityDatabase::LogInitFailure() {
  LOG(ERROR) << "Couldn't initialize the activity log database.";
}

void ActivityDatabase::RecordAction(scoped_refptr<Action> action) {
  if (initialized_) {
    if (batch_mode_)
      batched_actions_.push_back(action);
    else
      action->Record(&db_);
  }
}

void ActivityDatabase::RecordBatchedActions() {
  std::vector<scoped_refptr<Action> >::size_type i;
  for (i = 0; i != batched_actions_.size(); ++i) {
    batched_actions_.at(i)->Record(&db_);
  }
  batched_actions_.clear();
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

scoped_ptr<std::vector<scoped_refptr<Action> > > ActivityDatabase::GetActions(
    const std::string& extension_id, const int days_ago) {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::DB))
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK_GE(days_ago, 0);
  scoped_ptr<std::vector<scoped_refptr<Action> > >
      actions(new std::vector<scoped_refptr<Action> >());
  if (!initialized_)
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
  // Get the DOMActions.
  std::string dom_str = base::StringPrintf("SELECT * FROM %s "
                                           "WHERE extension_id=? AND "
                                           "time>? AND time<=?",
                                           DOMAction::kTableName);
  sql::Statement dom_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                      dom_str.c_str()));
  dom_statement.BindString(0, extension_id);
  dom_statement.BindInt64(1, early_bound);
  dom_statement.BindInt64(2, late_bound);
  while (dom_statement.Step()) {
    scoped_refptr<DOMAction> action = new DOMAction(dom_statement);
    actions->push_back(action);
  }
  // Get the APIActions.
  std::string api_str = base::StringPrintf("SELECT * FROM %s "
                                           "WHERE extension_id=? AND "
                                           "time>? AND time<=?",
                                            APIAction::kTableName);
  sql::Statement api_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                      api_str.c_str()));
  api_statement.BindString(0, extension_id);
  api_statement.BindInt64(1, early_bound);
  api_statement.BindInt64(2, late_bound);
  while (api_statement.Step()) {
    scoped_refptr<APIAction> action = new APIAction(api_statement);
    actions->push_back(action);
  }
  // Get the BlockedActions.
  std::string blocked_str = base::StringPrintf("SELECT * FROM %s "
                                               "WHERE extension_id=? AND "
                                               "time>? AND time<=?",
                                               BlockedAction::kTableName);
  sql::Statement blocked_statement(db_.GetCachedStatement(SQL_FROM_HERE,
                                                          blocked_str.c_str()));
  blocked_statement.BindString(0, extension_id);
  blocked_statement.BindInt64(1, early_bound);
  blocked_statement.BindInt64(2, late_bound);
  while (blocked_statement.Step()) {
    scoped_refptr<BlockedAction> action = new BlockedAction(blocked_statement);
    actions->push_back(action);
  }
  // Sort by time (from newest to oldest).
  std::sort(actions->begin(), actions->end(), SortActionsByTime);
  return actions.Pass();
}

void ActivityDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void ActivityDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

void ActivityDatabase::RollbackTransaction() {
  db_.RollbackTransaction();
}

bool ActivityDatabase::Raze() {
  return db_.Raze();
}

void ActivityDatabase::Close() {
  timer_.Stop();
  RecordBatchedActions();
  db_.Close();
  delete this;
}

void ActivityDatabase::KillDatabase() {
  timer_.Stop();
  db_.RazeAndClose();
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

}  // namespace extensions
