// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/test/history_backend_db_base_test.h"

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/test/database_test_utils.h"
#include "components/history/core/test/test_history_database.h"
#include "url/gurl.h"

namespace history {

// Delegate class for when we create a backend without a HistoryService.
class BackendDelegate : public HistoryBackend::Delegate {
 public:
  explicit BackendDelegate(HistoryBackendDBBaseTest* history_test)
      : history_test_(history_test) {}

  // HistoryBackend::Delegate implementation.
  void NotifyProfileError(sql::InitStatus init_status) override {
    history_test_->last_profile_error_ = init_status;
  }
  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {
    // Save the in-memory backend to the history test object, this happens
    // synchronously, so we don't have to do anything fancy.
    history_test_->in_mem_backend_.swap(backend);
  }
  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {}
  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        base::Time visit_time) override {}
  void NotifyURLsModified(const URLRows& changed_urls) override {}
  void NotifyURLsDeleted(bool all_history,
                         bool expired,
                         const URLRows& deleted_rows,
                         const std::set<GURL>& favicon_urls) override {}
  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const base::string16& term) override {}
  void NotifyKeywordSearchTermDeleted(URLID url_id) override {}
  void DBLoaded() override {}

 private:
  HistoryBackendDBBaseTest* history_test_;
};

HistoryBackendDBBaseTest::HistoryBackendDBBaseTest()
    : db_(nullptr),
      last_profile_error_ (sql::INIT_OK) {
}

HistoryBackendDBBaseTest::~HistoryBackendDBBaseTest() {
}

void HistoryBackendDBBaseTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  history_dir_ = temp_dir_.path().AppendASCII("HistoryBackendDBBaseTest");
  ASSERT_TRUE(base::CreateDirectory(history_dir_));
}

void HistoryBackendDBBaseTest::TearDown() {
  DeleteBackend();

  // Make sure we don't have any event pending that could disrupt the next
  // test.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  base::MessageLoop::current()->Run();
}

void HistoryBackendDBBaseTest::CreateBackendAndDatabase() {
  backend_ = new HistoryBackend(new BackendDelegate(this), nullptr,
                                base::ThreadTaskRunnerHandle::Get());
  backend_->Init(false,
                 TestHistoryDatabaseParamsForPath(history_dir_));
  db_ = backend_->db_.get();
  DCHECK(in_mem_backend_) << "Mem backend should have been set by "
      "HistoryBackend::Init";
}

void HistoryBackendDBBaseTest::CreateBackendAndDatabaseAllowFail() {
  backend_ = new HistoryBackend(new BackendDelegate(this), nullptr,
                                base::ThreadTaskRunnerHandle::Get());
  backend_->Init(false,
                 TestHistoryDatabaseParamsForPath(history_dir_));
  db_ = backend_->db_.get();
}

void HistoryBackendDBBaseTest::CreateDBVersion(int version) {
  base::FilePath data_path;
  ASSERT_TRUE(GetTestDataHistoryDir(&data_path));
  data_path =
      data_path.AppendASCII(base::StringPrintf("history.%d.sql", version));
  ASSERT_NO_FATAL_FAILURE(
      ExecuteSQLScript(data_path, history_dir_.Append(kHistoryFilename)));
}

void HistoryBackendDBBaseTest::DeleteBackend() {
  if (backend_.get()) {
    backend_->Closing();
    backend_ = nullptr;
  }
}

bool HistoryBackendDBBaseTest::AddDownload(uint32_t id,
                                           const std::string& guid,
                                           DownloadState state,
                                           base::Time time) {
  std::vector<GURL> url_chain;
  url_chain.push_back(GURL("foo-url"));

  DownloadRow download(
      base::FilePath(FILE_PATH_LITERAL("current-path")),
      base::FilePath(FILE_PATH_LITERAL("target-path")), url_chain,
      GURL("http://referrer.example.com/"), GURL("http://site-url.example.com"),
      GURL("http://tab-url.example.com/"),
      GURL("http://tab-referrer-url.example.com/"), std::string(),
      "application/vnd.oasis.opendocument.text", "application/octet-stream",
      time, time, std::string(), std::string(), 0, 512, state,
      DownloadDangerType::NOT_DANGEROUS, kTestDownloadInterruptReasonNone,
      std::string(), id, guid, false, "by_ext_id", "by_ext_name");
  return db_->CreateDownload(download);
}

}  // namespace history
