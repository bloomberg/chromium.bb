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
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_unittest_base.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/tools/profiles/thumbnail-inl.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/protocol/history_delete_directive_specifics.pb.h"
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

  virtual void NotifyProfileError(int backend_id,
                                  sql::InitStatus init_status) OVERRIDE {}
  virtual void SetInMemoryBackend(int backend_id,
                                  InMemoryHistoryBackend* backend) OVERRIDE;
  virtual void BroadcastNotifications(int type,
                                      HistoryDetails* details) OVERRIDE;
  virtual void DBLoaded(int backend_id) OVERRIDE {}
  virtual void StartTopSitesMigration(int backend_id) OVERRIDE {}
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
    backend_ = new HistoryBackend(history_dir_, 0, new BackendDelegate(this),
                                  NULL);
    backend_->Init(std::string(), false);
    db_ = backend_->db_.get();
    DCHECK(in_mem_backend_.get()) << "Mem backend should have been set by "
        "HistoryBackend::Init";
  }

  void CreateDBVersion(int version) {
    base::FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("History");
    data_path = data_path.AppendASCII(StringPrintf("history.%d.sql", version));
    ASSERT_NO_FATAL_FAILURE(
        ExecuteSQLScript(data_path, history_dir_.Append(
            chrome::kHistoryFilename)));
  }

  // testing::Test
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_dir_ = temp_dir_.path().AppendASCII("HistoryBackendDBTest");
    ASSERT_TRUE(file_util::CreateDirectory(history_dir_));
  }

  void DeleteBackend() {
    if (backend_) {
      backend_->Closing();
      backend_ = NULL;
    }
  }

  virtual void TearDown() {
    DeleteBackend();

    // Make sure we don't have any event pending that could disrupt the next
    // test.
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  int64 AddDownload(DownloadItem::DownloadState state, const Time& time) {
    std::vector<GURL> url_chain;
    url_chain.push_back(GURL("foo-url"));

    DownloadRow download(
        base::FilePath(FILE_PATH_LITERAL("foo-path")),
        base::FilePath(FILE_PATH_LITERAL("foo-path")),
        url_chain,
        GURL(""),
        time,
        time,
        0,
        512,
        state,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        content::DOWNLOAD_INTERRUPT_REASON_NONE,
        0,
        0);
    return db_->CreateDownload(download);
  }

  base::ScopedTempDir temp_dir_;

  MessageLoopForUI message_loop_;

  // names of the database files
  base::FilePath history_dir_;

  // Created via CreateBackendAndDatabase.
  scoped_refptr<HistoryBackend> backend_;
  scoped_ptr<InMemoryHistoryBackend> in_mem_backend_;
  HistoryDatabase* db_;  // Cached reference to the backend's database.
};

void BackendDelegate::SetInMemoryBackend(int backend_id,
                                         InMemoryHistoryBackend* backend) {
  // Save the in-memory backend to the history test object, this happens
  // synchronously, so we don't have to do anything fancy.
  history_test_->in_mem_backend_.reset(backend);
}

void BackendDelegate::BroadcastNotifications(int type,
                                             HistoryDetails* details) {
  // Currently, just send the notifications directly to the in-memory database.
  // We may want do do something more fancy in the future.
  content::Details<HistoryDetails> det(details);
  history_test_->in_mem_backend_->Observe(type,
      content::Source<HistoryBackendDBTest>(NULL), det);

  // The backend passes ownership of the details pointer to us.
  delete details;
}

namespace {

TEST_F(HistoryBackendDBTest, ClearBrowsingData_Downloads) {
  CreateBackendAndDatabase();

  // Initially there should be nothing in the downloads database.
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // Add a download, test that it was added, remove it, test that it was
  // removed.
  DownloadID handle;
  EXPECT_NE(0, handle = AddDownload(DownloadItem::COMPLETE, Time()));
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());
  db_->RemoveDownload(handle);
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

    int64 db_handle = 0;
    // Null path.
    s.BindInt64(0, ++db_handle);
    s.BindString(1, "");
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
    s.BindInt64(0, ++db_handle);
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

TEST_F(HistoryBackendDBTest, ConfirmDownloadRowCreateAndDelete) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Add some downloads.
  AddDownload(DownloadItem::COMPLETE, now);
  int64 did2 = AddDownload(DownloadItem::COMPLETE, now +
                           base::TimeDelta::FromDays(2));
  int64 did3 = AddDownload(DownloadItem::COMPLETE, now -
                           base::TimeDelta::FromDays(2));

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
  db_->RemoveDownload(did2);
  db_->RemoveDownload(did3);
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
  {"SERVER_FAILED",  30},
  {"SERVER_NO_RANGE",  31},
  {"SERVER_PRECONDITION",  32},
  {"SERVER_BAD_CONTENT",  33},
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

// The tracker uses RenderProcessHost pointers for scoping but never
// dereferences them. We use ints because it's easier. This function converts
// between the two.
static void* MakeFakeHost(int id) {
  void* host = 0;
  memcpy(&host, &id, sizeof(id));
  return host;
}

class HistoryTest : public testing::Test {
 public:
  HistoryTest()
      : got_thumbnail_callback_(false),
        redirect_query_success_(false),
        query_url_success_(false) {
  }

  virtual ~HistoryTest() {
  }

  void OnSegmentUsageAvailable(CancelableRequestProvider::Handle handle,
                               std::vector<PageUsageData*>* data) {
    page_usage_data_.swap(*data);
    MessageLoop::current()->Quit();
  }

  void OnDeleteURLsDone(CancelableRequestProvider::Handle handle) {
    MessageLoop::current()->Quit();
  }

  void OnMostVisitedURLsAvailable(CancelableRequestProvider::Handle handle,
                                  MostVisitedURLList url_list) {
    most_visited_urls_.swap(url_list);
    MessageLoop::current()->Quit();
  }

 protected:
  friend class BackendDelegate;

  // testing::Test
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_dir_ = temp_dir_.path().AppendASCII("HistoryTest");
    ASSERT_TRUE(file_util::CreateDirectory(history_dir_));
    history_service_.reset(new HistoryService);
    if (!history_service_->Init(history_dir_, NULL)) {
      history_service_.reset();
      ADD_FAILURE();
    }
  }

  virtual void TearDown() {
    if (history_service_.get())
      CleanupHistoryService();

    // Make sure we don't have any event pending that could disrupt the next
    // test.
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  void CleanupHistoryService() {
    DCHECK(history_service_.get());

    history_service_->NotifyRenderProcessHostDestruction(0);
    history_service_->SetOnBackendDestroyTask(MessageLoop::QuitClosure());
    history_service_->Cleanup();
    history_service_.reset();

    // Wait for the backend class to terminate before deleting the files and
    // moving to the next test. Note: if this never terminates, somebody is
    // probably leaking a reference to the history backend, so it never calls
    // our destroy task.
    MessageLoop::current()->Run();
  }

  // Fills the query_url_row_ and query_url_visits_ structures with the
  // information about the given URL and returns true. If the URL was not
  // found, this will return false and those structures will not be changed.
  bool QueryURL(HistoryService* history, const GURL& url) {
    history_service_->QueryURL(url, true, &consumer_,
                               base::Bind(&HistoryTest::SaveURLAndQuit,
                                          base::Unretained(this)));
    MessageLoop::current()->Run();  // Will be exited in SaveURLAndQuit.
    return query_url_success_;
  }

  // Callback for HistoryService::QueryURL.
  void SaveURLAndQuit(HistoryService::Handle handle,
                      bool success,
                      const URLRow* url_row,
                      VisitVector* visit_vector) {
    query_url_success_ = success;
    if (query_url_success_) {
      query_url_row_ = *url_row;
      query_url_visits_.swap(*visit_vector);
    } else {
      query_url_row_ = URLRow();
      query_url_visits_.clear();
    }
    MessageLoop::current()->Quit();
  }

  // Fills in saved_redirects_ with the redirect information for the given URL,
  // returning true on success. False means the URL was not found.
  bool QueryRedirectsFrom(HistoryService* history, const GURL& url) {
    history_service_->QueryRedirectsFrom(
        url, &consumer_,
        base::Bind(&HistoryTest::OnRedirectQueryComplete,
                   base::Unretained(this)));
    MessageLoop::current()->Run();  // Will be exited in *QueryComplete.
    return redirect_query_success_;
  }

  // Callback for QueryRedirects.
  void OnRedirectQueryComplete(HistoryService::Handle handle,
                               GURL url,
                               bool success,
                               history::RedirectList* redirects) {
    redirect_query_success_ = success;
    if (redirect_query_success_)
      saved_redirects_.swap(*redirects);
    else
      saved_redirects_.clear();
    MessageLoop::current()->Quit();
  }

  base::ScopedTempDir temp_dir_;

  MessageLoopForUI message_loop_;

  // PageUsageData vector to test segments.
  ScopedVector<PageUsageData> page_usage_data_;

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
  bool redirect_query_success_;

  // For history requests.
  CancelableRequestConsumer consumer_;

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
      first_redirects.back(), base::Time::Now(), MakeFakeHost(1),
      0, GURL(), first_redirects, content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, true);

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
                   MakeFakeHost(1), 1, second_redirects[0], second_redirects,
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
  ASSERT_TRUE(history_service_.get());

  // Add the page once as typed.
  const GURL test_url("http://www.google.com/");
  history_service_->AddPage(
      test_url, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // We should have the same typed & visit count.
  EXPECT_EQ(1, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again not typed.
  history_service_->AddPage(
      test_url, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // The second time should not have updated the typed count.
  EXPECT_EQ(2, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again as a generated URL.
  history_service_->AddPage(
      test_url, base::Time::Now(), NULL, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_GENERATED,
      history::SOURCE_BROWSED, false);
  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));

  // This should have worked like a link click.
  EXPECT_EQ(3, query_url_row_.visit_count());
  EXPECT_EQ(1, query_url_row_.typed_count());

  // Add the page again as a reload.
  history_service_->AddPage(
      test_url, base::Time::Now(), NULL, 0, GURL(),
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
  const string16 existing_title = UTF8ToUTF16("Google");
  history_service_->SetPageTitle(existing_url, existing_title);

  // Make sure the title got set.
  EXPECT_TRUE(QueryURL(history_service_.get(), existing_url));
  EXPECT_EQ(existing_title, query_url_row_.title());

  // set a title on a nonexistent page
  const GURL nonexistent_url("http://news.google.com/");
  const string16 nonexistent_title = UTF8ToUTF16("Google News");
  history_service_->SetPageTitle(nonexistent_url, nonexistent_title);

  // Make sure nothing got written.
  EXPECT_FALSE(QueryURL(history_service_.get(), nonexistent_url));
  EXPECT_EQ(string16(), query_url_row_.title());

  // TODO(brettw) this should also test redirects, which get the title of the
  // destination page.
}

// crbug.com/159387: This test fails when daylight savings time ends.
TEST_F(HistoryTest, DISABLED_Segments) {
  ASSERT_TRUE(history_service_.get());

  static const void* scope = static_cast<void*>(this);

  // Add a URL.
  const GURL existing_url("http://www.google.com/");
  history_service_->AddPage(
      existing_url, base::Time::Now(), scope, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);

  // Make sure a segment was created.
  history_service_->QuerySegmentUsageSince(
      &consumer_, Time::Now() - TimeDelta::FromDays(1), 10,
      base::Bind(&HistoryTest::OnSegmentUsageAvailable,
                 base::Unretained(this)));

  // Wait for processing.
  MessageLoop::current()->Run();

  ASSERT_EQ(1U, page_usage_data_.size());
  EXPECT_TRUE(page_usage_data_[0]->GetURL() == existing_url);
  EXPECT_DOUBLE_EQ(3.0, page_usage_data_[0]->GetScore());

  // Add a URL which doesn't create a segment.
  const GURL link_url("http://yahoo.com/");
  history_service_->AddPage(
      link_url, base::Time::Now(), scope, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_LINK,
      history::SOURCE_BROWSED, false);

  // Query again
  history_service_->QuerySegmentUsageSince(
      &consumer_, Time::Now() - TimeDelta::FromDays(1), 10,
      base::Bind(&HistoryTest::OnSegmentUsageAvailable,
                 base::Unretained(this)));

  // Wait for processing.
  MessageLoop::current()->Run();

  // Make sure we still have one segment.
  ASSERT_EQ(1U, page_usage_data_.size());
  EXPECT_TRUE(page_usage_data_[0]->GetURL() == existing_url);

  // Add a page linked from existing_url.
  history_service_->AddPage(
      GURL("http://www.google.com/foo"), base::Time::Now(),
      scope, 3, existing_url, history::RedirectList(),
      content::PAGE_TRANSITION_LINK, history::SOURCE_BROWSED,
      false);

  // Query again
  history_service_->QuerySegmentUsageSince(
      &consumer_, Time::Now() - TimeDelta::FromDays(1), 10,
      base::Bind(&HistoryTest::OnSegmentUsageAvailable,
                 base::Unretained(this)));

  // Wait for processing.
  MessageLoop::current()->Run();

  // Make sure we still have one segment.
  ASSERT_EQ(1U, page_usage_data_.size());
  EXPECT_TRUE(page_usage_data_[0]->GetURL() == existing_url);

  // However, the score should have increased.
  EXPECT_GT(page_usage_data_[0]->GetScore(), 5.0);
}

TEST_F(HistoryTest, MostVisitedURLs) {
  ASSERT_TRUE(history_service_.get());

  const GURL url0("http://www.google.com/url0/");
  const GURL url1("http://www.google.com/url1/");
  const GURL url2("http://www.google.com/url2/");
  const GURL url3("http://www.google.com/url3/");
  const GURL url4("http://www.google.com/url4/");

  static const void* scope = static_cast<void*>(this);

  // Add two pages.
  history_service_->AddPage(
      url0, base::Time::Now(), scope, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->AddPage(
      url1, base::Time::Now(), scope, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20, 90, &consumer_,
      base::Bind(
          &HistoryTest::OnMostVisitedURLsAvailable,
          base::Unretained(this)));
  MessageLoop::current()->Run();

  EXPECT_EQ(2U, most_visited_urls_.size());
  EXPECT_EQ(url0, most_visited_urls_[0].url);
  EXPECT_EQ(url1, most_visited_urls_[1].url);

  // Add another page.
  history_service_->AddPage(
      url2, base::Time::Now(), scope, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20, 90, &consumer_,
      base::Bind(
          &HistoryTest::OnMostVisitedURLsAvailable,
          base::Unretained(this)));
  MessageLoop::current()->Run();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url0, most_visited_urls_[0].url);
  EXPECT_EQ(url1, most_visited_urls_[1].url);
  EXPECT_EQ(url2, most_visited_urls_[2].url);

  // Revisit url2, making it the top URL.
  history_service_->AddPage(
      url2, base::Time::Now(), scope, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20, 90, &consumer_,
      base::Bind(
          &HistoryTest::OnMostVisitedURLsAvailable,
          base::Unretained(this)));
  MessageLoop::current()->Run();

  EXPECT_EQ(3U, most_visited_urls_.size());
  EXPECT_EQ(url2, most_visited_urls_[0].url);
  EXPECT_EQ(url0, most_visited_urls_[1].url);
  EXPECT_EQ(url1, most_visited_urls_[2].url);

  // Revisit url1, making it the top URL.
  history_service_->AddPage(
      url1, base::Time::Now(), scope, 0, GURL(),
      history::RedirectList(), content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20, 90, &consumer_,
      base::Bind(
          &HistoryTest::OnMostVisitedURLsAvailable,
          base::Unretained(this)));
  MessageLoop::current()->Run();

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
      url4, base::Time::Now(), scope, 0, GURL(),
      redirects, content::PAGE_TRANSITION_TYPED,
      history::SOURCE_BROWSED, false);
  history_service_->QueryMostVisitedURLs(
      20, 90, &consumer_,
      base::Bind(
          &HistoryTest::OnMostVisitedURLsAvailable,
          base::Unretained(this)));
  MessageLoop::current()->Run();

  EXPECT_EQ(4U, most_visited_urls_.size());
  EXPECT_EQ(url1, most_visited_urls_[0].url);
  EXPECT_EQ(url2, most_visited_urls_[1].url);
  EXPECT_EQ(url0, most_visited_urls_[2].url);
  EXPECT_EQ(url3, most_visited_urls_[3].url);
  EXPECT_EQ(2U, most_visited_urls_[3].redirects.size());
}

// The version of the history database should be current in the "typical
// history" example file or it will be imported on startup, throwing off timing
// measurements.
//
// See test/data/profiles/profile_with_default_theme/README.txt for
// instructions on how to up the version.
TEST(HistoryProfileTest, TypicalProfileVersion) {
  base::FilePath file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &file));
  file = file.AppendASCII("profiles");
  file = file.AppendASCII("profile_with_default_theme");
  file = file.AppendASCII("Default");
  file = file.AppendASCII("History");

  int cur_version = HistoryDatabase::GetCurrentVersion();

  sql::Connection db;
  ASSERT_TRUE(db.Open(file));

  {
    sql::Statement s(db.GetUniqueStatement(
        "SELECT value FROM meta WHERE key = 'version'"));
    EXPECT_TRUE(s.Step());
    int file_version = s.ColumnInt(0);
    EXPECT_EQ(cur_version, file_version);
  }
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

  HistoryDBTaskImpl() : invoke_count(0), done_invoked(false) {}

  virtual bool RunOnDBThread(HistoryBackend* backend,
                             HistoryDatabase* db) OVERRIDE {
    return (++invoke_count == kWantInvokeCount);
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    done_invoked = true;
    MessageLoop::current()->Quit();
  }

  int invoke_count;
  bool done_invoked;

 private:
  virtual ~HistoryDBTaskImpl() {}

  DISALLOW_COPY_AND_ASSIGN(HistoryDBTaskImpl);
};

// static
const int HistoryDBTaskImpl::kWantInvokeCount = 2;

}  // namespace

TEST_F(HistoryTest, HistoryDBTask) {
  ASSERT_TRUE(history_service_.get());
  CancelableRequestConsumerT<int, 0> request_consumer;
  scoped_refptr<HistoryDBTaskImpl> task(new HistoryDBTaskImpl());
  history_service_->ScheduleDBTask(task.get(), &request_consumer);
  // Run the message loop. When HistoryDBTaskImpl::DoneRunOnMainThread runs,
  // it will stop the message loop. If the test hangs here, it means
  // DoneRunOnMainThread isn't being invoked correctly.
  MessageLoop::current()->Run();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history_service_.reset();
  ASSERT_EQ(HistoryDBTaskImpl::kWantInvokeCount, task->invoke_count);
  ASSERT_TRUE(task->done_invoked);
}

TEST_F(HistoryTest, HistoryDBTaskCanceled) {
  ASSERT_TRUE(history_service_.get());
  CancelableRequestConsumerT<int, 0> request_consumer;
  scoped_refptr<HistoryDBTaskImpl> task(new HistoryDBTaskImpl());
  history_service_->ScheduleDBTask(task.get(), &request_consumer);
  request_consumer.CancelAllRequests();
  CleanupHistoryService();
  // WARNING: history has now been deleted.
  history_service_.reset();
  ASSERT_FALSE(task->done_invoked);
}

// Create a delete directive for a few specific history entries,
// including ones that don't exist.  The expected entries should be
// deleted.
TEST_F(HistoryTest, ProcessGlobalIdDeleteDirective) {
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

  sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
  sync_pb::GlobalIdDirective* global_id_directive =
      delete_directive.mutable_global_id_directive();
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(0))
      .ToInternalValue());
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(2))
      .ToInternalValue());
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(5))
      .ToInternalValue());
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(10))
      .ToInternalValue());
  global_id_directive->add_global_id(
      (base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(20))
      .ToInternalValue());

  history_service_->ProcessDeleteDirectiveForTest(delete_directive);

  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(7, query_url_row_.visit_count());
}

// Create a delete directive for a given time range.  The expected
// entries should be deleted.
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

  sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
  sync_pb::TimeRangeDirective* time_range_directive =
      delete_directive.mutable_time_range_directive();
  time_range_directive->set_start_time_usec(2);
  time_range_directive->set_end_time_usec(9);

  history_service_->ProcessDeleteDirectiveForTest(delete_directive);

  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(2, query_url_row_.visit_count());
}

// Create a local delete directive and process it while sync is
// offline.  The expected entries should be deleted, and an error
// should be returned.
TEST_F(HistoryTest, ProcessLocalDeleteDirectiveSyncOffline) {
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

  EXPECT_TRUE(
      history_service_->ProcessLocalDeleteDirective(delete_directive).IsSet());

  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(9, query_url_row_.visit_count());
}

// Dummy SyncChangeProcessor used to help review what SyncChanges are pushed
// back up to Sync.
//
// TODO(akalin): Unify all the various test implementations of
// syncer::SyncChangeProcessor.
class TestChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestChangeProcessor() {}
  virtual ~TestChangeProcessor() {}

  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    changes_.insert(changes_.end(), change_list.begin(), change_list.end());
    return syncer::SyncError();
  }

  const syncer::SyncChangeList& GetChanges() const {
    return changes_;
  }

 private:
  syncer::SyncChangeList changes_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeProcessor);
};

// SyncChangeProcessor implementation that delegates to another one.
// This is necessary since most things expect a
// scoped_ptr<SyncChangeProcessor>.
//
// TODO(akalin): Unify this too.
class SyncChangeProcessorDelegate : public syncer::SyncChangeProcessor {
 public:
  explicit SyncChangeProcessorDelegate(syncer::SyncChangeProcessor* recipient)
      : recipient_(recipient) {
    DCHECK(recipient_);
  }

  virtual ~SyncChangeProcessorDelegate() {}

  // syncer::SyncChangeProcessor implementation.
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    return recipient_->ProcessSyncChanges(from_here, change_list);
  }

 private:
  // The recipient of all sync changes.
  syncer::SyncChangeProcessor* const recipient_;

  DISALLOW_COPY_AND_ASSIGN(SyncChangeProcessorDelegate);
};

// Create a local delete directive and process it while sync is
// online, and then when offline.  The expected entries should be
// deleted, the delete directive should be sent to sync, no error
// should be returned for the first time, and an error should be
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

  TestChangeProcessor change_processor;

  EXPECT_FALSE(
      history_service_->MergeDataAndStartSyncing(
          syncer::HISTORY_DELETE_DIRECTIVES,
          syncer::SyncDataList(),
          scoped_ptr<syncer::SyncChangeProcessor>(
              new SyncChangeProcessorDelegate(&change_processor)),
          scoped_ptr<syncer::SyncErrorFactory>()).error().IsSet());
  EXPECT_FALSE(
      history_service_->ProcessLocalDeleteDirective(delete_directive).IsSet());
  EXPECT_EQ(1u, change_processor.GetChanges().size());

  EXPECT_TRUE(QueryURL(history_service_.get(), test_url));
  EXPECT_EQ(9, query_url_row_.visit_count());

  history_service_->StopSyncing(syncer::HISTORY_DELETE_DIRECTIVES);
  EXPECT_TRUE(
      history_service_->ProcessLocalDeleteDirective(delete_directive).IsSet());
  EXPECT_EQ(1u, change_processor.GetChanges().size());
}

TEST_F(HistoryBackendDBTest, MigratePresentations) {
  // Create the db we want. Use 22 since segments didn't change in that time
  // frame.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));

  const SegmentID segment_id = 2;
  const URLID url_id = 3;
  const GURL url("http://www.foo.com");
  const std::string url_name(VisitSegmentDatabase::ComputeSegmentName(url));
  const string16 title(ASCIIToUTF16("Title1"));
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

}  // namespace

}  // namespace history
