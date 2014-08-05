// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History unit tests come in two flavors:
//
// 1. The more complicated style is that the unit test creates a full history
//    service. This spawns a background thread for the history backend, and
//    all communication is asynchronous. This is useful for testing more
//    complicated things or end-to-end behavior.
//
// 2. The simpler style is to create a history backend on this thread and
//    access it directly without a HistoryService object. This is much simpler
//    because communication is synchronous. Generally, sets should go through
//    the history backend (since there is a lot of logic) but gets can come
//    directly from the HistoryDatabase. This is because the backend generally
//    has no logic in the getter except threading stuff, which we don't want
//    to run.

#include <time.h>

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_unittest_base.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/common/thumbnail_score.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_service_proxy_for_test.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/protocol/history_delete_directive_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

using base::Time;
using base::TimeDelta;
using content::DownloadItem;

namespace history {
class HistoryBackendDBTest;

// Delegate class for when we create a backend without a HistoryService.
//
// This must be outside the anonymous namespace for the friend statement in
// HistoryBackendDBTest to work.
class BackendDelegate : public HistoryBackend::Delegate {
 public:
  explicit BackendDelegate(HistoryBackendDBTest* history_test)
      : history_test_(history_test) {
  }

  virtual void NotifyProfileError(sql::InitStatus init_status) OVERRIDE {}
  virtual void SetInMemoryBackend(
      scoped_ptr<InMemoryHistoryBackend> backend) OVERRIDE;
  virtual void BroadcastNotifications(
      int type,
      scoped_ptr<HistoryDetails> details) OVERRIDE;
  virtual void DBLoaded() OVERRIDE {}
  virtual void NotifyVisitDBObserversOnAddVisit(
      const BriefVisitInfo& info) OVERRIDE {}
 private:
  HistoryBackendDBTest* history_test_;
};

// This must be outside the anonymous namespace for the friend statement in
// HistoryBackend to work.
class HistoryBackendDBTest : public HistoryUnitTestBase {
 public:
  HistoryBackendDBTest() : db_(NULL) {
  }

  virtual ~HistoryBackendDBTest() {
  }

 protected:
  friend class BackendDelegate;

  // Creates the HistoryBackend and HistoryDatabase on the current thread,
  // assigning the values to backend_ and db_.
  void CreateBackendAndDatabase() {
    backend_ =
        new HistoryBackend(history_dir_, new BackendDelegate(this), NULL);
    backend_->Init(std::string(), false);
    db_ = backend_->db_.get();
    DCHECK(in_mem_backend_) << "Mem backend should have been set by "
        "HistoryBackend::Init";
  }

  void CreateDBVersion(int version) {
    base::FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("History");
    data_path =
          data_path.AppendASCII(base::StringPrintf("history.%d.sql", version));
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQLScript(data_path, history_dir_.Append(
            chrome::kHistoryFilename)));
  }

  void CreateArchivedDB() {
    base::FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("History");
    data_path = data_path.AppendASCII("archived_history.4.sql");
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQLScript(data_path, history_dir_.Append(
            chrome::kArchivedHistoryFilename)));
  }

  // testing::Test
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_dir_ = temp_dir_.path().AppendASCII("HistoryBackendDBTest");
    ASSERT_TRUE(base::CreateDirectory(history_dir_));
  }

  void DeleteBackend() {
    if (backend_.get()) {
      backend_->Closing();
      backend_ = NULL;
    }
  }

  virtual void TearDown() {
    DeleteBackend();

    // Make sure we don't have any event pending that could disrupt the next
    // test.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->Run();
  }

  bool AddDownload(uint32 id,
                   DownloadItem::DownloadState state,
                   const Time& time) {
    std::vector<GURL> url_chain;
    url_chain.push_back(GURL("foo-url"));

    DownloadRow download(base::FilePath(FILE_PATH_LITERAL("current-path")),
                         base::FilePath(FILE_PATH_LITERAL("target-path")),
                         url_chain,
                         GURL("http://referrer.com/"),
                         "application/vnd.oasis.opendocument.text",
                         "application/octet-stream",
                         time,
                         time,
                         std::string(),
                         std::string(),
                         0,
                         512,
                         state,
                         content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                         content::DOWNLOAD_INTERRUPT_REASON_NONE,
                         id,
                         false,
                         "by_ext_id",
                         "by_ext_name");
    return db_->CreateDownload(download);
  }

  base::ScopedTempDir temp_dir_;

  base::MessageLoopForUI message_loop_;

  // names of the database files
  base::FilePath history_dir_;

  // Created via CreateBackendAndDatabase.
  scoped_refptr<HistoryBackend> backend_;
  scoped_ptr<InMemoryHistoryBackend> in_mem_backend_;
  HistoryDatabase* db_;  // Cached reference to the backend's database.
};

void BackendDelegate::SetInMemoryBackend(
    scoped_ptr<InMemoryHistoryBackend> backend) {
  // Save the in-memory backend to the history test object, this happens
  // synchronously, so we don't have to do anything fancy.
  history_test_->in_mem_backend_.swap(backend);
}

void BackendDelegate::BroadcastNotifications(
    int type,
    scoped_ptr<HistoryDetails> details) {
  // Currently, just send the notifications directly to the in-memory database.
  // We may want do do something more fancy in the future.
  content::Details<HistoryDetails> det(details.get());
  history_test_->in_mem_backend_->Observe(type,
      content::Source<HistoryBackendDBTest>(NULL), det);
}

TEST_F(HistoryBackendDBTest, ClearBrowsingData_Downloads) {
  CreateBackendAndDatabase();

  // Initially there should be nothing in the downloads database.
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // Add a download, test that it was added correctly, remove it, test that it
  // was removed.
  Time now = Time();
  uint32 id = 1;
  EXPECT_TRUE(AddDownload(id, DownloadItem::COMPLETE, Time()));
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());

  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("current-path")),
            downloads[0].current_path);
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("target-path")),
            downloads[0].target_path);
  EXPECT_EQ(1UL, downloads[0].url_chain.size());
  EXPECT_EQ(GURL("foo-url"), downloads[0].url_chain[0]);
  EXPECT_EQ(std::string("http://referrer.com/"),
            std::string(downloads[0].referrer_url.spec()));
  EXPECT_EQ(now, downloads[0].start_time);
  EXPECT_EQ(now, downloads[0].end_time);
  EXPECT_EQ(0, downloads[0].received_bytes);
  EXPECT_EQ(512, downloads[0].total_bytes);
  EXPECT_EQ(DownloadItem::COMPLETE, downloads[0].state);
  EXPECT_EQ(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            downloads[0].danger_type);
  EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE,
            downloads[0].interrupt_reason);
  EXPECT_FALSE(downloads[0].opened);
  EXPECT_EQ("by_ext_id", downloads[0].by_ext_id);
  EXPECT_EQ("by_ext_name", downloads[0].by_ext_name);
  EXPECT_EQ("application/vnd.oasis.opendocument.text", downloads[0].mime_type);
  EXPECT_EQ("application/octet-stream", downloads[0].original_mime_type);

  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());
  db_->RemoveDownload(id);
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());
}

TEST_F(HistoryBackendDBTest, MigrateDownloadsState) {
  // Create the db we want.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    // Open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));

    // Manually insert corrupted rows; there's infrastructure in place now to
    // make this impossible, at least according to the test above.
    for (int state = 0; state < 5; ++state) {
      sql::Statement s(db.GetUniqueStatement(
            "INSERT INTO downloads (id, full_path, url, start_time, "
            "received_bytes, total_bytes, state, end_time, opened) VALUES "
            "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1 + state);
      s.BindString(1, "path");
      s.BindString(2, "url");
      s.BindInt64(3, base::Time::Now().ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, state);
      s.BindInt64(7, base::Time::Now().ToTimeT());
      s.BindInt(8, state % 2);
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate from version
  // 22 to the current version, fixing just the row whose state was 3.
  // Then close the db so that we can re-open it directly.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    {
      // The version should have been updated.
      int cur_version = HistoryDatabase::GetCurrentVersion();
      ASSERT_LT(22, cur_version);
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, state, opened "
          "FROM downloads "
          "ORDER BY id"));
      int counter = 0;
      while (statement.Step()) {
        EXPECT_EQ(1 + counter, statement.ColumnInt64(0));
        // The only thing that migration should have changed was state from 3 to
        // 4.
        EXPECT_EQ(((counter == 3) ? 4 : counter), statement.ColumnInt(1));
        EXPECT_EQ(counter % 2, statement.ColumnInt(2));
        ++counter;
      }
      EXPECT_EQ(5, counter);
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadsReasonPathsAndDangerType) {
  Time now(base::Time::Now());

  // Create the db we want.  The schema didn't change from 22->23, so just
  // re-use the v22 file.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));

    // Manually insert some rows.
    sql::Statement s(db.GetUniqueStatement(
        "INSERT INTO downloads (id, full_path, url, start_time, "
        "received_bytes, total_bytes, state, end_time, opened) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    int64 id = 0;
    // Null path.
    s.BindInt64(0, ++id);
    s.BindString(1, std::string());
    s.BindString(2, "http://whatever.com/index.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
    s.Reset(true);

    // Non-null path.
    s.BindInt64(0, ++id);
    s.BindString(1, "/path/to/some/file");
    s.BindString(2, "http://whatever.com/index1.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db using the HistoryDatabase, which should migrate from version
  // 23 to 24, creating the new tables and creating the new path, reason,
  // and danger columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    {
      // The version should have been updated.
      int cur_version = HistoryDatabase::GetCurrentVersion();
      ASSERT_LT(23, cur_version);
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      base::Time nowish(base::Time::FromTimeT(now.ToTimeT()));

      // Confirm downloads table is valid.
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, interrupt_reason, current_path, target_path, "
          "       danger_type, start_time, end_time "
          "FROM downloads ORDER BY id"));
      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(1, statement.ColumnInt64(0));
      EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE,
                statement.ColumnInt(1));
      EXPECT_EQ("", statement.ColumnString(2));
      EXPECT_EQ("", statement.ColumnString(3));
      // Implicit dependence on value of kDangerTypeNotDangerous from
      // download_database.cc.
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(5));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(6));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE,
                statement.ColumnInt(1));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(2));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(3));
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(5));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(6));

      EXPECT_FALSE(statement.Step());
    }
    {
      // Confirm downloads_url_chains table is valid.
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, chain_index, url FROM downloads_url_chains "
          " ORDER BY id, chain_index"));
      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(1, statement.ColumnInt64(0));
      EXPECT_EQ(0, statement.ColumnInt(1));
      EXPECT_EQ("http://whatever.com/index.html", statement.ColumnString(2));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(0, statement.ColumnInt(1));
      EXPECT_EQ("http://whatever.com/index1.html", statement.ColumnString(2));

      EXPECT_FALSE(statement.Step());
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateReferrer) {
  Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    sql::Statement s(db.GetUniqueStatement(
        "INSERT INTO downloads (id, full_path, url, start_time, "
        "received_bytes, total_bytes, state, end_time, opened) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    int64 db_handle = 0;
    s.BindInt64(0, ++db_handle);
    s.BindString(1, "full_path");
    s.BindString(2, "http://whatever.com/index.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
  }
  // Re-open the db using the HistoryDatabase, which should migrate to version
  // 26, creating the referrer column.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(26, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT referrer from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadedByExtension) {
  Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(26));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to version
  // 27, creating the by_ext_id and by_ext_name columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(27, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT by_ext_id, by_ext_name from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadValidators) {
  Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(27));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer, by_ext_id, by_ext_name) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      s.BindString(12, "by extension ID");
      s.BindString(13, "by extension name");
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the etag and last_modified columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(28, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT etag, last_modified from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, PurgeArchivedDatabase) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(27));
  ASSERT_NO_FATAL_FAILURE(CreateArchivedDB());

  ASSERT_TRUE(base::PathExists(
      history_dir_.Append(chrome::kArchivedHistoryFilename)));

  CreateBackendAndDatabase();
  DeleteBackend();

  // We do not retain expired history entries in an archived database as of M37.
  // Verify that any legacy archived database is deleted on start-up.
  ASSERT_FALSE(base::PathExists(
      history_dir_.Append(chrome::kArchivedHistoryFilename)));
}

TEST_F(HistoryBackendDBTest, MigrateDownloadMimeType) {
  Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(28));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer, by_ext_id, by_ext_name, etag, "
          "last_modified) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      s.BindString(12, "by extension ID");
      s.BindString(13, "by extension name");
      s.BindString(14, "etag");
      s.BindInt64(15, now.ToTimeT());
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating themime_type abd original_mime_type columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(29, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT mime_type, original_mime_type from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, ConfirmDownloadRowCreateAndDelete) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Add some downloads.
  uint32 id1 = 1, id2 = 2, id3 = 3;
  AddDownload(id1, DownloadItem::COMPLETE, now);
  AddDownload(id2, DownloadItem::COMPLETE, now + base::TimeDelta::FromDays(2));
  AddDownload(id3, DownloadItem::COMPLETE, now - base::TimeDelta::FromDays(2));

  // Confirm that resulted in the correct number of rows in the DB.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(3, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(3, statement1.ColumnInt(0));
  }

  // Delete some rows and make sure the results are still correct.
  CreateBackendAndDatabase();
  db_->RemoveDownload(id2);
  db_->RemoveDownload(id3);
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(1, statement1.ColumnInt(0));
  }
}

TEST_F(HistoryBackendDBTest, DownloadNukeRecordsMissingURLs) {
  CreateBackendAndDatabase();
  base::Time now(base::Time::Now());
  std::vector<GURL> url_chain;
  DownloadRow download(base::FilePath(FILE_PATH_LITERAL("foo-path")),
                       base::FilePath(FILE_PATH_LITERAL("foo-path")),
                       url_chain,
                       GURL(std::string()),
                       "application/octet-stream",
                       "application/octet-stream",
                       now,
                       now,
                       std::string(),
                       std::string(),
                       0,
                       512,
                       DownloadItem::COMPLETE,
                       content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                       content::DOWNLOAD_INTERRUPT_REASON_NONE,
                       1,
                       0,
                       "by_ext_id",
                       "by_ext_name");

  // Creating records without any urls should fail.
  EXPECT_FALSE(db_->CreateDownload(download));

  download.url_chain.push_back(GURL("foo-url"));
  EXPECT_TRUE(db_->CreateDownload(download));

  // Pretend that the URLs were dropped.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "DELETE FROM downloads_url_chains WHERE id=1"));
    ASSERT_TRUE(statement.Run());
  }
  CreateBackendAndDatabase();
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // QueryDownloads should have nuked the corrupt record.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    {
      sql::Statement statement(db.GetUniqueStatement(
            "SELECT count(*) from downloads"));
      ASSERT_TRUE(statement.Step());
      EXPECT_EQ(0, statement.ColumnInt(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, ConfirmDownloadInProgressCleanup) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Put an IN_PROGRESS download in the DB.
  AddDownload(1, DownloadItem::IN_PROGRESS, now);

  // Confirm that they made it into the DB unchanged.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select state, interrupt_reason from downloads"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(DownloadDatabase::kStateInProgress, statement1.ColumnInt(0));
    EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE, statement1.ColumnInt(1));
    EXPECT_FALSE(statement1.Step());
  }

  // Read in the DB through query downloads, then test that the
  // right transformation was returned.
  CreateBackendAndDatabase();
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(content::DownloadItem::INTERRUPTED, results[0].state);
  EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_CRASH,
            results[0].interrupt_reason);

  // Allow the update to propagate, shut down the DB, and confirm that
  // the query updated the on disk database as well.
  base::MessageLoop::current()->RunUntilIdle();
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select state, interrupt_reason from downloads"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(DownloadDatabase::kStateInterrupted, statement1.ColumnInt(0));
    EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_CRASH,
              statement1.ColumnInt(1));
    EXPECT_FALSE(statement1.Step());
  }
}

struct InterruptReasonAssociation {
  std::string name;
  int value;
};

// Test is dependent on interrupt reasons being listed in header file
// in order.
const InterruptReasonAssociation current_reasons[] = {
#define INTERRUPT_REASON(a, b) { #a, b },
#include "content/public/browser/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
};

// This represents a list of all reasons we've previously used;
// Do Not Remove Any Entries From This List.
const InterruptReasonAssociation historical_reasons[] = {
  {"FILE_FAILED",  1},
  {"FILE_ACCESS_DENIED",  2},
  {"FILE_NO_SPACE",  3},
  {"FILE_NAME_TOO_LONG",  5},
  {"FILE_TOO_LARGE",  6},
  {"FILE_VIRUS_INFECTED",  7},
  {"FILE_TRANSIENT_ERROR",  10},
  {"FILE_BLOCKED",  11},
  {"FILE_SECURITY_CHECK_FAILED",  12},
  {"FILE_TOO_SHORT", 13},
  {"NETWORK_FAILED",  20},
  {"NETWORK_TIMEOUT",  21},
  {"NETWORK_DISCONNECTED",  22},
  {"NETWORK_SERVER_DOWN",  23},
  {"NETWORK_INVALID_REQUEST", 24},
  {"SERVER_FAILED",  30},
  {"SERVER_NO_RANGE",  31},
  {"SERVER_PRECONDITION",  32},
  {"SERVER_BAD_CONTENT",  33},
  {"SERVER_UNAUTHORIZED", 34},
  {"SERVER_CERT_PROBLEM", 35},
  {"USER_CANCELED",  40},
  {"USER_SHUTDOWN",  41},
  {"CRASH",  50},
};

// Make sure no one has changed a DownloadInterruptReason we've previously
// persisted.
TEST_F(HistoryBackendDBTest,
       ConfirmDownloadInterruptReasonBackwardsCompatible) {
  // Are there any cases in which a historical number has been repurposed
  // for an error other than it's original?
  for (size_t i = 0; i < arraysize(current_reasons); i++) {
    const InterruptReasonAssociation& cur_reason(current_reasons[i]);
    bool found = false;

    for (size_t j = 0; j < arraysize(historical_reasons); ++j) {
      const InterruptReasonAssociation& hist_reason(historical_reasons[j]);

      if (hist_reason.value == cur_reason.value) {
        EXPECT_EQ(cur_reason.name, hist_reason.name)
            << "Same integer value used for old error \""
            << hist_reason.name
            << "\" as for new error \""
            << cur_reason.name
            << "\"." << std::endl
            << "**This will cause database conflicts with persisted values**"
            << std::endl
            << "Please assign a new, non-conflicting value for the new error.";
      }

      if (hist_reason.name == cur_reason.name) {
        EXPECT_EQ(cur_reason.value, hist_reason.value)
            << "Same name (\"" << hist_reason.name
            << "\") maps to a different value historically ("
            << hist_reason.value << ") and currently ("
            << cur_reason.value << ")" << std::endl
            << "This may cause database conflicts with persisted values"
            << std::endl
            << "If this error is the same as the old one, you should"
            << std::endl
            << "use the old value, and if it is different, you should"
            << std::endl
            << "use a new name.";

        found = true;
      }
    }

    EXPECT_TRUE(found)
        << "Error \"" << cur_reason.name << "\" not found in historical list."
        << std::endl
        << "Please add it.";
  }
}

class HistoryTest : public testing::Test {
 public:
  HistoryTest()
      : got_thumbnail_callback_(false),
        query_url_success_(false) {
  }

  virtual ~HistoryTest() {
  }

  void OnMostVisitedURLsAvailable(const MostVisitedURLList* url_list) {
    most_visited_urls_ = *url_list;
    base::MessageLoop::current()->Quit();
  }

 protected:
  friend class BackendDelegate;

  // testing::Test
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_dir_ = temp_dir_.path().AppendASCII("HistoryTest");
    ASSERT_TRUE(base::CreateDirectory(history_dir_));
    history_service_.reset(new HistoryService);
    if (!history_service_->Init(history_dir_)) {
      history_service_.reset();
      ADD_FAILURE();
    }
  }

  virtual void TearDown() {
    if (history_service_)
      CleanupHistoryService();

    // Make sure we don't have any event pending that could disrupt the next
    // test.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->Run();
  }

  void CleanupHistoryService() {
    DCHECK(history_service_);

    history_service_->ClearCachedDataForContextID(0);
    history_service_->SetOnBackendDestroyTask(base::MessageLoop::QuitClosure());
    history_service_->Cleanup();
    history_service_.reset();

    // Wait for the backend class to terminate before deleting the files and
    // moving to the next test. Note: if this never terminates, somebody is
    // probably leaking a reference to the history backend, so it never calls
    // our destroy task.
    base::MessageLoop::current()->Run();
  }

  // Fills the query_url_row_ and query_url_visits_ structures with the
  // information about the given URL and returns true. If the URL was not
  // found, this will return false and those structures will not be changed.
  bool QueryURL(HistoryService* history, const GURL& url) {
    history_service_->QueryURL(
        url,
        true,
        base::Bind(&HistoryTest::SaveURLAndQuit, base::Unretained(this)),
        &tracker_);
    base::MessageLoop::current()->Run();  // Will be exited in SaveURLAndQuit.
    return query_url_success_;
  }

  // Callback for HistoryService::QueryURL.
  void SaveURLAndQuit(bool success,
                      const URLRow& url_row,
                      const VisitVector& visits) {
    query_url_success_ = success;
    if (query_url_success_) {
      query_url_row_ = url_row;
      query_url_visits_ = visits;
    } else {
      query_url_row_ = URLRow();
      query_url_visits_.clear();
    }
    base::MessageLoop::current()->Quit();
  }

  // Fills in saved_redirects_ with the redirect information for the given URL,
  // returning true on success. False means the URL was not found.
  void QueryRedirectsFrom(HistoryService* history, const GURL& url) {
    history_service_->QueryRedirectsFrom(
        url,
        base::Bind(&HistoryTest::OnRedirectQueryComplete,
                   base::Unretained(this)),
        &tracker_);
    base::MessageLoop::current()->Run();  // Will be exited in *QueryComplete.
  }

  // Callback for QueryRedirects.
  void OnRedirectQueryComplete(const history::RedirectList* redirects) {
    saved_redirects_.clear();
    if (!redirects->empty()) {
      saved_redirects_.insert(
          saved_redirects_.end(), redirects->begin(), redirects->end());
    }
    base::MessageLoop::current()->Quit();
  }

  base::ScopedTempDir temp_dir_;

  base::MessageLoopForUI message_loop_;

  MostVisitedURLList most_visited_urls_;

  // When non-NULL, this will be deleted on tear down and we will block until
  // the backend thread has completed. This allows tests for the history
  // service to use this feature, but other tests to ignore this.
  scoped_ptr<HistoryService> history_service_;

  // names of the database files
  base::FilePath history_dir_;

  // Set by the thumbnail callback when we get data, you should be sure to
  // clear this before issuing a thumbnail request.
  bool got_thumbnail_callback_;
  std::vector<unsigned char> thumbnail_data_;

  // Set by the redirect callback when we get data. You should be sure to
  // clear this before issuing a redirect request.
  history::RedirectList saved_redirects_;

  // For history requests.
  base::CancelableTaskTracker tracker_;

  // For saving URL info after a call to QueryURL
  bool query_url_success_;
  URLRow query_url_row_;
  VisitVector query_url_visits_;
};

TEST_F(HistoryTest, AddPage) {
  ASSERT_TRUE(history_service_.get());
  // Add the page once from a child frame.
  const GURL test_url("http://www.google.com/");
  history_service_->AddPage(test_url, base::Time::Now(), NULL, 0, GURL(),
                            history::RedirectList(),
                            content::PAGE_TRANSITION_MANUAL_SUBFRAME,
                            history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(0, query_url_row_.typed_count());
  EXPECT_TRUE(query_url_row_.hidden());  // Hidden because of child frame.

  // Add the page once from the main frame (should unhide it).
  history_service_->AddPage(test_url, base::Time::Now(), NULL, 0, GURL(),
                   history::RedirectList(), content::PAGE_TRANSITION_LINK,
                   history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(2, query_url_row_.visit_count());  // Added twice.
  EXPECT_EQ(0, query_url_row_.typed_count());  // Never typed.
  EXPECT_FALSE(query_url_row_.hidden());  // Because loaded in main frame.
}

TEST_F(HistoryTest, AddRedirect) {
  ASSERT_TRUE(history_service_.get());
  const char* first_sequence[] = {
    "http://first.page.com/",
    "http://second.page.com/"};
  int first_count = arraysize(first_sequence);
  history::RedirectList first_redirects;
  for (int i = 0; i < first_count; i++)
    first_redirects.push_back(GURL(first_sequence[i]));

  // Add the sequence of pages as a server with no referrer. Note that we need
  // to have a non-NULL page ID scope.
  history_service_->AddPage(
      first_redirects.back(), base::Time::Now(),
      reinterpret_cast<ContextID>(1), 0, GURL(), first_redirects,
      content::PAGE_TRANSITION_LINK, history::SOURCE_BROWSED, true);

  // The first page should be added once with a link visit type (because we set
  // LINK when we added the original URL, and a referrer of nowhere (0).
  EXPECT_TRUE(QueryURL(history_service_.get(), first_redirects[0]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  int64 first_visit = query_url_visits_[0].visit_id;
  EXPECT_EQ(content::PAGE_TRANSITION_LINK |
            content::PAGE_TRANSITION_CHAIN_START,
            query_url_visits_[0].transition);
  EXPECT_EQ(0, query_url_visits_[0].referring_visit);  // No referrer.

  // The second page should be a server redirect type with a referrer of the
  // first page.
  EXPECT_TRUE(QueryURL(history_service_.get(), first_redirects[1]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  int64 second_visit = query_url_visits_[0].visit_id;
  EXPECT_EQ(content::PAGE_TRANSITION_SERVER_REDIRECT |
            content::PAGE_TRANSITION_CHAIN_END,
            query_url_visits_[0].transition);
  EXPECT_EQ(first_visit, query_url_visits_[0].referring_visit);

  // Check that the redirect finding function successfully reports it.
  saved_redirects_.clear();
  QueryRedirectsFrom(history_service_.get(), first_redirects[0]);
  ASSERT_EQ(1U, saved_redirects_.size());
  EXPECT_EQ(first_redirects[1], saved_redirects_[0]);

  // Now add a client redirect from that second visit to a third, client
  // redirects are tracked by the RenderView prior to updating history,
  // so we pass in a CLIENT_REDIRECT qualifier to mock that behavior.
  history::RedirectList second_redirects;
  second_redirects.push_back(first_redirects[1]);
  second_redirects.push_back(GURL("http://last.page.com/"));
  history_service_->AddPage(second_redirects[1], base::Time::Now(),
                   reinterpret_cast<ContextID>(1), 1,
                   second_redirects[0], second_redirects,
                   static_cast<content::PageTransition>(
                       content::PAGE_TRANSITION_LINK |
                       content::PAGE_TRANSITION_CLIENT_REDIRECT),
                   history::SOURCE_BROWSED, true);

  // The last page (source of the client redirect) should NOT have an
  // additional visit added, because it was a client redirect (normally it
  // would). We should only have 1 left over from the first sequence.
  EXPECT_TRUE(QueryURL(history_service_.get(), second_redirects[0]));
  EXPECT_EQ(1, query_url_row_.visit_count());

  // The final page should be set as a client redirect from the previous visit.
  EXPECT_TRUE(QueryURL(history_service_.get(), second_redirects[1]));
  EXPECT_EQ(1, query_url_row_.visit_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  EXPECT_EQ(content::PAGE_TRANSITION_CLIENT_REDIRECT |
            content::PAGE_TRANSITION_CHAIN_END,
            query_url_visits_[0].transition);
  EXPECT_EQ(second_visit, query_url_visits_[0].referring_visit);
}

TEST_F(HistoryTest, MakeIntranetURLsTyped) {
  ASSERT_TRUE(history_service_.get());

  // Add a non-typed visit to an intranet URL on an unvisited host.  This should
  // get promoted to a typed visit.
  const GURL test_url("http://intranet_host/path");
  history_service_->AddPage(
      test_url, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  EXPECT_EQ(content::PAGE_TRANSITION_TYPED,
      content::PageTransitionStripQualifier(query_url_visits_[0].transition));

  // Add more visits on the same host.  None of these should be promoted since
  // there is already a typed visit.

  // Different path.
  const GURL test_url2("http://intranet_host/different_path");
  history_service_->AddPage(
      test_url2, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url2));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(0, query_url_row_.typed_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  EXPECT_EQ(content::PAGE_TRANSITION_LINK,
      content::PageTransitionStripQualifier(query_url_visits_[0].transition));

  // No path.
  const GURL test_url3("http://intranet_host/");
  history_service_->AddPage(
      test_url3, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url3));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(0, query_url_row_.typed_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  EXPECT_EQ(content::PAGE_TRANSITION_LINK,
      content::PageTransitionStripQualifier(query_url_visits_[0].transition));

  // Different scheme.
  const GURL test_url4("https://intranet_host/");
  history_service_->AddPage(
      test_url4, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url4));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(0, query_url_row_.typed_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  EXPECT_EQ(content::PAGE_TRANSITION_LINK,
      content::PageTransitionStripQualifier(query_url_visits_[0].transition));

  // Different transition.
  const GURL test_url5("http://intranet_host/another_path");
  history_service_->AddPage(
      test_url5, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(),
      content::PAGE_TRANSITION_AUTO_BOOKMARK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url5));
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(0, query_url_row_.typed_count());
  ASSERT_EQ(1U, query_url_visits_.size());
  EXPECT_EQ(content::PAGE_TRANSITION_AUTO_BOOKMARK,
      content::PageTransitionStripQualifier(query_url_visits_[0].transition));

  // Original URL.
  history_service_->AddPage(
      test_url, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(2, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());
  ASSERT_EQ(2U, query_url_visits_.size());
  EXPECT_EQ(content::PAGE_TRANSITION_LINK,
      content::PageTransitionStripQualifier(query_url_visits_[1].transition));
}

TEST_F(HistoryTest, Typed) {
  const ContextID context_id = reinterpret_cast<ContextID>(1);

  ASSERT_TRUE(history_service_.get());

  // Add the page once as typed.
  const GURL test_url("http://www.google.com/");
  history_service_->AddPage(
      test_url, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // We should have the same typed & visit count.
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again not typed.
  history_service_->AddPage(
      test_url, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // The second time should not have updated the typed count.
  EXPECT_EQ(2, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again as a generated URL.
  history_service_->AddPage(
      test_url, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_GENERATED,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // This should have worked like a link click.
  EXPECT_EQ(3, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again as a reload.
  history_service_->AddPage(
      test_url, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_RELOAD,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // This should not have incremented any visit counts.
  EXPECT_EQ(3, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());
}

TEST_F(HistoryTest, SetTitle) {
  ASSERT_TRUE(history_service_.get());

  // Add a URL.
  const GURL existing_url("http://www.google.com/");
  history_service_->AddPage(
      existing_url, base::Time::Now(), history::SOURCE_BROWSED);

  // Set some title.
  const base::string16 existing_title = base::UTF8ToUTF16("Google");
  history_service_->SetPageTitle(existing_url, existing_title);

  // Make sure the title got set.
  EXPECT_TRUE(QueryURL(history_service_.get(), existing_url));
  EXPECT_EQ(existing_title, query_url_row_.title());

  // set a title on a nonexistent page
  const GURL nonexistent_url("http://news.google.com/");
  const base::string16 nonexistent_title = base::UTF8ToUTF16("Google News");
  history_service_->SetPageTitle(nonexistent_url, nonexistent_title);

  // Make sure nothing got written.
  EXPECT_FALSE(QueryURL(history_service_.get(), nonexistent_url));
  EXPECT_EQ(base::string16(), query_url_row_.title());

  // TODO(brettw) this should also test redirects, which get the title of the
  // destination page.
}

TEST_F(HistoryTest, MostVisitedURLs) {
  ASSERT_TRUE(history_service_.get());

  const GURL url0("http://www.google.com/url0/");
  const GURL url1("http://www.google.com/url1/");
  const GURL url2("http://www.google.com/url2/");
  const GURL url3("http://www.google.com/url3/");
  const GURL url4("http://www.google.com/url4/");

  const ContextID context_id = reinterpret_cast<ContextID>(1);

  // Add two pages.
  history_service_->AddPage(
      url0, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->AddPage(
      url1, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20,
      90,
      base::Bind(&HistoryTest::OnMostVisitedURLsAvailable,
                 base::Unretained(this)),
      &tracker_);
  base::MessageLoop::current()->Run();

  EXPECT_EQ(2U, most_visited_urls_.size());
  EXPECT_EQ(url0, most_visited_urls_[0].url);
  EXPECT_EQ(url1, most_visited_urls_[1].url);

  // Add another page.
  history_service_->AddPage(
      url2, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20,
      90,
      base::Bind(&HistoryTest::OnMostVisitedURLsAvailable,
                 base::Unretained(this)),
      &tracker_);
  base::MessageLoop::current()->Run();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url0, most_visited_urls_[0].url);
  EXPECT_EQ(url1, most_visited_urls_[1].url);
  EXPECT_EQ(url2, most_visited_urls_[2].url);

  // Revisit url2, making it the top URL.
  history_service_->AddPage(
      url2, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20,
      90,
      base::Bind(&HistoryTest::OnMostVisitedURLsAvailable,
                 base::Unretained(this)),
      &tracker_);
  base::MessageLoop::current()->Run();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url2, most_visited_urls_[0].url);
  EXPECT_EQ(url0, most_visited_urls_[1].url);
  EXPECT_EQ(url1, most_visited_urls_[2].url);

  // Revisit url1, making it the top URL.
  history_service_->AddPage(
      url1, base::Time::Now(), context_id, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20,
      90,
      base::Bind(&HistoryTest::OnMostVisitedURLsAvailable,
                 base::Unretained(this)),
      &tracker_);
  base::MessageLoop::current()->Run();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url1, most_visited_urls_[0].url);
  EXPECT_EQ(url2, most_visited_urls_[1].url);
  EXPECT_EQ(url0, most_visited_urls_[2].url);

  // Redirects
  history::RedirectList redirects;
  redirects.push_back(url3);
  redirects.push_back(url4);

  // Visit url4 using redirects.
  history_service_->AddPage(
      url4, base::Time::Now(), context_id, 0, GURL(),
      redirects, content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20,
      90,
      base::Bind(&HistoryTest::OnMostVisitedURLsAvailable,
                 base::Unretained(this)),
      &tracker_);
  base::MessageLoop::current()->Run();

  EXPECT_EQ(4U, most_visited_urls_.size());
  EXPECT_EQ(url1, most_visited_urls_[0].url);
  EXPECT_EQ(url2, most_visited_urls_[1].url);
  EXPECT_EQ(url0, most_visited_urls_[2].url);
  EXPECT_EQ(url3, most_visited_urls_[3].url);
  EXPECT_EQ(2U, most_visited_urls_[3].redirects.size());
}

namespace {

// A HistoryDBTask implementation. Each time RunOnDBThread is invoked
// invoke_count is increment. When invoked kWantInvokeCount times, true is
// returned from RunOnDBThread which should stop RunOnDBThread from being
// invoked again. When DoneRunOnMainThread is invoked, done_invoked is set to
// true.
class HistoryDBTaskImpl : public HistoryDBTask {
 public:
  static const int kWantInvokeCount;

  HistoryDBTaskImpl(int* invoke_count, bool* done_invoked)
      : invoke_count_(invoke_count), done_invoked_(done_invoked) {}

  virtual bool RunOnDBThread(HistoryBackend* backend,
                             HistoryDatabase* db) OVERRIDE {
    return (++*invoke_count_ == kWantInvokeCount);
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    *done_invoked_ = true;
    base::MessageLoop::current()->Quit();
  }

  int* invoke_count_;
  bool* done_invoked_;

 private:
  virtual ~HistoryDBTaskImpl() {}

  DISALLOW_COPY_AND_ASSIGN(HistoryDBTaskImpl);
};

// static
const int HistoryDBTaskImpl::kWantInvokeCount = 2;

}  // namespace

TEST_F(HistoryTest, HistoryDBTask) {
  ASSERT_TRUE(history_service_.get());
  base::CancelableTaskTracker task_tracker;
  int invoke_count = 0;
  bool done_invoked = false;
  history_service_->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new HistoryDBTaskImpl(&invoke_count, &done_invoked)),
      &task_tracker);
  // Run the message loop. When HistoryDBTaskImpl::DoneRunOnMainThread runs,
  // it will stop the message loop. If the test hangs here, it means
  // DoneRunOnMainThread isn't being invoked correctly.
  base::MessageLoop::current()->Run();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history_service_.reset();
  ASSERT_EQ(HistoryDBTaskImpl::kWantInvokeCount, invoke_count);
  ASSERT_TRUE(done_invoked);
}

TEST_F(HistoryTest, HistoryDBTaskCanceled) {
  ASSERT_TRUE(history_service_.get());
  base::CancelableTaskTracker task_tracker;
  int invoke_count = 0;
  bool done_invoked = false;
  history_service_->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new HistoryDBTaskImpl(&invoke_count, &done_invoked)),
      &task_tracker);
  task_tracker.TryCancelAll();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history_service_.reset();
  ASSERT_FALSE(done_invoked);
}

// Create a local delete directive and process it while sync is
// online, and then when offline. The delete directive should be sent to sync,
// no error should be returned for the first time, and an error should be
// returned for the second time.
TEST_F(HistoryTest, ProcessLocalDeleteDirectiveSyncOnline) {
  ASSERT_TRUE(history_service_.get());

  const GURL test_url("http://www.google.com/");
  for (int64 i = 1; i <= 10; ++i) {
    base::Time t =
        base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(i);
    history_service_->AddPage(test_url, t, NULL, 0, GURL(),
                              history::RedirectList(),
                              content::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false);
  }

  sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
  sync_pb::GlobalIdDirective* global_id_directive =
      delete_directive.mutable_global_id_directive();
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(1))
      .ToInternalValue());

  syncer::FakeSyncChangeProcessor change_processor;

  EXPECT_FALSE(
      history_service_->MergeDataAndStartSyncing(
                            syncer::HISTORY_DELETE_DIRECTIVES,
                            syncer::SyncDataList(),
                            scoped_ptr<syncer::SyncChangeProcessor>(
                                new syncer::SyncChangeProcessorWrapperForTest(
                                    &change_processor)),
                            scoped_ptr<syncer::SyncErrorFactory>())
          .error()
          .IsSet());

  syncer::SyncError err =
      history_service_->ProcessLocalDeleteDirective(delete_directive);
  EXPECT_FALSE(err.IsSet());
  EXPECT_EQ(1u, change_processor.changes().size());

  history_service_->StopSyncing(syncer::HISTORY_DELETE_DIRECTIVES);
  err = history_service_->ProcessLocalDeleteDirective(delete_directive);
  EXPECT_TRUE(err.IsSet());
  EXPECT_EQ(1u, change_processor.changes().size());
}

// Closure function that runs periodically to check result of delete directive
// processing. Stop when timeout or processing ends indicated by the creation
// of sync changes.
void CheckDirectiveProcessingResult(
    Time timeout,
    const syncer::FakeSyncChangeProcessor* change_processor,
    uint32 num_changes) {
  if (base::Time::Now() > timeout ||
      change_processor->changes().size() >= num_changes) {
    return;
  }

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CheckDirectiveProcessingResult, timeout,
                 change_processor, num_changes));
}

// Create a delete directive for a few specific history entries,
// including ones that don't exist. The expected entries should be
// deleted.
TEST_F(HistoryTest, ProcessGlobalIdDeleteDirective) {
  ASSERT_TRUE(history_service_.get());
  const GURL test_url("http://www.google.com/");
  for (int64 i = 1; i <= 20; i++) {
    base::Time t =
        base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(i);
    history_service_->AddPage(test_url, t, NULL, 0, GURL(),
                              history::RedirectList(),
                              content::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false);
  }

  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(20, query_url_row_.visit_count());

  syncer::SyncDataList directives;
  // 1st directive.
  sync_pb::EntitySpecifics entity_specs;
  sync_pb::GlobalIdDirective* global_id_directive =
      entity_specs.mutable_history_delete_directive()
          ->mutable_global_id_directive();
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(6))
      .ToInternalValue());
  global_id_directive->set_start_time_usec(3);
  global_id_directive->set_end_time_usec(10);
  directives.push_back(syncer::SyncData::CreateRemoteData(
      1,
      entity_specs,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));

  // 2nd directive.
  global_id_directive->Clear();
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(17))
      .ToInternalValue());
  global_id_directive->set_start_time_usec(13);
  global_id_directive->set_end_time_usec(19);
  directives.push_back(syncer::SyncData::CreateRemoteData(
      2,
      entity_specs,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(
      history_service_->MergeDataAndStartSyncing(
                            syncer::HISTORY_DELETE_DIRECTIVES,
                            directives,
                            scoped_ptr<syncer::SyncChangeProcessor>(
                                new syncer::SyncChangeProcessorWrapperForTest(
                                    &change_processor)),
                            scoped_ptr<syncer::SyncErrorFactory>())
          .error()
          .IsSet());

  // Inject a task to check status and keep message loop filled before directive
  // processing finishes.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CheckDirectiveProcessingResult,
                 base::Time::Now() + base::TimeDelta::FromSeconds(10),
                 &change_processor, 2));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  ASSERT_EQ(5, query_url_row_.visit_count());
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(1),
            query_url_visits_[0].visit_time);
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(2),
            query_url_visits_[1].visit_time);
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(11),
            query_url_visits_[2].visit_time);
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(12),
            query_url_visits_[3].visit_time);
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(20),
            query_url_visits_[4].visit_time);

  // Expect two sync changes for deleting processed directives.
  const syncer::SyncChangeList& sync_changes = change_processor.changes();
  ASSERT_EQ(2u, sync_changes.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[0].change_type());
  EXPECT_EQ(1, syncer::SyncDataRemote(sync_changes[0].sync_data()).GetId());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
  EXPECT_EQ(2, syncer::SyncDataRemote(sync_changes[1].sync_data()).GetId());
}

// Create delete directives for time ranges.  The expected entries should be
// deleted.
TEST_F(HistoryTest, ProcessTimeRangeDeleteDirective) {
  ASSERT_TRUE(history_service_.get());
  const GURL test_url("http://www.google.com/");
  for (int64 i = 1; i <= 10; ++i) {
    base::Time t =
        base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(i);
    history_service_->AddPage(test_url, t, NULL, 0, GURL(),
                              history::RedirectList(),
                              content::PAGE_TRANSITION_LINK,
                              history::SOURCE_BROWSED, false);
  }

  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(10, query_url_row_.visit_count());

  syncer::SyncDataList directives;
  // 1st directive.
  sync_pb::EntitySpecifics entity_specs;
  sync_pb::TimeRangeDirective* time_range_directive =
      entity_specs.mutable_history_delete_directive()
          ->mutable_time_range_directive();
  time_range_directive->set_start_time_usec(2);
  time_range_directive->set_end_time_usec(5);
  directives.push_back(syncer::SyncData::CreateRemoteData(
      1,
      entity_specs,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));

  // 2nd directive.
  time_range_directive->Clear();
  time_range_directive->set_start_time_usec(8);
  time_range_directive->set_end_time_usec(10);
  directives.push_back(syncer::SyncData::CreateRemoteData(
      2,
      entity_specs,
      base::Time(),
      syncer::AttachmentIdList(),
      syncer::AttachmentServiceProxyForTest::Create()));

  syncer::FakeSyncChangeProcessor change_processor;
  EXPECT_FALSE(
      history_service_->MergeDataAndStartSyncing(
                            syncer::HISTORY_DELETE_DIRECTIVES,
                            directives,
                            scoped_ptr<syncer::SyncChangeProcessor>(
                                new syncer::SyncChangeProcessorWrapperForTest(
                                    &change_processor)),
                            scoped_ptr<syncer::SyncErrorFactory>())
          .error()
          .IsSet());

  // Inject a task to check status and keep message loop filled before
  // directive processing finishes.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CheckDirectiveProcessingResult,
                 base::Time::Now() + base::TimeDelta::FromSeconds(10),
                 &change_processor, 2));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  ASSERT_EQ(3, query_url_row_.visit_count());
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(1),
            query_url_visits_[0].visit_time);
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(6),
            query_url_visits_[1].visit_time);
  EXPECT_EQ(base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(7),
            query_url_visits_[2].visit_time);

  // Expect two sync changes for deleting processed directives.
  const syncer::SyncChangeList& sync_changes = change_processor.changes();
  ASSERT_EQ(2u, sync_changes.size());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[0].change_type());
  EXPECT_EQ(1, syncer::SyncDataRemote(sync_changes[0].sync_data()).GetId());
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, sync_changes[1].change_type());
  EXPECT_EQ(2, syncer::SyncDataRemote(sync_changes[1].sync_data()).GetId());
}

TEST_F(HistoryBackendDBTest, MigratePresentations) {
  // Create the db we want. Use 22 since segments didn't change in that time
  // frame.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));

  const SegmentID segment_id = 2;
  const URLID url_id = 3;
  const GURL url("http://www.foo.com");
  const std::string url_name(VisitSegmentDatabase::ComputeSegmentName(url));
  const base::string16 title(base::ASCIIToUTF16("Title1"));
  const Time segment_time(Time::Now());

  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(chrome::kHistoryFilename)));

    // Add an entry to urls.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO urls "
                           "(id, url, title, last_visit_time) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, url_id);
      s.BindString(1, url.spec());
      s.BindString16(2, title);
      s.BindInt64(3, segment_time.ToInternalValue());
      ASSERT_TRUE(s.Run());
    }

    // Add an entry to segments.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO segments "
                           "(id, name, url_id, pres_index) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, segment_id);
      s.BindString(1, url_name);
      s.BindInt64(2, url_id);
      s.BindInt(3, 4);  // pres_index
      ASSERT_TRUE(s.Run());
    }

    // And one to segment_usage.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO segment_usage "
                           "(id, segment_id, time_slot, visit_count) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, 4);  // id.
      s.BindInt64(1, segment_id);
      s.BindInt64(2, segment_time.ToInternalValue());
      s.BindInt(3, 5);  // visit count.
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  std::vector<PageUsageData*> results;
  db_->QuerySegmentUsage(segment_time, 10, &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(url, results[0]->GetURL());
  EXPECT_EQ(segment_id, results[0]->GetID());
  EXPECT_EQ(title, results[0]->GetTitle());
  STLDeleteElements(&results);
}

}  // namespace history
