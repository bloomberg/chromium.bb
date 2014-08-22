// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_ui_controller.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::MockDownloadItem;
using content::MockDownloadManager;
using testing::AnyNumber;
using testing::Assign;
using testing::Return;
using testing::ReturnRefOfCopy;
using testing::SaveArg;
using testing::_;

namespace {

// A DownloadUIController::Delegate that stores the DownloadItem* for the last
// download that was sent to the UI.
class TestDelegate : public DownloadUIController::Delegate {
 public:
  explicit TestDelegate(base::WeakPtr<content::DownloadItem*> receiver);
  virtual ~TestDelegate() {}

 private:
  virtual void OnNewDownloadReady(content::DownloadItem* item) OVERRIDE;

  base::WeakPtr<content::DownloadItem*> receiver_;
};

TestDelegate::TestDelegate(base::WeakPtr<content::DownloadItem*> receiver)
    : receiver_(receiver) {
}

void TestDelegate::OnNewDownloadReady(content::DownloadItem* item) {
  if (receiver_.get())
    *receiver_ = item;
}

// A DownloadService that returns a custom DownloadHistory.
class TestDownloadService : public DownloadService {
 public:
  explicit TestDownloadService(Profile* profile);
  virtual ~TestDownloadService();

  void set_download_history(scoped_ptr<DownloadHistory> download_history) {
    download_history_.swap(download_history);
  }
  virtual DownloadHistory* GetDownloadHistory() OVERRIDE;

 private:
  scoped_ptr<DownloadHistory> download_history_;
};

TestDownloadService::TestDownloadService(Profile* profile)
    : DownloadService(profile) {
}

TestDownloadService::~TestDownloadService() {
}

DownloadHistory* TestDownloadService::GetDownloadHistory() {
  return download_history_.get();
}

// The test fixture:
class DownloadUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  DownloadUIControllerTest();

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;

  // Returns a TestDelegate. Invoking OnNewDownloadReady on the returned
  // delegate results in the DownloadItem* being stored in |notified_item_|.
  scoped_ptr<DownloadUIController::Delegate> GetTestDelegate();

  MockDownloadManager* manager() { return manager_.get(); }

  // Returns the DownloadManager::Observer registered by a test case. This is
  // the DownloadUIController's observer for all current test cases.
  content::DownloadManager::Observer* manager_observer() {
    return manager_observer_;
  }

  // The most recent DownloadItem that was passed into OnNewDownloadReady().
  content::DownloadItem* notified_item() { return notified_item_; }

  // DownloadHistory performs a query of existing downloads when it is first
  // instantiated. This method returns the completion callback for that query.
  // It can be used to inject history downloads.
  const HistoryService::DownloadQueryCallback& history_query_callback() const {
    return history_adapter_->download_query_callback_;
  }

  // DownloadManager::Observer registered by DownloadHistory.
  content::DownloadManager::Observer* download_history_manager_observer() {
    return download_history_manager_observer_;
  }

  scoped_ptr<MockDownloadItem> CreateMockInProgressDownload();

 private:
  // A private history adapter that stores the DownloadQueryCallback when
  // QueryDownloads is called.
  class HistoryAdapter : public DownloadHistory::HistoryAdapter {
   public:
    HistoryAdapter() : DownloadHistory::HistoryAdapter(NULL) {}
    HistoryService::DownloadQueryCallback download_query_callback_;

   private:
    virtual void QueryDownloads(
        const HistoryService::DownloadQueryCallback& callback) OVERRIDE {
      download_query_callback_ = callback;
    }
  };

  // Constructs and returns a TestDownloadService.
  static KeyedService* TestingDownloadServiceFactory(
      content::BrowserContext* browser_context);

  scoped_ptr<MockDownloadManager> manager_;
  content::DownloadManager::Observer* download_history_manager_observer_;
  content::DownloadManager::Observer* manager_observer_;
  content::DownloadItem* notified_item_;
  base::WeakPtrFactory<content::DownloadItem*> notified_item_receiver_factory_;

  HistoryAdapter* history_adapter_;
};

// static
KeyedService* DownloadUIControllerTest::TestingDownloadServiceFactory(
    content::BrowserContext* browser_context) {
  return new TestDownloadService(Profile::FromBrowserContext(browser_context));
}

DownloadUIControllerTest::DownloadUIControllerTest()
    : download_history_manager_observer_(NULL),
      manager_observer_(NULL),
      notified_item_(NULL),
      notified_item_receiver_factory_(&notified_item_) {
}

void DownloadUIControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  manager_.reset(new testing::StrictMock<MockDownloadManager>());
  EXPECT_CALL(*manager_, AddObserver(_))
      .WillOnce(SaveArg<0>(&download_history_manager_observer_));
  EXPECT_CALL(*manager_,
              RemoveObserver(testing::Eq(
                  testing::ByRef(download_history_manager_observer_))))
      .WillOnce(testing::Assign(
          &download_history_manager_observer_,
          static_cast<content::DownloadManager::Observer*>(NULL)));
  EXPECT_CALL(*manager_, GetAllDownloads(_)).Times(AnyNumber());

  scoped_ptr<HistoryAdapter> history_adapter(new HistoryAdapter);
  history_adapter_ = history_adapter.get();
  scoped_ptr<DownloadHistory> download_history(new DownloadHistory(
      manager_.get(),
      history_adapter.PassAs<DownloadHistory::HistoryAdapter>()));
  ASSERT_TRUE(download_history_manager_observer_);

  EXPECT_CALL(*manager_, AddObserver(_))
      .WillOnce(SaveArg<0>(&manager_observer_));
  EXPECT_CALL(*manager_,
              RemoveObserver(testing::Eq(testing::ByRef(manager_observer_))))
      .WillOnce(testing::Assign(
          &manager_observer_,
          static_cast<content::DownloadManager::Observer*>(NULL)));
  TestDownloadService* download_service = static_cast<TestDownloadService*>(
      DownloadServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(), &TestingDownloadServiceFactory));
  ASSERT_TRUE(download_service);
  download_service->set_download_history(download_history.Pass());
}

scoped_ptr<MockDownloadItem>
DownloadUIControllerTest::CreateMockInProgressDownload() {
  scoped_ptr<MockDownloadItem> item(
      new testing::StrictMock<MockDownloadItem>());
  EXPECT_CALL(*item, GetBrowserContext())
      .WillRepeatedly(Return(browser_context()));
  EXPECT_CALL(*item, GetId()).WillRepeatedly(Return(1));
  EXPECT_CALL(*item, GetTargetFilePath()).WillRepeatedly(
      ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  EXPECT_CALL(*item, GetFullPath()).WillRepeatedly(
      ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(content::DownloadItem::IN_PROGRESS));
  EXPECT_CALL(*item, GetUrlChain())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::vector<GURL>()));
  EXPECT_CALL(*item, GetReferrerUrl())
      .WillRepeatedly(testing::ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*item, GetStartTime()).WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*item, GetEndTime()).WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*item, GetETag()).WillRepeatedly(ReturnRefOfCopy(std::string()));
  EXPECT_CALL(*item, GetLastModifiedTime())
      .WillRepeatedly(ReturnRefOfCopy(std::string()));
  EXPECT_CALL(*item, GetDangerType())
      .WillRepeatedly(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(*item, GetLastReason())
      .WillRepeatedly(Return(content::DOWNLOAD_INTERRUPT_REASON_NONE));
  EXPECT_CALL(*item, GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(*item, GetTotalBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(*item, GetTargetDisposition()).WillRepeatedly(
      Return(content::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
  EXPECT_CALL(*item, GetOpened()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item, GetMimeType()).WillRepeatedly(Return(std::string()));
  EXPECT_CALL(*item, GetURL()).WillRepeatedly(testing::ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*item, IsTemporary()).WillRepeatedly(Return(false));
  return item.Pass();
}

scoped_ptr<DownloadUIController::Delegate>
DownloadUIControllerTest::GetTestDelegate() {
  scoped_ptr<DownloadUIController::Delegate> delegate(
      new TestDelegate(notified_item_receiver_factory_.GetWeakPtr()));
  return delegate.Pass();
}

// New downloads should be presented to the UI when GetTargetFilePath() returns
// a non-empty path.  I.e. once the download target has been determined.
TEST_F(DownloadUIControllerTest, DownloadUIController_NotifyBasic) {
  scoped_ptr<MockDownloadItem> item(CreateMockInProgressDownload());
  DownloadUIController controller(manager(), GetTestDelegate());
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());

  // The destination for the download hasn't been determined yet. It should not
  // be displayed.
  EXPECT_FALSE(notified_item());

  // Once the destination has been determined, then it should be displayed.
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  item->NotifyObserversDownloadUpdated();

  EXPECT_EQ(static_cast<content::DownloadItem*>(item.get()), notified_item());
}

// A download that's created in an interrupted state should also be displayed.
TEST_F(DownloadUIControllerTest, DownloadUIController_NotifyBasic_Interrupted) {
  scoped_ptr<MockDownloadItem> item = CreateMockInProgressDownload();
  DownloadUIController controller(manager(), GetTestDelegate());
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(content::DownloadItem::INTERRUPTED));

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());
  EXPECT_EQ(static_cast<content::DownloadItem*>(item.get()), notified_item());
}

// Downloads that have a target path on creation and are in the IN_PROGRESS
// state should be displayed in the UI immediately without requiring an
// additional OnDownloadUpdated() notification.
TEST_F(DownloadUIControllerTest, DownloadUIController_NotifyReadyOnCreate) {
  scoped_ptr<MockDownloadItem> item(CreateMockInProgressDownload());
  DownloadUIController controller(manager(), GetTestDelegate());

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());
  EXPECT_EQ(static_cast<content::DownloadItem*>(item.get()), notified_item());
}

// The UI shouldn't be notified of downloads that were restored from history.
TEST_F(DownloadUIControllerTest, DownloadUIController_HistoryDownload) {
  DownloadUIController controller(manager(), GetTestDelegate());
  // DownloadHistory should already have been created. It performs a query of
  // existing downloads upon creation. We'll use the callback to inject a
  // history download.
  ASSERT_FALSE(history_query_callback().is_null());

  // download_history_manager_observer is the DownloadManager::Observer
  // registered by the DownloadHistory. DownloadHistory relies on the
  // OnDownloadCreated notification to mark a download as having been restored
  // from history.
  ASSERT_TRUE(download_history_manager_observer());

  scoped_ptr<std::vector<history::DownloadRow> > history_downloads;
  history_downloads.reset(new std::vector<history::DownloadRow>());
  history_downloads->push_back(history::DownloadRow());
  history_downloads->front().id = 1;

  std::vector<GURL> url_chain;
  GURL url;
  scoped_ptr<MockDownloadItem> item = CreateMockInProgressDownload();

  EXPECT_CALL(*item, GetOriginalMimeType());
  EXPECT_CALL(*manager(), CheckForHistoryFilesRemoval());

  {
    testing::InSequence s;
    testing::MockFunction<void()> mock_function;
    // DownloadHistory will immediately try to create a download using the info
    // we push through the query callback. When DownloadManager::CreateDownload
    // is called, we need to first invoke the OnDownloadCreated callback for
    // DownloadHistory before returning the DownloadItem since that's the
    // sequence of events expected by DownloadHistory.
    base::Closure history_on_created_callback =
        base::Bind(&content::DownloadManager::Observer::OnDownloadCreated,
                   base::Unretained(download_history_manager_observer()),
                   manager(),
                   item.get());
    EXPECT_CALL(*manager(), MockCreateDownloadItem(_)).WillOnce(
        testing::DoAll(testing::InvokeWithoutArgs(&history_on_created_callback,
                                                  &base::Closure::Run),
                       Return(item.get())));
    EXPECT_CALL(mock_function, Call());

    history_query_callback().Run(history_downloads.Pass());
    mock_function.Call();
  }

  // Now pass along the notification to the OnDownloadCreated observer from
  // DownloadUIController. It should ignore the download since it's marked as
  // being restored from history.
  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());

  // Finally, the expectation we've been waiting for:
  EXPECT_FALSE(notified_item());
}

} // namespace
