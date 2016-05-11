// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_history.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "components/history/content/browser/download_constants_utils.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#endif

using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::ReturnRefOfCopy;
using testing::SetArgPointee;
using testing::WithArg;
using testing::_;

namespace {

typedef DownloadHistory::IdSet IdSet;
typedef std::vector<history::DownloadRow> InfoVector;
typedef testing::StrictMock<content::MockDownloadItem> StrictMockDownloadItem;

class FakeHistoryAdapter : public DownloadHistory::HistoryAdapter {
 public:
  FakeHistoryAdapter()
    : DownloadHistory::HistoryAdapter(NULL),
      slow_create_download_(false),
      fail_create_download_(false) {
  }

  ~FakeHistoryAdapter() override {}

  void QueryDownloads(
      const history::HistoryService::DownloadQueryCallback& callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeHistoryAdapter::QueryDownloadsDone,
            base::Unretained(this), callback));
  }

  void QueryDownloadsDone(
      const history::HistoryService::DownloadQueryCallback& callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(expect_query_downloads_.get());
    callback.Run(std::move(expect_query_downloads_));
  }

  void set_slow_create_download(bool slow) { slow_create_download_ = slow; }

  void CreateDownload(const history::DownloadRow& info,
                      const history::HistoryService::DownloadCreateCallback&
                          callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    create_download_info_ = info;
    // Must not call CreateDownload() again before FinishCreateDownload()!
    DCHECK(create_download_callback_.is_null());
    create_download_callback_ = base::Bind(callback, !fail_create_download_);
    fail_create_download_ = false;
    if (!slow_create_download_)
      FinishCreateDownload();
  }

  void FinishCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    create_download_callback_.Run();
    create_download_callback_.Reset();
  }

  void UpdateDownload(const history::DownloadRow& info) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    update_download_ = info;
  }

  void RemoveDownloads(const IdSet& ids) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    for (IdSet::const_iterator it = ids.begin();
         it != ids.end(); ++it) {
      remove_downloads_.insert(*it);
    }
  }

  void ExpectWillQueryDownloads(std::unique_ptr<InfoVector> infos) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    expect_query_downloads_ = std::move(infos);
  }

  void ExpectQueryDownloadsDone() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    EXPECT_TRUE(NULL == expect_query_downloads_.get());
  }

  void FailCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    fail_create_download_ = true;
  }

  void ExpectDownloadCreated(
      const history::DownloadRow& info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(info, create_download_info_);
    create_download_info_ = history::DownloadRow();
  }

  void ExpectNoDownloadCreated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(history::DownloadRow(), create_download_info_);
  }

  void ExpectDownloadUpdated(const history::DownloadRow& info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(update_download_, info);
    update_download_ = history::DownloadRow();
  }

  void ExpectNoDownloadUpdated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(history::DownloadRow(), update_download_);
  }

  void ExpectNoDownloadsRemoved() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(0, static_cast<int>(remove_downloads_.size()));
  }

  void ExpectDownloadsRemoved(const IdSet& ids) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    IdSet differences = base::STLSetDifference<IdSet>(ids, remove_downloads_);
    for (IdSet::const_iterator different = differences.begin();
         different != differences.end(); ++different) {
      EXPECT_TRUE(false) << *different;
    }
    remove_downloads_.clear();
  }

 private:
  bool slow_create_download_;
  bool fail_create_download_;
  base::Closure create_download_callback_;
  history::DownloadRow update_download_;
  std::unique_ptr<InfoVector> expect_query_downloads_;
  IdSet remove_downloads_;
  history::DownloadRow create_download_info_;

  DISALLOW_COPY_AND_ASSIGN(FakeHistoryAdapter);
};

class TestDownloadHistoryObserver : public DownloadHistory::Observer {
 public:
  void OnHistoryQueryComplete() override {
    on_history_query_complete_called_ = true;
  }
  bool on_history_query_complete_called_ = false;
};

class DownloadHistoryTest : public testing::Test {
 public:
  // Generic callback that receives a pointer to a StrictMockDownloadItem.
  typedef base::Callback<void(content::MockDownloadItem*)> DownloadItemCallback;

  DownloadHistoryTest()
      : ui_thread_(content::BrowserThread::UI, &loop_),
        manager_(new content::MockDownloadManager()),
        history_(NULL),
        manager_observer_(NULL),
        download_created_index_(0) {}
  ~DownloadHistoryTest() override { STLDeleteElements(&items_); }

 protected:
  void TearDown() override { download_history_.reset(); }

  content::MockDownloadManager& manager() { return *manager_.get(); }
  content::MockDownloadItem& item(size_t index) { return *items_[index]; }
  DownloadHistory* download_history() { return download_history_.get(); }

  void SetManagerObserver(
      content::DownloadManager::Observer* manager_observer) {
    manager_observer_ = manager_observer;
  }
  content::DownloadManager::Observer* manager_observer() {
    return manager_observer_;
  }

  void CreateDownloadHistory(std::unique_ptr<InfoVector> infos) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(infos.get());
    EXPECT_CALL(manager(), AddObserver(_)).WillOnce(WithArg<0>(Invoke(
        this, &DownloadHistoryTest::SetManagerObserver)));
    EXPECT_CALL(manager(), RemoveObserver(_));
    download_created_index_ = 0;
    for (size_t index = 0; index < infos->size(); ++index) {
      const history::DownloadRow& row = infos->at(index);
      content::MockDownloadManager::CreateDownloadItemAdapter adapter(
          row.guid, history::ToContentDownloadId(row.id), row.current_path,
          row.target_path, row.url_chain, row.referrer_url, row.site_url,
          row.tab_url, row.tab_referrer_url, row.mime_type,
          row.original_mime_type, row.start_time, row.end_time, row.etag,
          row.last_modified, row.received_bytes, row.total_bytes, std::string(),
          history::ToContentDownloadState(row.state),
          history::ToContentDownloadDangerType(row.danger_type),
          history::ToContentDownloadInterruptReason(row.interrupt_reason),
          row.opened);
      EXPECT_CALL(manager(), MockCreateDownloadItem(adapter))
        .WillOnce(DoAll(
            InvokeWithoutArgs(
                this, &DownloadHistoryTest::CallOnDownloadCreatedInOrder),
            Return(&item(index))));
    }
    EXPECT_CALL(manager(), CheckForHistoryFilesRemoval());
    history_ = new FakeHistoryAdapter();
    history_->ExpectWillQueryDownloads(std::move(infos));
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_)).WillRepeatedly(Return());
    download_history_.reset(new DownloadHistory(
        &manager(),
        std::unique_ptr<DownloadHistory::HistoryAdapter>(history_)));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    history_->ExpectQueryDownloadsDone();
  }

  void CallOnDownloadCreated(size_t index) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!pre_on_create_handler_.is_null())
      pre_on_create_handler_.Run(&item(index));
    manager_observer()->OnDownloadCreated(&manager(), &item(index));
    if (!post_on_create_handler_.is_null())
      post_on_create_handler_.Run(&item(index));
  }

  void CallOnDownloadCreatedInOrder() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Gmock doesn't appear to support something like InvokeWithTheseArgs. Maybe
    // gmock needs to learn about base::Callback.
    CallOnDownloadCreated(download_created_index_++);
  }

  void set_slow_create_download(bool slow) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->set_slow_create_download(slow);
  }

  void FinishCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->FinishCreateDownload();
  }

  void FailCreateDownload() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->FailCreateDownload();
  }

  void ExpectDownloadCreated(
      const history::DownloadRow& info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectDownloadCreated(info);
  }

  void ExpectNoDownloadCreated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectNoDownloadCreated();
  }

  void ExpectDownloadUpdated(const history::DownloadRow& info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectDownloadUpdated(info);
  }

  void ExpectNoDownloadUpdated() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectNoDownloadUpdated();
  }

  void ExpectNoDownloadsRemoved() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectNoDownloadsRemoved();
  }

  void ExpectDownloadsRemoved(const IdSet& ids) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    history_->ExpectDownloadsRemoved(ids);
  }

  void ExpectDownloadsRestoredFromHistory(bool expected_value) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    pre_on_create_handler_ =
        base::Bind(&DownloadHistoryTest::CheckDownloadWasRestoredFromHistory,
                   base::Unretained(this),
                   expected_value);
    post_on_create_handler_ =
        base::Bind(&DownloadHistoryTest::CheckDownloadWasRestoredFromHistory,
                   base::Unretained(this),
                   expected_value);
  }

  void InitBasicItem(const base::FilePath::CharType* path,
                     const char* url_string,
                     const char* referrer_string,
                     history::DownloadRow* info) {
    GURL url(url_string);
    GURL referrer(referrer_string);
    std::vector<GURL> url_chain;
    url_chain.push_back(url);
    InitItem(base::GenerateGUID(), static_cast<uint32_t>(items_.size() + 1),
             base::FilePath(path), base::FilePath(path), url_chain, referrer,
             GURL("http://example.com"), GURL("http://example.com/tab-url"),
             GURL("http://example.com/tab-referrer-url"),
             "application/octet-stream", "application/octet-stream",
             (base::Time::Now() - base::TimeDelta::FromMinutes(10)),
             (base::Time::Now() - base::TimeDelta::FromMinutes(1)), "Etag",
             "abc", 100, 100, content::DownloadItem::COMPLETE,
             content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
             content::DOWNLOAD_INTERRUPT_REASON_NONE, false, std::string(),
             std::string(), info);
  }

  void InitItem(const std::string& guid,
                uint32_t id,
                const base::FilePath& current_path,
                const base::FilePath& target_path,
                const std::vector<GURL>& url_chain,
                const GURL& referrer,
                const GURL& site_url,
                const GURL& tab_url,
                const GURL& tab_referrer_url,
                const std::string& mime_type,
                const std::string& original_mime_type,
                const base::Time& start_time,
                const base::Time& end_time,
                const std::string& etag,
                const std::string& last_modified,
                int64_t received_bytes,
                int64_t total_bytes,
                content::DownloadItem::DownloadState state,
                content::DownloadDangerType danger_type,
                content::DownloadInterruptReason interrupt_reason,
                bool opened,
                const std::string& by_extension_id,
                const std::string& by_extension_name,
                history::DownloadRow* info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    size_t index = items_.size();
    StrictMockDownloadItem* mock_item = new StrictMockDownloadItem();
    items_.push_back(mock_item);

    info->current_path = current_path;
    info->target_path = target_path;
    info->url_chain = url_chain;
    info->referrer_url = referrer;
    info->site_url = site_url;
    info->tab_url = tab_url;
    info->tab_referrer_url = tab_referrer_url;
    info->mime_type = mime_type;
    info->original_mime_type = original_mime_type;
    info->start_time = start_time;
    info->end_time = end_time;
    info->etag = etag;
    info->last_modified = last_modified;
    info->received_bytes = received_bytes;
    info->total_bytes = total_bytes;
    info->state = history::ToHistoryDownloadState(state);
    info->danger_type = history::ToHistoryDownloadDangerType(danger_type);
    info->interrupt_reason =
        history::ToHistoryDownloadInterruptReason(interrupt_reason);
    info->id = history::ToHistoryDownloadId(id);
    info->guid = guid;
    info->opened = opened;
    info->by_ext_id = by_extension_id;
    info->by_ext_name = by_extension_name;

    EXPECT_CALL(item(index), GetId()).WillRepeatedly(Return(id));
    EXPECT_CALL(item(index), GetGuid()).WillRepeatedly(ReturnRefOfCopy(guid));
    EXPECT_CALL(item(index), GetFullPath())
        .WillRepeatedly(ReturnRefOfCopy(current_path));
    EXPECT_CALL(item(index), GetTargetFilePath())
        .WillRepeatedly(ReturnRefOfCopy(target_path));
    DCHECK_LE(1u, url_chain.size());
    EXPECT_CALL(item(index), GetURL())
        .WillRepeatedly(ReturnRefOfCopy(url_chain[0]));
    EXPECT_CALL(item(index), GetUrlChain())
        .WillRepeatedly(ReturnRefOfCopy(url_chain));
    EXPECT_CALL(item(index), GetMimeType()).WillRepeatedly(Return(mime_type));
    EXPECT_CALL(item(index), GetOriginalMimeType()).WillRepeatedly(Return(
        original_mime_type));
    EXPECT_CALL(item(index), GetReferrerUrl())
        .WillRepeatedly(ReturnRefOfCopy(referrer));
    EXPECT_CALL(item(index), GetSiteUrl())
        .WillRepeatedly(ReturnRefOfCopy(site_url));
    EXPECT_CALL(item(index), GetTabUrl())
        .WillRepeatedly(ReturnRefOfCopy(tab_url));
    EXPECT_CALL(item(index), GetTabReferrerUrl())
        .WillRepeatedly(ReturnRefOfCopy(tab_referrer_url));
    EXPECT_CALL(item(index), GetStartTime()).WillRepeatedly(Return(start_time));
    EXPECT_CALL(item(index), GetEndTime()).WillRepeatedly(Return(end_time));
    EXPECT_CALL(item(index), GetETag()).WillRepeatedly(ReturnRefOfCopy(etag));
    EXPECT_CALL(item(index), GetLastModifiedTime())
        .WillRepeatedly(ReturnRefOfCopy(last_modified));
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(item(index), GetTotalBytes())
        .WillRepeatedly(Return(total_bytes));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(index), GetDangerType())
        .WillRepeatedly(Return(danger_type));
    EXPECT_CALL(item(index), GetLastReason())
        .WillRepeatedly(Return(interrupt_reason));
    EXPECT_CALL(item(index), GetOpened()).WillRepeatedly(Return(opened));
    EXPECT_CALL(item(index), GetTargetDisposition())
        .WillRepeatedly(
            Return(content::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    EXPECT_CALL(manager(), GetDownload(id))
        .WillRepeatedly(Return(&item(index)));
    EXPECT_CALL(item(index), IsTemporary()).WillRepeatedly(Return(false));
#if defined(ENABLE_EXTENSIONS)
    new extensions::DownloadedByExtension(
        &item(index), by_extension_id, by_extension_name);
#endif

    std::vector<content::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
  }

 private:
  void CheckDownloadWasRestoredFromHistory(bool expected_value,
                                           content::MockDownloadItem* item) {
    ASSERT_TRUE(download_history_.get());
    EXPECT_EQ(expected_value, download_history_->WasRestoredFromHistory(item));
  }

  base::MessageLoopForUI loop_;
  content::TestBrowserThread ui_thread_;
  std::vector<StrictMockDownloadItem*> items_;
  std::unique_ptr<content::MockDownloadManager> manager_;
  FakeHistoryAdapter* history_;
  std::unique_ptr<DownloadHistory> download_history_;
  content::DownloadManager::Observer* manager_observer_;
  size_t download_created_index_;
  DownloadItemCallback pre_on_create_handler_;
  DownloadItemCallback post_on_create_handler_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHistoryTest);
};

// Test loading an item from the database, changing it, saving it back, removing
// it.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Load) {
  // Load a download from history, create the item, OnDownloadCreated,
  // OnDownloadUpdated, OnDownloadRemoved.
  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);
  {
    std::unique_ptr<InfoVector> infos(new InfoVector());
    infos->push_back(info);
    CreateDownloadHistory(std::move(infos));
    ExpectNoDownloadCreated();
  }
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  info.opened = true;
  ExpectDownloadUpdated(info);

  // Pretend that the user removed the item.
  IdSet ids;
  ids.insert(info.id);
  item(0).NotifyObserversDownloadRemoved();
  ExpectDownloadsRemoved(ids);
}

// Test that the OnHistoryQueryComplete() observer method is invoked for an
// observer that was added before the initial history query completing.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_OnHistoryQueryComplete_Pre) {
  class TestHistoryAdapter : public DownloadHistory::HistoryAdapter {
   public:
    TestHistoryAdapter(
        history::HistoryService::DownloadQueryCallback* callback_storage)
        : HistoryAdapter(nullptr),
          query_callback_(callback_storage) {}
    void QueryDownloads(const history::HistoryService::DownloadQueryCallback&
                            callback) override {
      *query_callback_ = callback;
    }

    history::HistoryService::DownloadQueryCallback* query_callback_;
  };

  TestDownloadHistoryObserver observer;
  history::HistoryService::DownloadQueryCallback query_callback;
  std::unique_ptr<TestHistoryAdapter> test_history_adapter(
      new TestHistoryAdapter(&query_callback));

  // Create a new DownloadHistory object. This should cause
  // TestHistoryAdapter::QueryDownloads() to be called. The TestHistoryAdapter
  // stored the completion callback.
  std::unique_ptr<DownloadHistory> history(
      new DownloadHistory(&manager(), std::move(test_history_adapter)));
  history->AddObserver(&observer);
  EXPECT_FALSE(observer.on_history_query_complete_called_);
  ASSERT_FALSE(query_callback.is_null());

  // Now invoke the query completion callback.
  std::unique_ptr<std::vector<history::DownloadRow>> query_results(
      new std::vector<history::DownloadRow>());
  query_callback.Run(std::move(query_results));
  EXPECT_TRUE(observer.on_history_query_complete_called_);
  history->RemoveObserver(&observer);
}

// Test that the OnHistoryQueryComplete() observer method is invoked for an
// observer that was added after the initial history query completing.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_OnHistoryQueryComplete_Post) {
  TestDownloadHistoryObserver observer;
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));
  download_history()->AddObserver(&observer);
  EXPECT_TRUE(observer.on_history_query_complete_called_);
  download_history()->RemoveObserver(&observer);
}

// Test that WasRestoredFromHistory accurately identifies downloads that were
// created from history, even during an OnDownloadCreated() handler.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_WasRestoredFromHistory_True) {
  // This sets DownloadHistoryTest to call DH::WasRestoredFromHistory() both
  // before and after DH::OnDownloadCreated() is called. At each call, the
  // expected return value is |true| since the download was restored from
  // history.
  ExpectDownloadsRestoredFromHistory(true);

  // Construct a DownloadHistory with a single history download. This results in
  // DownloadManager::CreateDownload() being called for the restored download.
  // The above test expectation should verify that the value of
  // WasRestoredFromHistory is correct for this download.
  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);
  std::unique_ptr<InfoVector> infos(new InfoVector());
  infos->push_back(info);
  CreateDownloadHistory(std::move(infos));

  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
}

// Test that WasRestoredFromHistory accurately identifies downloads that were
// not created from history.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_WasRestoredFromHistory_False) {
  // This sets DownloadHistoryTest to call DH::WasRestoredFromHistory() both
  // before and after DH::OnDownloadCreated() is called. At each call, the
  // expected return value is |true| since the download was restored from
  // history.
  ExpectDownloadsRestoredFromHistory(false);

  // Create a DownloadHistory with no history downloads. No
  // DownloadManager::CreateDownload() calls are expected.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  // Notify DownloadHistory that a new download was created. The above test
  // expecation should verify that WasRestoredFromHistory is correct for this
  // download.
  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
}

// Test creating an item, saving it to the database, changing it, saving it
// back, removing it.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Create) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  info.opened = true;
  ExpectDownloadUpdated(info);

  // Pretend that the user removed the item.
  IdSet ids;
  ids.insert(info.id);
  item(0).NotifyObserversDownloadRemoved();
  ExpectDownloadsRemoved(ids);
}

// Test that changes to persisted fields in a DownloadItem triggers database
// updates.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Update) {
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  base::FilePath new_path(FILE_PATH_LITERAL("/foo/baz.txt"));
  base::Time new_time(base::Time::Now());
  std::string new_etag("new etag");
  std::string new_last_modifed("new last modified");

  // current_path
  EXPECT_CALL(item(0), GetFullPath()).WillRepeatedly(ReturnRefOfCopy(new_path));
  info.current_path = new_path;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // target_path
  EXPECT_CALL(item(0), GetTargetFilePath())
      .WillRepeatedly(ReturnRefOfCopy(new_path));
  info.target_path = new_path;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // end_time
  EXPECT_CALL(item(0), GetEndTime()).WillRepeatedly(Return(new_time));
  info.end_time = new_time;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // received_bytes
  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(101));
  info.received_bytes = 101;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // total_bytes
  EXPECT_CALL(item(0), GetTotalBytes()).WillRepeatedly(Return(102));
  info.total_bytes = 102;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // etag
  EXPECT_CALL(item(0), GetETag()).WillRepeatedly(ReturnRefOfCopy(new_etag));
  info.etag = new_etag;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // last_modified
  EXPECT_CALL(item(0), GetLastModifiedTime())
      .WillRepeatedly(ReturnRefOfCopy(new_last_modifed));
  info.last_modified = new_last_modifed;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // state
  EXPECT_CALL(item(0), GetState())
      .WillRepeatedly(Return(content::DownloadItem::INTERRUPTED));
  info.state = history::DownloadState::INTERRUPTED;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // danger_type
  EXPECT_CALL(item(0), GetDangerType())
      .WillRepeatedly(Return(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT));
  info.danger_type = history::DownloadDangerType::DANGEROUS_CONTENT;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // interrupt_reason
  EXPECT_CALL(item(0), GetLastReason())
      .WillRepeatedly(Return(content::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED));
  info.interrupt_reason = history::ToHistoryDownloadInterruptReason(
      content::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED);
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);

  // opened
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  info.opened = true;
  item(0).NotifyObserversDownloadUpdated();
  ExpectDownloadUpdated(info);
}

// Test creating a new item, saving it, removing it by setting it Temporary,
// changing it without saving it back because it's Temporary, clearing
// IsTemporary, saving it back, changing it, saving it back because it isn't
// Temporary anymore.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Temporary) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend the item was marked temporary. DownloadHistory should remove it
  // from history and start ignoring it.
  EXPECT_CALL(item(0), IsTemporary()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();
  IdSet ids;
  ids.insert(info.id);
  ExpectDownloadsRemoved(ids);

  // Change something that would make DownloadHistory call UpdateDownload if the
  // item weren't temporary.
  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(4200));
  item(0).NotifyObserversDownloadUpdated();
  ExpectNoDownloadUpdated();

  // Changing a temporary item back to a non-temporary item should make
  // DownloadHistory call CreateDownload.
  EXPECT_CALL(item(0), IsTemporary()).WillRepeatedly(Return(false));
  item(0).NotifyObserversDownloadUpdated();
  info.received_bytes = 4200;
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(100));
  item(0).NotifyObserversDownloadUpdated();
  info.received_bytes = 100;
  ExpectDownloadUpdated(info);
}

// Test removing downloads while they're still being added.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_RemoveWhileAdding) {
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);

  // Instruct CreateDownload() to not callback to DownloadHistory immediately,
  // but to wait for FinishCreateDownload().
  set_slow_create_download(true);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Call OnDownloadRemoved before calling back to DownloadHistory::ItemAdded().
  // Instead of calling RemoveDownloads() immediately, DownloadHistory should
  // add the item's id to removed_while_adding_. Then, ItemAdded should
  // immediately remove the item's record from history.
  item(0).NotifyObserversDownloadRemoved();
  EXPECT_CALL(manager(), GetDownload(item(0).GetId()))
    .WillRepeatedly(Return(static_cast<content::DownloadItem*>(NULL)));
  ExpectNoDownloadsRemoved();
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Now callback to DownloadHistory::ItemAdded(), and expect a call to
  // RemoveDownloads() for the item that was removed while it was being added.
  FinishCreateDownload();
  IdSet ids;
  ids.insert(info.id);
  ExpectDownloadsRemoved(ids);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));
}

// Test loading multiple items from the database and removing them all.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Multiple) {
  // Load a download from history, create the item, OnDownloadCreated,
  // OnDownloadUpdated, OnDownloadRemoved.
  history::DownloadRow info0, info1;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info0);
  InitBasicItem(FILE_PATH_LITERAL("/foo/qux.pdf"),
                "http://example.com/qux.pdf",
                "http://example.com/referrer1.html",
                &info1);
  {
    std::unique_ptr<InfoVector> infos(new InfoVector());
    infos->push_back(info0);
    infos->push_back(info1);
    CreateDownloadHistory(std::move(infos));
    ExpectNoDownloadCreated();
  }

  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(1)));

  // Pretend that the user removed both items.
  IdSet ids;
  ids.insert(info0.id);
  ids.insert(info1.id);
  item(0).NotifyObserversDownloadRemoved();
  item(1).NotifyObserversDownloadRemoved();
  ExpectDownloadsRemoved(ids);
}

// Test what happens when HistoryService/CreateDownload::CreateDownload() fails.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_CreateFailed) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);

  FailCreateDownload();
  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(100));
  item(0).NotifyObserversDownloadUpdated();
  info.received_bytes = 100;
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
}

TEST_F(DownloadHistoryTest, DownloadHistoryTest_UpdateWhileAdding) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  CreateDownloadHistory(std::unique_ptr<InfoVector>(new InfoVector()));

  history::DownloadRow info;
  InitBasicItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                "http://example.com/bar.pdf",
                "http://example.com/referrer.html",
                &info);

  // Instruct CreateDownload() to not callback to DownloadHistory immediately,
  // but to wait for FinishCreateDownload().
  set_slow_create_download(true);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  ExpectDownloadCreated(info);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item(0).NotifyObserversDownloadUpdated();

  FinishCreateDownload();
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // ItemAdded should call OnDownloadUpdated, which should detect that the item
  // changed while it was being added and call UpdateDownload immediately.
  info.opened = true;
  ExpectDownloadUpdated(info);
}

}  // anonymous namespace
