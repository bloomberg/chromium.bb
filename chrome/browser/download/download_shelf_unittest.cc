// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/test_download_shelf.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::SaveArg;
using ::testing::_;
using content::DownloadItem;

namespace {

class DownloadShelfTest : public testing::Test {
 public:
  DownloadShelfTest();

 protected:
  content::MockDownloadItem* download_item() {
    return download_item_.get();
  }
  content::MockDownloadManager* download_manager() {
    return download_manager_.get();
  }
  TestDownloadShelf* shelf() {
    return &shelf_;
  }

 private:
  scoped_ptr<content::MockDownloadItem> GetInProgressMockDownload();

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<content::MockDownloadItem> download_item_;
  scoped_refptr<content::MockDownloadManager> download_manager_;
  TestDownloadShelf shelf_;
};

DownloadShelfTest::DownloadShelfTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  download_item_.reset(new ::testing::NiceMock<content::MockDownloadItem>());
  ON_CALL(*download_item_, GetAutoOpened()).WillByDefault(Return(false));
  ON_CALL(*download_item_, GetMimeType()).WillByDefault(Return("text/plain"));
  ON_CALL(*download_item_, GetOpenWhenComplete()).WillByDefault(Return(false));
  ON_CALL(*download_item_, GetTargetDisposition())
      .WillByDefault(Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));
  ON_CALL(*download_item_, GetURL())
      .WillByDefault(ReturnRefOfCopy(GURL("http://example.com/foo")));
  ON_CALL(*download_item_, IsComplete()).WillByDefault(Return(false));
  ON_CALL(*download_item_, IsInProgress()).WillByDefault(Return(true));
  ON_CALL(*download_item_, IsTemporary()).WillByDefault(Return(false));
  ON_CALL(*download_item_, ShouldOpenFileBasedOnExtension())
      .WillByDefault(Return(false));

  download_manager_ = new ::testing::NiceMock<content::MockDownloadManager>();
  ON_CALL(*download_manager_, GetDownload(_))
      .WillByDefault(Return(download_item_.get()));

  shelf_.set_download_manager(download_manager_.get());
}

} // namespace

TEST_F(DownloadShelfTest, ClosesShelfWhenHidden) {
  shelf()->Show();
  EXPECT_TRUE(shelf()->IsShowing());
  shelf()->Hide();
  EXPECT_FALSE(shelf()->IsShowing());
  shelf()->Unhide();
  EXPECT_TRUE(shelf()->IsShowing());
}

TEST_F(DownloadShelfTest, CloseWhileHiddenPreventsShowOnUnhide) {
  shelf()->Show();
  shelf()->Hide();
  shelf()->Close();
  shelf()->Unhide();
  EXPECT_FALSE(shelf()->IsShowing());
}

TEST_F(DownloadShelfTest, UnhideDoesntShowIfNotShownOnHide) {
  shelf()->Hide();
  shelf()->Unhide();
  EXPECT_FALSE(shelf()->IsShowing());
}

TEST_F(DownloadShelfTest, AddDownloadWhileHiddenUnhides) {
  shelf()->Show();
  shelf()->Hide();
  shelf()->AddDownload(download_item());
  EXPECT_TRUE(shelf()->IsShowing());
}

TEST_F(DownloadShelfTest, AddDownloadWhileHiddenUnhidesAndShows) {
  shelf()->Hide();
  shelf()->AddDownload(download_item());
  EXPECT_TRUE(shelf()->IsShowing());
}

// Normal downloads should be added synchronously and cause the shelf to show.
TEST_F(DownloadShelfTest, AddNormalDownload) {
  EXPECT_FALSE(shelf()->IsShowing());
  shelf()->AddDownload(download_item());
  EXPECT_TRUE(shelf()->did_add_download());
  EXPECT_TRUE(shelf()->IsShowing());
}

// Add a transient download. It should not be added immediately. Instead it
// should be added after a delay. For testing, the delay is set to 0 seconds. So
// the download should be added once the message loop is flushed.
TEST_F(DownloadShelfTest, AddDelayedDownload) {
  EXPECT_CALL(*download_item(), ShouldOpenFileBasedOnExtension())
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(DownloadItemModel(download_item())
              .ShouldRemoveFromShelfWhenComplete());
  shelf()->AddDownload(download_item());

  EXPECT_FALSE(shelf()->did_add_download());
  EXPECT_FALSE(shelf()->IsShowing());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(shelf()->did_add_download());
  EXPECT_TRUE(shelf()->IsShowing());
}

// Add a transient download that completes before the delay. It should not be
// displayed on the shelf.
TEST_F(DownloadShelfTest, AddDelayedCompletedDownload) {
  EXPECT_CALL(*download_item(), ShouldOpenFileBasedOnExtension())
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(DownloadItemModel(download_item())
              .ShouldRemoveFromShelfWhenComplete());
  shelf()->AddDownload(download_item());

  EXPECT_FALSE(shelf()->did_add_download());
  EXPECT_FALSE(shelf()->IsShowing());

  EXPECT_CALL(*download_item(), IsComplete())
      .WillRepeatedly(Return(true));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_FALSE(shelf()->did_add_download());
  EXPECT_FALSE(shelf()->IsShowing());
}

// Add a transient download that completes and becomes non-transient before the
// delay. It should be displayed on the shelf even though it is complete.
TEST_F(DownloadShelfTest, AddDelayedCompleteNonTransientDownload) {
  EXPECT_CALL(*download_item(), ShouldOpenFileBasedOnExtension())
      .WillRepeatedly(Return(true));
  ASSERT_TRUE(DownloadItemModel(download_item())
              .ShouldRemoveFromShelfWhenComplete());
  shelf()->AddDownload(download_item());

  EXPECT_FALSE(shelf()->did_add_download());
  EXPECT_FALSE(shelf()->IsShowing());

  EXPECT_CALL(*download_item(), IsComplete())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*download_item(), ShouldOpenFileBasedOnExtension())
      .WillRepeatedly(Return(false));
  ASSERT_FALSE(DownloadItemModel(download_item())
               .ShouldRemoveFromShelfWhenComplete());

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_TRUE(shelf()->did_add_download());
  EXPECT_TRUE(shelf()->IsShowing());
}
