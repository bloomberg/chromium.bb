// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/history/download_database.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_service.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgPointee;
using testing::WithArg;
using testing::_;

namespace {

void CheckInfoEqual(const history::DownloadRow& left,
                    const history::DownloadRow& right) {
  EXPECT_EQ(left.current_path.value(), right.current_path.value());
  EXPECT_EQ(left.target_path.value(), right.target_path.value());
  EXPECT_EQ(left.url_chain.size(), right.url_chain.size());
  for (unsigned int i = 0;
       i < left.url_chain.size() && i < right.url_chain.size();
       ++i) {
    EXPECT_EQ(left.url_chain[i].spec(), right.url_chain[i].spec());
  }
  EXPECT_EQ(left.referrer_url.spec(), right.referrer_url.spec());
  EXPECT_EQ(left.start_time.ToTimeT(), right.start_time.ToTimeT());
  EXPECT_EQ(left.end_time.ToTimeT(), right.end_time.ToTimeT());
  EXPECT_EQ(left.received_bytes, right.received_bytes);
  EXPECT_EQ(left.total_bytes, right.total_bytes);
  EXPECT_EQ(left.state, right.state);
  EXPECT_EQ(left.danger_type, right.danger_type);
  EXPECT_EQ(left.db_handle, right.db_handle);
  EXPECT_EQ(left.opened, right.opened);
}

typedef std::set<int64> HandleSet;
typedef std::vector<history::DownloadRow> InfoVector;
typedef testing::NiceMock<content::MockDownloadItem> NiceMockDownloadItem;

class FakeHistoryAdapter : public DownloadHistory::HistoryAdapter {
 public:
  FakeHistoryAdapter()
    : DownloadHistory::HistoryAdapter(NULL),
      slow_create_download_(false),
      fail_create_download_(false),
      handle_counter_(0) {
  }

  virtual ~FakeHistoryAdapter() {}

  virtual void QueryDownloads(
      const HistoryService::DownloadQueryCallback& callback) OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&FakeHistoryAdapter::QueryDownloadsDone,
            base::Unretained(this), callback));
  }

  void QueryDownloadsDone(
      const HistoryService::DownloadQueryCallback& callback) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    CHECK(expect_query_downloads_.get());
    callback.Run(expect_query_downloads_.Pass());
  }

  void set_slow_create_download(bool slow) { slow_create_download_ = slow; }

  virtual void CreateDownload(
      const history::DownloadRow& info,
      const HistoryService::DownloadCreateCallback& callback) OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    create_download_info_ = info;
    // Must not call CreateDownload() again before FinishCreateDownload()!
    DCHECK(create_download_callback_.is_null());
    create_download_callback_ = base::Bind(
        callback,
        (fail_create_download_ ?
          history::DownloadDatabase::kUninitializedHandle :
          handle_counter_++));
    fail_create_download_ = false;
    if (!slow_create_download_)
      FinishCreateDownload();
  }

  void FinishCreateDownload() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    create_download_callback_.Run();
    create_download_callback_.Reset();
  }

  virtual void UpdateDownload(
      const history::DownloadRow& info) OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    update_download_ = info;
  }

  virtual void RemoveDownloads(const HandleSet& handles) OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    for (HandleSet::const_iterator it = handles.begin();
         it != handles.end(); ++it) {
      remove_downloads_.insert(*it);
    }
  }

  void ExpectWillQueryDownloads(scoped_ptr<InfoVector> infos) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    expect_query_downloads_ = infos.Pass();
  }

  void ExpectQueryDownloadsDone() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    EXPECT_TRUE(NULL == expect_query_downloads_.get());
  }

  void FailCreateDownload() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    fail_create_download_ = true;
  }

  void ExpectDownloadCreated(
      const history::DownloadRow& info) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    CheckInfoEqual(info, create_download_info_);
    create_download_info_ = history::DownloadRow();
  }

  void ExpectNoDownloadCreated() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    CheckInfoEqual(history::DownloadRow(), create_download_info_);
  }

  void ExpectDownloadUpdated(const history::DownloadRow& info) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    CheckInfoEqual(update_download_, info);
    update_download_ = history::DownloadRow();
  }

  void ExpectNoDownloadUpdated() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    CheckInfoEqual(history::DownloadRow(), update_download_);
  }

  void ExpectNoDownloadsRemoved() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    EXPECT_EQ(0, static_cast<int>(remove_downloads_.size()));
  }

  void ExpectDownloadsRemoved(const HandleSet& handles) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    HandleSet differences;
    std::insert_iterator<HandleSet> differences_iter(
        differences, differences.begin());
    std::set_difference(handles.begin(), handles.end(),
                        remove_downloads_.begin(),
                        remove_downloads_.end(),
                        differences_iter);
    for (HandleSet::const_iterator different = differences.begin();
         different != differences.end(); ++different) {
      EXPECT_TRUE(false) << *different;
    }
    remove_downloads_.clear();
  }

 private:
  bool slow_create_download_;
  bool fail_create_download_;
  base::Closure create_download_callback_;
  int handle_counter_;
  history::DownloadRow update_download_;
  scoped_ptr<InfoVector> expect_query_downloads_;
  HandleSet remove_downloads_;
  history::DownloadRow create_download_info_;

  DISALLOW_COPY_AND_ASSIGN(FakeHistoryAdapter);
};

class DownloadHistoryTest : public testing::Test {
 public:
  DownloadHistoryTest()
    : ui_thread_(content::BrowserThread::UI, &loop_),
      manager_(new content::MockDownloadManager()),
      history_(NULL),
      download_history_(NULL),
      manager_observer_(NULL),
      item_observer_(NULL),
      download_created_index_(0) {
  }
  virtual ~DownloadHistoryTest() {
    STLDeleteElements(&items_);
  }

 protected:
  virtual void TearDown() OVERRIDE {
    download_history_.reset();
  }

  content::MockDownloadManager& manager() { return *manager_.get(); }
  content::MockDownloadItem& item(size_t index) { return *items_[index]; }

  void SetManagerObserver(
      content::DownloadManager::Observer* manager_observer) {
    manager_observer_ = manager_observer;
  }
  content::DownloadManager::Observer* manager_observer() {
    return manager_observer_;
  }

  // Relies on the same object observing all download items.
  void SetItemObserver(
      content::DownloadItem::Observer* item_observer) {
    item_observer_ = item_observer;
  }
  content::DownloadItem::Observer* item_observer() {
    return item_observer_;
  }

  void ExpectWillQueryDownloads(scoped_ptr<InfoVector> infos) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    CHECK(infos.get());
    EXPECT_CALL(manager(), AddObserver(_)).WillOnce(WithArg<0>(Invoke(
        this, &DownloadHistoryTest::SetManagerObserver)));
    EXPECT_CALL(manager(), RemoveObserver(_));
    download_created_index_ = 0;
    for (size_t index = 0; index < infos->size(); ++index) {
      content::MockDownloadManager::CreateDownloadItemAdapter adapter(
          infos->at(index).current_path,
          infos->at(index).target_path,
          infos->at(index).url_chain,
          infos->at(index).referrer_url,
          infos->at(index).start_time,
          infos->at(index).end_time,
          infos->at(index).received_bytes,
          infos->at(index).total_bytes,
          infos->at(index).state,
          infos->at(index).danger_type,
          infos->at(index).interrupt_reason,
          infos->at(index).opened);
      EXPECT_CALL(manager(), MockCreateDownloadItem(adapter))
        .WillOnce(DoAll(
            InvokeWithoutArgs(
                this, &DownloadHistoryTest::CallOnDownloadCreatedInOrder),
            Return(&item(index))));
    }
    EXPECT_CALL(manager(), CheckForHistoryFilesRemoval());
    history_ = new FakeHistoryAdapter();
    history_->ExpectWillQueryDownloads(infos.Pass());
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_)).WillRepeatedly(Return());
    download_history_.reset(new DownloadHistory(
        &manager(), scoped_ptr<DownloadHistory::HistoryAdapter>(history_)));
    content::RunAllPendingInMessageLoop(content::BrowserThread::UI);
    history_->ExpectQueryDownloadsDone();
  }

  void CallOnDownloadCreated(size_t index) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    manager_observer()->OnDownloadCreated(&manager(), &item(index));
  }

  void CallOnDownloadCreatedInOrder() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    // Gmock doesn't appear to support something like InvokeWithTheseArgs. Maybe
    // gmock needs to learn about base::Callback.
    CallOnDownloadCreated(download_created_index_++);
  }

  void set_slow_create_download(bool slow) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->set_slow_create_download(slow);
  }

  void FinishCreateDownload() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->FinishCreateDownload();
  }

  void FailCreateDownload() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->FailCreateDownload();
  }

  void ExpectDownloadCreated(
      const history::DownloadRow& info) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->ExpectDownloadCreated(info);
  }

  void ExpectNoDownloadCreated() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->ExpectNoDownloadCreated();
  }

  void ExpectDownloadUpdated(const history::DownloadRow& info) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->ExpectDownloadUpdated(info);
  }

  void ExpectNoDownloadUpdated() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->ExpectNoDownloadUpdated();
  }

  void ExpectNoDownloadsRemoved() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->ExpectNoDownloadsRemoved();
  }

  void ExpectDownloadsRemoved(const HandleSet& handles) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    history_->ExpectDownloadsRemoved(handles);
  }

  // Caller is responsibile for making sure reference arguments lifetime is
  // greater than the lifetime of the NiceMockDownloadItem() created by this
  // routine.
  void InitItem(
      int32 id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer,
      const base::Time& start_time,
      const base::Time& end_time,
      int64 received_bytes,
      int64 total_bytes,
      content::DownloadItem::DownloadState state,
      content::DownloadDangerType danger_type,
      content::DownloadInterruptReason interrupt_reason,
      int64 db_handle,
      bool opened,
      history::DownloadRow* info) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    int32 index = items_.size();
    NiceMockDownloadItem* mock_item = new NiceMockDownloadItem();
    items_.push_back(mock_item);

    info->current_path = current_path;
    info->target_path = target_path;
    info->url_chain = url_chain;
    info->referrer_url = referrer;
    info->start_time = start_time;
    info->end_time = end_time;
    info->received_bytes = received_bytes;
    info->total_bytes = total_bytes;
    info->state = state;
    info->danger_type = danger_type;
    info->interrupt_reason = interrupt_reason;
    info->db_handle = db_handle;
    info->opened = opened;

    EXPECT_CALL(item(index), GetId()).WillRepeatedly(Return(id));
    EXPECT_CALL(item(index),
                GetFullPath()).WillRepeatedly(ReturnRef(current_path));
    EXPECT_CALL(item(index),
                GetTargetFilePath()).WillRepeatedly(ReturnRef(target_path));
    DCHECK_LE(1u, url_chain.size());
    EXPECT_CALL(item(index), GetURL()).WillRepeatedly(ReturnRef(url_chain[0]));
    EXPECT_CALL(item(index),
                GetUrlChain()).WillRepeatedly(ReturnRef(url_chain));
    EXPECT_CALL(item(index), GetMimeType()).WillRepeatedly(Return(
        "application/octet-stream"));
    EXPECT_CALL(item(index), GetReferrerUrl()).WillRepeatedly(ReturnRef(
        referrer));
    EXPECT_CALL(item(index), GetStartTime()).WillRepeatedly(Return(start_time));
    EXPECT_CALL(item(index), GetEndTime()).WillRepeatedly(Return(end_time));
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(item(index), GetTotalBytes()).WillRepeatedly(Return(
        total_bytes));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    EXPECT_CALL(item(index), GetDangerType())
        .WillRepeatedly(Return(danger_type));
    EXPECT_CALL(item(index), GetLastReason()).WillRepeatedly(
        Return(interrupt_reason));
    EXPECT_CALL(item(index), GetOpened()).WillRepeatedly(Return(opened));
    EXPECT_CALL(item(index), GetTargetDisposition()).WillRepeatedly(Return(
          content::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    EXPECT_CALL(manager(), GetDownload(id))
        .WillRepeatedly(Return(&item(index)));
    EXPECT_CALL(item(index), AddObserver(_)).WillOnce(WithArg<0>(Invoke(
        this, &DownloadHistoryTest::SetItemObserver)));
    EXPECT_CALL(item(index), RemoveObserver(_));

    std::vector<content::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
      .WillRepeatedly(SetArgPointee<0>(items));
  }

 private:
  MessageLoopForUI loop_;
  content::TestBrowserThread ui_thread_;
  std::vector<NiceMockDownloadItem*> items_;
  scoped_refptr<content::MockDownloadManager> manager_;
  FakeHistoryAdapter* history_;
  scoped_ptr<DownloadHistory> download_history_;
  content::DownloadManager::Observer* manager_observer_;
  content::DownloadItem::Observer* item_observer_;
  size_t download_created_index_;

  DISALLOW_COPY_AND_ASSIGN(DownloadHistoryTest);
};

}  // anonymous namespace

// Test loading an item from the database, changing it, saving it back, removing
// it.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Load) {
  // Load a download from history, create the item, OnDownloadCreated,
  // OnDownloadUpdated, OnDownloadRemoved.
  history::DownloadRow info;
  base::FilePath path(FILE_PATH_LITERAL("/foo/bar.pdf"));
  GURL url("http://example.com/bar.pdf");
  GURL referrer("http://example.com/referrer.html");
  std::vector<GURL> url_chain;
  url_chain.push_back(url);
  InitItem(base::RandInt(0, 1 << 20),
           path,
           path,
           url_chain,
           referrer,
           (base::Time::Now() - base::TimeDelta::FromMinutes(10)),
           (base::Time::Now() - base::TimeDelta::FromMinutes(1)),
           100,
           100,
           content::DownloadItem::COMPLETE,
           content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
           content::DOWNLOAD_INTERRUPT_REASON_NONE,
           base::RandInt(0, 1 << 20),
           false,
           &info);
  {
    scoped_ptr<InfoVector> infos(new InfoVector());
    infos->push_back(info);
    ExpectWillQueryDownloads(infos.Pass());
    ExpectNoDownloadCreated();
  }
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item_observer()->OnDownloadUpdated(&item(0));
  info.opened = true;
  ExpectDownloadUpdated(info);

  // Pretend that the user removed the item.
  HandleSet handles;
  handles.insert(info.db_handle);
  item_observer()->OnDownloadRemoved(&item(0));
  ExpectDownloadsRemoved(handles);
}

// Test creating an item, saving it to the database, changing it, saving it
// back, removing it.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Create) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  ExpectWillQueryDownloads(scoped_ptr<InfoVector>(new InfoVector()));

  // Note that db_handle must be -1 at first because it isn't in the db yet.
  history::DownloadRow info;
  base::FilePath path(FILE_PATH_LITERAL("/foo/bar.pdf"));
  GURL url("http://example.com/bar.pdf");
  GURL referrer("http://example.com/referrer.html");
  std::vector<GURL> url_chain;
  url_chain.push_back(url);
  InitItem(base::RandInt(0, 1 << 20),
           path,
           path,
           url_chain,
           referrer,
           (base::Time::Now() - base::TimeDelta::FromMinutes(10)),
           (base::Time::Now() - base::TimeDelta::FromMinutes(1)),
           100,
           100,
           content::DownloadItem::COMPLETE,
           content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
           content::DOWNLOAD_INTERRUPT_REASON_NONE,
           -1,
           false,
           &info);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  // CreateDownload() always gets db_handle=-1.
  ExpectDownloadCreated(info);
  info.db_handle = 0;
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item_observer()->OnDownloadUpdated(&item(0));
  info.opened = true;
  ExpectDownloadUpdated(info);

  // Pretend that the user removed the item.
  HandleSet handles;
  handles.insert(info.db_handle);
  item_observer()->OnDownloadRemoved(&item(0));
  ExpectDownloadsRemoved(handles);
}

// Test creating a new item, saving it, removing it by setting it Temporary,
// changing it without saving it back because it's Temporary, clearing
// IsTemporary, saving it back, changing it, saving it back because it isn't
// Temporary anymore.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Temporary) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  ExpectWillQueryDownloads(scoped_ptr<InfoVector>(new InfoVector()));

  // Note that db_handle must be -1 at first because it isn't in the db yet.
  history::DownloadRow info;
  base::FilePath path(FILE_PATH_LITERAL("/foo/bar.pdf"));
  GURL url("http://example.com/bar.pdf");
  GURL referrer("http://example.com/referrer.html");
  std::vector<GURL> url_chain;
  url_chain.push_back(url);
  InitItem(base::RandInt(0, 1 << 20),
           path,
           path,
           url_chain,
           referrer,
           (base::Time::Now() - base::TimeDelta::FromMinutes(10)),
           (base::Time::Now() - base::TimeDelta::FromMinutes(1)),
           100,
           100,
           content::DownloadItem::COMPLETE,
           content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
           content::DOWNLOAD_INTERRUPT_REASON_NONE,
           -1,
           false,
           &info);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  // CreateDownload() always gets db_handle=-1.
  ExpectDownloadCreated(info);
  info.db_handle = 0;
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend the item was marked temporary. DownloadHistory should remove it
  // from history and start ignoring it.
  EXPECT_CALL(item(0), IsTemporary()).WillRepeatedly(Return(true));
  item_observer()->OnDownloadUpdated(&item(0));
  HandleSet handles;
  handles.insert(info.db_handle);
  ExpectDownloadsRemoved(handles);

  // Change something that would make DownloadHistory call UpdateDownload if the
  // item weren't temporary.
  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(4200));
  item_observer()->OnDownloadUpdated(&item(0));
  ExpectNoDownloadUpdated();

  // Changing a temporary item back to a non-temporary item should make
  // DownloadHistory call CreateDownload.
  EXPECT_CALL(item(0), IsTemporary()).WillRepeatedly(Return(false));
  item_observer()->OnDownloadUpdated(&item(0));
  info.received_bytes = 4200;
  info.db_handle = -1;
  // CreateDownload() always gets db_handle=-1.
  ExpectDownloadCreated(info);
  info.db_handle = 1;
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(100));
  item_observer()->OnDownloadUpdated(&item(0));
  info.received_bytes = 100;
  ExpectDownloadUpdated(info);
}

// Test removing downloads while they're still being added.
TEST_F(DownloadHistoryTest,
    DownloadHistoryTest_RemoveWhileAdding) {
  ExpectWillQueryDownloads(scoped_ptr<InfoVector>(new InfoVector()));

  // Note that db_handle must be -1 at first because it isn't in the db yet.
  history::DownloadRow info;
  base::FilePath path(FILE_PATH_LITERAL("/foo/bar.pdf"));
  GURL url("http://example.com/bar.pdf");
  GURL referrer("http://example.com/referrer.html");
  std::vector<GURL> url_chain;
  url_chain.push_back(url);
  InitItem(base::RandInt(0, 1 << 20),
           path,
           path,
           url_chain,
           referrer,
           (base::Time::Now() - base::TimeDelta::FromMinutes(10)),
           (base::Time::Now() - base::TimeDelta::FromMinutes(1)),
           100,
           100,
           content::DownloadItem::COMPLETE,
           content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
           content::DOWNLOAD_INTERRUPT_REASON_NONE,
           -1,
           false,
           &info);

  // Instruct CreateDownload() to not callback to DownloadHistory immediately,
  // but to wait for FinishCreateDownload().
  set_slow_create_download(true);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  // CreateDownload() always gets db_handle=-1.
  ExpectDownloadCreated(info);
  info.db_handle = 0;
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Call OnDownloadRemoved before calling back to DownloadHistory::ItemAdded().
  // Instead of calling RemoveDownloads() immediately, DownloadHistory should
  // add the item's id to removed_while_adding_. Then, ItemAdded should
  // immediately remove the item's record from history.
  item_observer()->OnDownloadRemoved(&item(0));
  EXPECT_CALL(manager(), GetDownload(item(0).GetId()))
    .WillRepeatedly(Return(static_cast<content::DownloadItem*>(NULL)));
  ExpectNoDownloadsRemoved();
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Now callback to DownloadHistory::ItemAdded(), and expect a call to
  // RemoveDownloads() for the item that was removed while it was being added.
  FinishCreateDownload();
  HandleSet handles;
  handles.insert(info.db_handle);
  ExpectDownloadsRemoved(handles);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));
}

// Test loading multiple items from the database and removing them all.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_Multiple) {
  // Load a download from history, create the item, OnDownloadCreated,
  // OnDownloadUpdated, OnDownloadRemoved.
  history::DownloadRow info0, info1;
  base::FilePath path0(FILE_PATH_LITERAL("/foo/bar.pdf"));
  GURL url0("http://example.com/bar.pdf");
  GURL referrer0("http://example.com/referrer.html");
  std::vector<GURL> url0_chain;
  url0_chain.push_back(url0);
  InitItem(base::RandInt(0, 1 << 10),
           path0,
           path0,
           url0_chain,
           referrer0,
           (base::Time::Now() - base::TimeDelta::FromMinutes(11)),
           (base::Time::Now() - base::TimeDelta::FromMinutes(2)),
           100,
           100,
           content::DownloadItem::COMPLETE,
           content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
           content::DOWNLOAD_INTERRUPT_REASON_NONE,
           base::RandInt(0, 1 << 10),
           false,
           &info0);
  base::FilePath path1(FILE_PATH_LITERAL("/foo/qux.pdf"));
  GURL url1("http://example.com/qux.pdf");
  GURL referrer1("http://example.com/referrer.html");
  std::vector<GURL> url1_chain;
  url1_chain.push_back(url0);
  InitItem(item(0).GetId() + base::RandInt(1, 1 << 10),
           path1,
           path1,
           url1_chain,
           referrer1,
           (base::Time::Now() - base::TimeDelta::FromMinutes(10)),
           (base::Time::Now() - base::TimeDelta::FromMinutes(1)),
           200,
           200,
           content::DownloadItem::COMPLETE,
           content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
           content::DOWNLOAD_INTERRUPT_REASON_NONE,
           info0.db_handle + base::RandInt(1, 1 << 10),
           false,
           &info1);
  {
    scoped_ptr<InfoVector> infos(new InfoVector());
    infos->push_back(info0);
    infos->push_back(info1);
    ExpectWillQueryDownloads(infos.Pass());
    ExpectNoDownloadCreated();
  }

  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(1)));

  // Pretend that the user removed both items.
  HandleSet handles;
  handles.insert(info0.db_handle);
  handles.insert(info1.db_handle);
  item_observer()->OnDownloadRemoved(&item(0));
  item_observer()->OnDownloadRemoved(&item(1));
  ExpectDownloadsRemoved(handles);
}

// Test what happens when HistoryService/CreateDownload::CreateDownload() fails.
TEST_F(DownloadHistoryTest, DownloadHistoryTest_CreateFailed) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  ExpectWillQueryDownloads(scoped_ptr<InfoVector>(new InfoVector()));

  // Note that db_handle must be -1 at first because it isn't in the db yet.
  history::DownloadRow info;
  base::FilePath path(FILE_PATH_LITERAL("/foo/bar.pdf"));
  GURL url("http://example.com/bar.pdf");
  GURL referrer("http://example.com/referrer.html");
  std::vector<GURL> url_chain;
  url_chain.push_back(url);
  InitItem(base::RandInt(0, 1 << 20),
           path,
           path,
           url_chain,
           referrer,
           (base::Time::Now() - base::TimeDelta::FromMinutes(10)),
           (base::Time::Now() - base::TimeDelta::FromMinutes(1)),
           100,
           100,
           content::DownloadItem::COMPLETE,
           content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
           content::DOWNLOAD_INTERRUPT_REASON_NONE,
           -1,
           false,
           &info);

  FailCreateDownload();
  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  // CreateDownload() always gets db_handle=-1.
  ExpectDownloadCreated(info);
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  EXPECT_CALL(item(0), GetReceivedBytes()).WillRepeatedly(Return(100));
  item_observer()->OnDownloadUpdated(&item(0));
  info.received_bytes = 100;
  ExpectDownloadCreated(info);
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));
}

TEST_F(DownloadHistoryTest,
    DownloadHistoryTest_UpdateWhileAdding) {
  // Create a fresh item not from history, OnDownloadCreated, OnDownloadUpdated,
  // OnDownloadRemoved.
  ExpectWillQueryDownloads(scoped_ptr<InfoVector>(new InfoVector()));

  // Note that db_handle must be -1 at first because it isn't in the db yet.
  history::DownloadRow info;
  base::FilePath path(FILE_PATH_LITERAL("/foo/bar.pdf"));
  GURL url("http://example.com/bar.pdf");
  GURL referrer("http://example.com/referrer.html");
  std::vector<GURL> url_chain;
  url_chain.push_back(url);
  InitItem(base::RandInt(0, 1 << 20),
           path,
           path,
           url_chain,
           referrer,
           (base::Time::Now() - base::TimeDelta::FromMinutes(10)),
           (base::Time::Now() - base::TimeDelta::FromMinutes(1)),
           100,
           100,
           content::DownloadItem::COMPLETE,
           content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
           content::DOWNLOAD_INTERRUPT_REASON_NONE,
           -1,
           false,
           &info);

  // Instruct CreateDownload() to not callback to DownloadHistory immediately,
  // but to wait for FinishCreateDownload().
  set_slow_create_download(true);

  // Pretend the manager just created |item|.
  CallOnDownloadCreated(0);
  // CreateDownload() always gets db_handle=-1.
  ExpectDownloadCreated(info);
  info.db_handle = 0;
  EXPECT_FALSE(DownloadHistory::IsPersisted(&item(0)));

  // Pretend that something changed on the item.
  EXPECT_CALL(item(0), GetOpened()).WillRepeatedly(Return(true));
  item_observer()->OnDownloadUpdated(&item(0));

  FinishCreateDownload();
  EXPECT_TRUE(DownloadHistory::IsPersisted(&item(0)));

  // ItemAdded should call OnDownloadUpdated, which should detect that the item
  // changed while it was being added and call UpdateDownload immediately.
  info.opened = true;
  ExpectDownloadUpdated(info);
}
