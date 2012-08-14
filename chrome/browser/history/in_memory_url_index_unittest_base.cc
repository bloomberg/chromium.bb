// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index_unittest_base.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_url_cache_database.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace history {

InMemoryURLIndexTestBase::InMemoryURLIndexTestBase()
    : ui_thread_(content::BrowserThread::UI, &message_loop_),
      db_thread_(content::BrowserThread::DB) {
}

InMemoryURLIndexTestBase::~InMemoryURLIndexTestBase() {}

void InMemoryURLIndexTestBase::SetUp() {
  if (!profile_.get())
    profile_.reset(new TestingProfile);
  db_thread_.Start();
  // We cannot access the database until the backend has been loaded.
  profile_->CreateHistoryService(true, false);
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_.get(),
                                           Profile::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  url_index_ = history_service->InMemoryIndex();
  BlockUntilIndexLoaded();
  DCHECK(url_index_->index_available());
  profile_->CreateBookmarkModel(true);
  HistoryBackend* backend = history_service->get_history_backend_for_testing();
  history_database_ = backend->db();
  ui_test_utils::WaitForHistoryToLoad(history_service);
  DCHECK(history_service->backend_loaded());

  // Create and populate a working copy of the URL history database from the
  // data contained in the file specified by the TestDBName() function.
  // TODO(mrossetti): Adopt sqlite3_ functions for performing the database
  // initialization. See http://crbug.com/137352.
  FilePath test_file_path;
  PathService::Get(chrome::DIR_TEST_DATA, &test_file_path);
  test_file_path = test_file_path.Append(FILE_PATH_LITERAL("History"));
  test_file_path = test_file_path.Append(TestDBName());
  EXPECT_TRUE(file_util::PathExists(test_file_path));

  std::string sql_command_buffer;
  file_util::ReadFileToString(test_file_path, &sql_command_buffer);
  std::vector<std::string> sql_commands;
  base::SplitStringDontTrim(sql_command_buffer, '\n', &sql_commands);
  sql::Connection* db(history_database_->get_db_for_testing());
  for (std::vector<std::string>::const_iterator i = sql_commands.begin();
       i != sql_commands.end(); ++i) {
    // We only process lines which begin with a upper-case letter.
    // TODO(mrossetti): Can iswupper() be used here?
    const std::string& sql_command(*i);
    if (sql_command[0] >= 'A' && sql_command[0] <= 'Z')
      EXPECT_TRUE(db->Execute(sql_command.c_str()));
  }

  // Update the last_visit_time table column such that it represents a time
  // relative to 'now'.
  // TODO(mrossetti): Do an UPDATE to alter the days-ago.
  // See http://crbug.com/137352.
  sql::Statement statement(db->GetUniqueStatement(
      "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls"));
  EXPECT_TRUE(statement.is_valid());
  base::Time time_right_now = base::Time::NowFromSystemTime();
  base::TimeDelta day_delta = base::TimeDelta::FromDays(1);
  while (statement.Step()) {
    URLRow row;
    history_database_->FillURLRow(statement, &row);
    row.set_last_visit(time_right_now -
                       day_delta * row.last_visit().ToInternalValue());
    history_database_->UpdateURLRow(row.id(), row);
  }
}

void InMemoryURLIndexTestBase::TearDown() {
  MessageLoop::current()->RunAllPending();
  message_loop_.RunAllPending();
  db_thread_.Stop();
}

void InMemoryURLIndexTestBase::LoadIndex() {
  url_index_->ScheduleRebuildFromHistory();
  BlockUntilIndexLoaded();
}

void InMemoryURLIndexTestBase::BlockUntilIndexLoaded() {
  if (url_index_->index_available())
    return;
  InMemoryURLIndex::Observer observer(url_index_);
  MessageLoop::current()->Run();
}

URLIndexPrivateData* InMemoryURLIndexTestBase::GetPrivateData() {
  return url_index_->private_data();
}

bool InMemoryURLIndexTestBase::UpdateURL(const URLRow& row) {
  bool success = GetPrivateData()->UpdateURL(row);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  return success;
}

bool InMemoryURLIndexTestBase::DeleteURL(const GURL& url) {
  bool success = GetPrivateData()->DeleteURL(url);
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  return success;
}

bool InMemoryURLIndexTestBase::GetCacheDBPath(FilePath* file_path) {
  return GetPrivateData()->GetCacheDBPath(file_path);
}

}  // namespace history
