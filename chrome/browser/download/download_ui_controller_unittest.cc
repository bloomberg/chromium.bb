// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_ui_controller.h"
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
  virtual void NotifyDownloadStarting(content::DownloadItem* item) OVERRIDE;

  base::WeakPtr<content::DownloadItem*> receiver_;
};

TestDelegate::TestDelegate(base::WeakPtr<content::DownloadItem*> receiver)
    : receiver_(receiver) {
}

void TestDelegate::NotifyDownloadStarting(content::DownloadItem* item) {
  if (receiver_.get())
    *receiver_ = item;
}

class DownloadUIControllerTest : public testing::Test {
 public:
  DownloadUIControllerTest();

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE;

  // Returns a MockDownloadItem that has AddObserver and RemoveObserver
  // expectations set up to store the observer in |item_observer_|.
  scoped_ptr<MockDownloadItem> GetMockDownload();

  // Returns a TestDelegate. Invoking NotifyDownloadStarting on the returned
  // delegate results in the DownloadItem* being stored in |received_item_|.
  scoped_ptr<DownloadUIController::Delegate> GetTestDelegate();

  MockDownloadManager* manager() { return manager_.get(); }
  content::DownloadManager::Observer* manager_observer() {
    return manager_observer_;
  }
  content::DownloadItem::Observer* item_observer() { return item_observer_; }
  content::DownloadItem* received_item() { return received_item_; }

 private:
  scoped_refptr<MockDownloadManager> manager_;
  content::DownloadManager::Observer* manager_observer_;
  content::DownloadItem::Observer* item_observer_;
  content::DownloadItem* received_item_;

  base::WeakPtrFactory<content::DownloadItem*> receiver_factory_;
};

DownloadUIControllerTest::DownloadUIControllerTest()
    : manager_observer_(NULL),
      item_observer_(NULL),
      received_item_(NULL),
      receiver_factory_(&received_item_) {
}

void DownloadUIControllerTest::SetUp() {
  manager_ = new testing::StrictMock<MockDownloadManager>();
  EXPECT_CALL(*manager_, AddObserver(_))
      .WillOnce(SaveArg<0>(&manager_observer_));
  EXPECT_CALL(*manager_, RemoveObserver(_))
      .WillOnce(Assign(&manager_observer_,
                       static_cast<content::DownloadManager::Observer*>(NULL)));
  EXPECT_CALL(*manager_, GetAllDownloads(_));
}

scoped_ptr<MockDownloadItem> DownloadUIControllerTest::GetMockDownload() {
  scoped_ptr<MockDownloadItem> item(
      new testing::StrictMock<MockDownloadItem>());
  EXPECT_CALL(*item, AddObserver(_))
      .WillOnce(SaveArg<0>(&item_observer_));
  EXPECT_CALL(*item, RemoveObserver(_))
      .WillOnce(Assign(&item_observer_,
                       static_cast<content::DownloadItem::Observer*>(NULL)));
  return item.Pass();
}

scoped_ptr<DownloadUIController::Delegate>
DownloadUIControllerTest::GetTestDelegate() {
  scoped_ptr<DownloadUIController::Delegate> delegate(
      new TestDelegate(receiver_factory_.GetWeakPtr()));
  return delegate.Pass();
}

// Normal downloads that are constructed in the IN_PROGRESS state should be
// presented to the UI when GetTargetFilePath() returns a non-empty path.
// I.e. once the download target has been determined.
TEST_F(DownloadUIControllerTest, DownloadUIController_NotifyBasic) {
  scoped_ptr<MockDownloadItem> item = GetMockDownload();
  DownloadUIController controller(manager(), GetTestDelegate());
  EXPECT_CALL(*item, IsInProgress())
      .WillOnce(Return(true));
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*item, IsComplete())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());

  // The destination for the download hasn't been determined yet. It should not
  // be displayed.
  EXPECT_FALSE(received_item());
  ASSERT_TRUE(item_observer());

  // Once the destination has been determined, then it should be displayed.
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  item_observer()->OnDownloadUpdated(item.get());

  EXPECT_EQ(static_cast<content::DownloadItem*>(item.get()), received_item());
}

// Downloads that have a target path on creation and are in the IN_PROGRESS
// state should be displayed in the UI immediately without requiring an
// additional OnDownloadUpdated() notification.
TEST_F(DownloadUIControllerTest, DownloadUIController_NotifyReadyOnCreate) {
  scoped_ptr<MockDownloadItem> item = GetMockDownload();
  DownloadUIController controller(manager(), GetTestDelegate());
  EXPECT_CALL(*item, IsInProgress())
      .WillOnce(Return(true));
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  EXPECT_CALL(*item, IsComplete())
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());
  EXPECT_EQ(static_cast<content::DownloadItem*>(item.get()), received_item());
}

// History downloads (downloads that are not in IN_PROGRESS on create) should
// not be displayed on the shelf.
TEST_F(DownloadUIControllerTest, DownloadUIController_NoNotifyHistory) {
  scoped_ptr<MockDownloadItem> item = GetMockDownload();
  DownloadUIController controller(manager(), GetTestDelegate());
  EXPECT_CALL(*item, IsInProgress())
      .WillOnce(Return(false));
  EXPECT_CALL(*item, IsComplete())
      .Times(AnyNumber())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());
  EXPECT_FALSE(received_item());

  item_observer()->OnDownloadUpdated(item.get());
  EXPECT_FALSE(received_item());
}

} // namespace
