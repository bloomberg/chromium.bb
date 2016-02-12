// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_item_impl.h"

#include <stdint.h>

#include <iterator>
#include <queue>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/mock_download_file.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_destination_observer.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::_;

const int kDownloadChunkSize = 1000;
const int kDownloadSpeed = 1000;
const base::FilePath::CharType kDummyTargetPath[] =
    FILE_PATH_LITERAL("/testpath");
const base::FilePath::CharType kDummyIntermediatePath[] =
    FILE_PATH_LITERAL("/testpathx");

namespace content {

namespace {

class MockDelegate : public DownloadItemImplDelegate {
 public:
  MockDelegate() : DownloadItemImplDelegate() {
    SetDefaultExpectations();
  }

  MOCK_METHOD2(DetermineDownloadTarget, void(
      DownloadItemImpl*, const DownloadTargetCallback&));
  MOCK_METHOD2(ShouldCompleteDownload,
               bool(DownloadItemImpl*, const base::Closure&));
  MOCK_METHOD2(ShouldOpenDownload,
               bool(DownloadItemImpl*, const ShouldOpenDownloadCallback&));
  MOCK_METHOD1(ShouldOpenFileBasedOnExtension, bool(const base::FilePath&));
  MOCK_METHOD1(CheckForFileRemoval, void(DownloadItemImpl*));

  void ResumeInterruptedDownload(scoped_ptr<DownloadUrlParameters> params,
                                 uint32_t id) override {
    MockResumeInterruptedDownload(params.get(), id);
  }
  MOCK_METHOD2(MockResumeInterruptedDownload,
               void(DownloadUrlParameters* params, uint32_t id));

  MOCK_CONST_METHOD0(GetBrowserContext, BrowserContext*());
  MOCK_METHOD1(UpdatePersistence, void(DownloadItemImpl*));
  MOCK_METHOD1(DownloadOpened, void(DownloadItemImpl*));
  MOCK_METHOD1(DownloadRemoved, void(DownloadItemImpl*));
  MOCK_CONST_METHOD1(AssertStateConsistent, void(DownloadItemImpl*));

  void VerifyAndClearExpectations() {
    ::testing::Mock::VerifyAndClearExpectations(this);
    SetDefaultExpectations();
  }

 private:
  void SetDefaultExpectations() {
    EXPECT_CALL(*this, AssertStateConsistent(_))
        .WillRepeatedly(Return());
    EXPECT_CALL(*this, ShouldOpenFileBasedOnExtension(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldOpenDownload(_, _))
        .WillRepeatedly(Return(true));
  }
};

class MockRequestHandle : public DownloadRequestHandleInterface {
 public:
  MOCK_CONST_METHOD0(GetWebContents, WebContents*());
  MOCK_CONST_METHOD0(GetDownloadManager, DownloadManager*());
  MOCK_CONST_METHOD0(PauseRequest, void());
  MOCK_CONST_METHOD0(ResumeRequest, void());
  MOCK_CONST_METHOD0(CancelRequest, void());
  MOCK_CONST_METHOD0(DebugString, std::string());
};

class TestDownloadItemObserver : public DownloadItem::Observer {
 public:
  explicit TestDownloadItemObserver(DownloadItem* item)
      : item_(item),
        last_state_(item->GetState()),
        removed_(false),
        destroyed_(false),
        updated_(false),
        interrupt_count_(0),
        resume_count_(0) {
    item_->AddObserver(this);
  }

  ~TestDownloadItemObserver() override {
    if (item_)
      item_->RemoveObserver(this);
  }

  bool download_removed() const { return removed_; }
  bool download_destroyed() const { return destroyed_; }
  int interrupt_count() const { return interrupt_count_; }
  int resume_count() const { return resume_count_; }

  bool CheckAndResetDownloadUpdated() {
    bool was_updated = updated_;
    updated_ = false;
    return was_updated;
  }

 private:
  void OnDownloadRemoved(DownloadItem* download) override {
    DVLOG(20) << " " << __FUNCTION__
              << " download = " << download->DebugString(false);
    removed_ = true;
  }

  void OnDownloadUpdated(DownloadItem* download) override {
    DVLOG(20) << " " << __FUNCTION__
              << " download = " << download->DebugString(false);
    updated_ = true;
    DownloadItem::DownloadState new_state = download->GetState();
    if (last_state_ == DownloadItem::IN_PROGRESS &&
        new_state == DownloadItem::INTERRUPTED) {
      interrupt_count_++;
    }
    if (last_state_ == DownloadItem::INTERRUPTED &&
        new_state == DownloadItem::IN_PROGRESS) {
      resume_count_++;
    }
    last_state_ = new_state;
  }

  void OnDownloadOpened(DownloadItem* download) override {
    DVLOG(20) << " " << __FUNCTION__
              << " download = " << download->DebugString(false);
  }

  void OnDownloadDestroyed(DownloadItem* download) override {
    DVLOG(20) << " " << __FUNCTION__
              << " download = " << download->DebugString(false);
    destroyed_ = true;
    item_->RemoveObserver(this);
    item_ = nullptr;
  }

  DownloadItem* item_;
  DownloadItem::DownloadState last_state_;
  bool removed_;
  bool destroyed_;
  bool updated_;
  int interrupt_count_;
  int resume_count_;
};

// Schedules a task to invoke the RenameCompletionCallback with |new_path| on
// the UI thread. Should only be used as the action for
// MockDownloadFile::Rename as follows:
//   EXPECT_CALL(download_file, Rename*(_,_))
//       .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
//                                        new_path));
ACTION_P2(ScheduleRenameCallback, interrupt_reason, new_path) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(arg1, interrupt_reason, new_path));
}

// Schedules a task to invoke a callback that's bound to the specified
// parameter.
// E.g.:
//
//   EXPECT_CALL(foo, Bar(1, _))
//     .WithArg<1>(ScheduleCallbackWithParam(0));
//
//   .. will invoke the second argument to Bar with 0 as the parameter.
ACTION_P(ScheduleCallbackWithParam, param) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(arg0, param));
}

ACTION_P(ScheduleClosure, closure) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, closure);
}

}  // namespace

class DownloadItemTest : public testing::Test {
 public:
  DownloadItemTest() {
    base::FeatureList::ClearInstanceForTesting();
    scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(features::kDownloadResumption.name,
                                            std::string());
    base::FeatureList::SetInstance(std::move(feature_list));
  }

  ~DownloadItemTest() {
    RunAllPendingInMessageLoops();
    STLDeleteElements(&allocated_downloads_);
  }

  DownloadItemImpl* CreateDownloadItemWithCreateInfo(
      scoped_ptr<DownloadCreateInfo> info) {
    DownloadItemImpl* download = new DownloadItemImpl(
        &delegate_, next_download_id_++, *(info.get()), net::BoundNetLog());
    allocated_downloads_.insert(download);
    return download;
  }

  // This class keeps ownership of the created download item; it will
  // be torn down at the end of the test unless DestroyDownloadItem is
  // called.
  DownloadItemImpl* CreateDownloadItem() {
    scoped_ptr<DownloadCreateInfo> info;

    info.reset(new DownloadCreateInfo());
    info->save_info = scoped_ptr<DownloadSaveInfo>(new DownloadSaveInfo());
    info->save_info->prompt_for_save_location = false;
    info->url_chain.push_back(GURL("http://example.com/download"));
    info->etag = "SomethingToSatisfyResumption";

    return CreateDownloadItemWithCreateInfo(std::move(info));
  }

  // Add DownloadFile to DownloadItem
  MockDownloadFile* CallDownloadItemStart(
      DownloadItemImpl* item,
      DownloadItemImplDelegate::DownloadTargetCallback* callback) {
    MockDownloadFile* mock_download_file(new StrictMock<MockDownloadFile>);
    scoped_ptr<DownloadFile> download_file(mock_download_file);
    EXPECT_CALL(*mock_download_file, Initialize(_));
    if (callback) {
      // Save the callback.
      EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(item, _))
          .WillOnce(SaveArg<1>(callback));
    } else {
      // Drop it on the floor.
      EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(item, _));
    }

    scoped_ptr<DownloadRequestHandleInterface> request_handle(
        new NiceMock<MockRequestHandle>);
    item->Start(std::move(download_file), std::move(request_handle));
    RunAllPendingInMessageLoops();

    // So that we don't have a function writing to a stack variable
    // lying around if the above failed.
    mock_delegate()->VerifyAndClearExpectations();
    EXPECT_CALL(*mock_delegate(), AssertStateConsistent(_))
        .WillRepeatedly(Return());
    EXPECT_CALL(*mock_delegate(), ShouldOpenFileBasedOnExtension(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(_, _))
        .WillRepeatedly(Return(true));

    return mock_download_file;
  }

  // Perform the intermediate rename for |item|. The target path for the
  // download will be set to kDummyTargetPath. Returns the MockDownloadFile*
  // that was added to the DownloadItem.
  MockDownloadFile* DoIntermediateRename(DownloadItemImpl* item,
                                         DownloadDangerType danger_type) {
    EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
    EXPECT_TRUE(item->GetTargetFilePath().empty());
    DownloadItemImplDelegate::DownloadTargetCallback callback;
    MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
    base::FilePath target_path(kDummyTargetPath);
    base::FilePath intermediate_path(kDummyIntermediatePath);
    EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
        .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                         intermediate_path));
    callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 danger_type, intermediate_path);
    RunAllPendingInMessageLoops();
    return download_file;
  }

  void DoDestinationComplete(DownloadItemImpl* item,
                             MockDownloadFile* download_file) {
    EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(_, _))
        .WillOnce(Return(true));
    base::FilePath final_path(kDummyTargetPath);
    EXPECT_CALL(*download_file, RenameAndAnnotate(_, _))
        .WillOnce(
            ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE, final_path));
    EXPECT_CALL(*download_file, FullPath())
        .WillRepeatedly(Return(base::FilePath(kDummyTargetPath)));
    EXPECT_CALL(*download_file, Detach());

    item->DestinationObserverAsWeakPtr()->DestinationCompleted("");
    RunAllPendingInMessageLoops();
  }

  // Cleanup a download item (specifically get rid of the DownloadFile on it).
  // The item must be in the expected state.
  void CleanupItem(DownloadItemImpl* item,
                   MockDownloadFile* download_file,
                   DownloadItem::DownloadState expected_state) {
    EXPECT_EQ(expected_state, item->GetState());

    if (expected_state == DownloadItem::IN_PROGRESS) {
      if (download_file)
        EXPECT_CALL(*download_file, Cancel());
      item->Cancel(true);
      RunAllPendingInMessageLoops();
    }
  }

  // Destroy a previously created download item.
  void DestroyDownloadItem(DownloadItem* item) {
    allocated_downloads_.erase(item);
    delete item;
  }

  void RunAllPendingInMessageLoops() { base::RunLoop().RunUntilIdle(); }

  MockDelegate* mock_delegate() {
    return &delegate_;
  }

  void OnDownloadFileAcquired(base::FilePath* return_path,
                              const base::FilePath& path) {
    *return_path = path;
  }

 private:
  int next_download_id_ = DownloadItem::kInvalidId + 1;
  StrictMock<MockDelegate> delegate_;
  std::set<DownloadItem*> allocated_downloads_;
  TestBrowserThreadBundle thread_bundle_;
};

// Tests to ensure calls that change a DownloadItem generate an update to
// observers.
// State changing functions not tested:
//  void OpenDownload();
//  void ShowDownloadInShell();
//  void CompleteDelayedDownload();
//  set_* mutators

TEST_F(DownloadItemTest, NotificationAfterUpdate) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  TestDownloadItemObserver observer(item);

  item->DestinationUpdate(kDownloadChunkSize, kDownloadSpeed, std::string());
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ(kDownloadSpeed, item->CurrentSpeed());
  CleanupItem(item, file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, NotificationAfterCancel) {
  DownloadItemImpl* user_cancel = CreateDownloadItem();
  MockDownloadFile* download_file = CallDownloadItemStart(user_cancel, nullptr);
  EXPECT_CALL(*download_file, Cancel());
  TestDownloadItemObserver observer1(user_cancel);

  user_cancel->Cancel(true);
  ASSERT_TRUE(observer1.CheckAndResetDownloadUpdated());

  DownloadItemImpl* system_cancel = CreateDownloadItem();
  download_file = CallDownloadItemStart(system_cancel, nullptr);
  EXPECT_CALL(*download_file, Cancel());
  TestDownloadItemObserver observer2(system_cancel);

  system_cancel->Cancel(false);
  ASSERT_TRUE(observer2.CheckAndResetDownloadUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterComplete) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  DoDestinationComplete(item, download_file);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterDownloadedFileRemoved) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);

  item->OnDownloadedFileRemoved();
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterInterrupted) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_CALL(*download_file, Cancel());
  TestDownloadItemObserver observer(item);

  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(_,_))
      .Times(0);

  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterDestroyed) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);

  DestroyDownloadItem(item);
  ASSERT_TRUE(observer.download_destroyed());
}

TEST_F(DownloadItemTest, ContinueAfterInterrupted) {
  TestBrowserContext test_browser_context;
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download, using a continuable interrupt.
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  EXPECT_CALL(*mock_delegate(), GetBrowserContext())
      .WillRepeatedly(Return(&test_browser_context));
  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(_, _)).Times(1);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  // Since the download is resumed automatically, the interrupt count doesn't
  // increase.
  ASSERT_EQ(0, observer.interrupt_count());

  // Test expectations verify that ResumeInterruptedDownload() is called (by way
  // of MockResumeInterruptedDownload) after the download is interrupted. But
  // the mock doesn't follow through with the resumption.
  // ResumeInterruptedDownload() being called is sufficient for verifying that
  // the automatic resumption was triggered.
  RunAllPendingInMessageLoops();

  // The download item is currently in RESUMING_INTERNAL state, which maps to
  // IN_PROGRESS.
  CleanupItem(item, nullptr, DownloadItem::IN_PROGRESS);
}

// Same as above, but with a non-continuable interrupt.
TEST_F(DownloadItemTest, RestartAfterInterrupted) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download, using a restartable interrupt.
  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  // Should not try to auto-resume.
  ASSERT_EQ(1, observer.interrupt_count());
  ASSERT_EQ(0, observer.resume_count());
  RunAllPendingInMessageLoops();

  CleanupItem(item, nullptr, DownloadItem::INTERRUPTED);
}

// Check we do correct cleanup for RESUME_MODE_INVALID interrupts.
TEST_F(DownloadItemTest, UnresumableInterrupt) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Fail final rename with unresumable reason.
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file,
              RenameAndAnnotate(base::FilePath(kDummyTargetPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
                                       base::FilePath()));
  EXPECT_CALL(*download_file, Cancel());

  // Complete download to trigger final rename.
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  RunAllPendingInMessageLoops();

  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  // Should not try to auto-resume.
  ASSERT_EQ(1, observer.interrupt_count());
  ASSERT_EQ(0, observer.resume_count());

  CleanupItem(item, nullptr, DownloadItem::INTERRUPTED);
}

TEST_F(DownloadItemTest, LimitRestartsAfterInterrupted) {
  TestBrowserContext test_browser_context;
  DownloadItemImpl* item = CreateDownloadItem();
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);
  MockDownloadFile* mock_download_file(nullptr);
  scoped_ptr<DownloadFile> download_file;
  MockRequestHandle* mock_request_handle(nullptr);
  scoped_ptr<DownloadRequestHandleInterface> request_handle;
  DownloadItemImplDelegate::DownloadTargetCallback callback;

  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(item, _))
      .WillRepeatedly(SaveArg<1>(&callback));
  EXPECT_CALL(*mock_delegate(), GetBrowserContext())
      .WillRepeatedly(Return(&test_browser_context));
  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(_, _))
      .Times(DownloadItemImpl::kMaxAutoResumeAttempts);
  for (int i = 0; i < (DownloadItemImpl::kMaxAutoResumeAttempts + 1); ++i) {
    DVLOG(20) << "Loop iteration " << i;

    mock_download_file = new NiceMock<MockDownloadFile>;
    download_file.reset(mock_download_file);
    mock_request_handle = new NiceMock<MockRequestHandle>;
    request_handle.reset(mock_request_handle);

    ON_CALL(*mock_download_file, FullPath())
        .WillByDefault(Return(base::FilePath()));

    // Copied key parts of DoIntermediateRename & CallDownloadItemStart
    // to allow for holding onto the request handle.
    item->Start(std::move(download_file), std::move(request_handle));
    RunAllPendingInMessageLoops();

    base::FilePath target_path(kDummyTargetPath);
    base::FilePath intermediate_path(kDummyIntermediatePath);
    if (i == 0) {
      // RenameAndUniquify is only called the first time. In all the subsequent
      // iterations, the intermediate file already has the correct name, hence
      // no rename is necessary.
      EXPECT_CALL(*mock_download_file, RenameAndUniquify(intermediate_path, _))
          .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                           intermediate_path));
    }
    callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
    RunAllPendingInMessageLoops();

    // Use a continuable interrupt.
    item->DestinationObserverAsWeakPtr()->DestinationError(
        DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR);

    ::testing::Mock::VerifyAndClearExpectations(mock_download_file);
  }

  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(1, observer.interrupt_count());
  CleanupItem(item, nullptr, DownloadItem::INTERRUPTED);
}

// Test that resumption uses the final URL in a URL chain when resuming.
TEST_F(DownloadItemTest, ResumeUsingFinalURL) {
  TestBrowserContext test_browser_context;
  scoped_ptr<DownloadCreateInfo> create_info(new DownloadCreateInfo);
  create_info->save_info = scoped_ptr<DownloadSaveInfo>(new DownloadSaveInfo());
  create_info->save_info->prompt_for_save_location = false;
  create_info->etag = "SomethingToSatisfyResumption";
  create_info->url_chain.push_back(GURL("http://example.com/a"));
  create_info->url_chain.push_back(GURL("http://example.com/b"));
  create_info->url_chain.push_back(GURL("http://example.com/c"));

  DownloadItemImpl* item =
      CreateDownloadItemWithCreateInfo(std::move(create_info));
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download, using a continuable interrupt.
  EXPECT_CALL(*download_file, FullPath()).WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  EXPECT_CALL(*mock_delegate(), GetBrowserContext())
      .WillRepeatedly(Return(&test_browser_context));
  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(
                                    Property(&DownloadUrlParameters::url,
                                             GURL("http://example.com/c")),
                                    _))
      .Times(1);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR);

  // Test expectations verify that ResumeInterruptedDownload() is called (by way
  // of MockResumeInterruptedDownload) after the download is interrupted. But
  // the mock doesn't follow through with the resumption.
  // ResumeInterruptedDownload() being called is sufficient for verifying that
  // the resumption was triggered.
  RunAllPendingInMessageLoops();

  // The download is currently in RESUMING_INTERNAL, which maps to IN_PROGRESS.
  CleanupItem(item, nullptr, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, NotificationAfterRemove) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file = CallDownloadItemStart(item, nullptr);
  EXPECT_CALL(*download_file, Cancel());
  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  TestDownloadItemObserver observer(item);

  item->Remove();
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  ASSERT_TRUE(observer.download_removed());
}

TEST_F(DownloadItemTest, NotificationAfterOnContentCheckCompleted) {
  // Setting to NOT_DANGEROUS does not trigger a notification.
  DownloadItemImpl* safe_item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(safe_item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  TestDownloadItemObserver safe_observer(safe_item);

  safe_item->OnAllDataSaved(std::string());
  EXPECT_TRUE(safe_observer.CheckAndResetDownloadUpdated());
  safe_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_TRUE(safe_observer.CheckAndResetDownloadUpdated());
  CleanupItem(safe_item, download_file, DownloadItem::IN_PROGRESS);

  // Setting to unsafe url or unsafe file should trigger a notification.
  DownloadItemImpl* unsafeurl_item =
      CreateDownloadItem();
  download_file =
      DoIntermediateRename(unsafeurl_item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  TestDownloadItemObserver unsafeurl_observer(unsafeurl_item);

  unsafeurl_item->OnAllDataSaved(std::string());
  EXPECT_TRUE(unsafeurl_observer.CheckAndResetDownloadUpdated());
  unsafeurl_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
  EXPECT_TRUE(unsafeurl_observer.CheckAndResetDownloadUpdated());

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _));
  unsafeurl_item->ValidateDangerousDownload();
  EXPECT_TRUE(unsafeurl_observer.CheckAndResetDownloadUpdated());
  CleanupItem(unsafeurl_item, download_file, DownloadItem::IN_PROGRESS);

  DownloadItemImpl* unsafefile_item =
      CreateDownloadItem();
  download_file =
      DoIntermediateRename(unsafefile_item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  TestDownloadItemObserver unsafefile_observer(unsafefile_item);

  unsafefile_item->OnAllDataSaved(std::string());
  EXPECT_TRUE(unsafefile_observer.CheckAndResetDownloadUpdated());
  unsafefile_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  EXPECT_TRUE(unsafefile_observer.CheckAndResetDownloadUpdated());

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _));
  unsafefile_item->ValidateDangerousDownload();
  EXPECT_TRUE(unsafefile_observer.CheckAndResetDownloadUpdated());
  CleanupItem(unsafefile_item, download_file, DownloadItem::IN_PROGRESS);
}

// DownloadItemImpl::OnDownloadTargetDetermined will schedule a task to run
// DownloadFile::Rename(). Once the rename
// completes, DownloadItemImpl receives a notification with the new file
// name. Check that observers are updated when the new filename is available and
// not before.
TEST_F(DownloadItemTest, NotificationAfterOnDownloadTargetDetermined) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  TestDownloadItemObserver observer(item);
  base::FilePath target_path(kDummyTargetPath);
  base::FilePath intermediate_path(target_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      target_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       new_intermediate_path));

  // Currently, a notification would be generated if the danger type is anything
  // other than NOT_DANGEROUS.
  callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());
  RunAllPendingInMessageLoops();
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ(new_intermediate_path, item->GetFullPath());

  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, NotificationAfterTogglePause) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* mock_download_file(new MockDownloadFile);
  scoped_ptr<DownloadFile> download_file(mock_download_file);
  scoped_ptr<DownloadRequestHandleInterface> request_handle(
      new NiceMock<MockRequestHandle>);

  EXPECT_CALL(*mock_download_file, Initialize(_));
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(_, _));
  item->Start(std::move(download_file), std::move(request_handle));

  item->Pause();
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());

  ASSERT_TRUE(item->IsPaused());

  item->Resume();
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());

  RunAllPendingInMessageLoops();

  CleanupItem(item, mock_download_file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, DisplayName) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  base::FilePath target_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(target_path.InsertBeforeExtensionASCII("x"));
  EXPECT_EQ(FILE_PATH_LITERAL(""),
            item->GetFileNameToReportUser().value());
  EXPECT_CALL(*download_file, RenameAndUniquify(_, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       intermediate_path));
  callback.Run(target_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  EXPECT_EQ(FILE_PATH_LITERAL("foo.bar"),
            item->GetFileNameToReportUser().value());
  item->SetDisplayName(base::FilePath(FILE_PATH_LITERAL("new.name")));
  EXPECT_EQ(FILE_PATH_LITERAL("new.name"),
            item->GetFileNameToReportUser().value());
  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

// Test to make sure that Start method calls DF initialize properly.
TEST_F(DownloadItemTest, Start) {
  MockDownloadFile* mock_download_file(new MockDownloadFile);
  scoped_ptr<DownloadFile> download_file(mock_download_file);
  DownloadItemImpl* item = CreateDownloadItem();
  EXPECT_CALL(*mock_download_file, Initialize(_));
  scoped_ptr<DownloadRequestHandleInterface> request_handle(
      new NiceMock<MockRequestHandle>);
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(item, _));
  item->Start(std::move(download_file), std::move(request_handle));
  RunAllPendingInMessageLoops();

  CleanupItem(item, mock_download_file, DownloadItem::IN_PROGRESS);
}

// Download file and the request should be cancelled as a result of download
// file initialization failing.
TEST_F(DownloadItemTest, InitDownloadFileFails) {
  scoped_ptr<MockDownloadFile> file(new MockDownloadFile());
  scoped_ptr<MockRequestHandle> request_handle(new MockRequestHandle());
  EXPECT_CALL(*file, Cancel());
  EXPECT_CALL(*request_handle, CancelRequest());
  EXPECT_CALL(*file, Initialize(_))
      .WillOnce(ScheduleCallbackWithParam(
          DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED));

  DownloadItemImpl* item = CreateDownloadItem();
  item->Start(std::move(file), std::move(request_handle));
  RunAllPendingInMessageLoops();

  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
            item->GetLastReason());
}

// Test that the delegate is invoked after the download file is renamed.
TEST_F(DownloadItemTest, CallbackAfterRename) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       new_intermediate_path));

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(final_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       final_path));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  RunAllPendingInMessageLoops();
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
}

// Test that the delegate is invoked after the download file is renamed and the
// download item is in an interrupted state.
TEST_F(DownloadItemTest, CallbackAfterInterruptedRename) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
                                       new_intermediate_path));
  EXPECT_CALL(*download_file, Cancel())
      .Times(1);

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
}

TEST_F(DownloadItemTest, Interrupted) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  const DownloadInterruptReason reason(
      DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);

  // Confirm interrupt sets state properly.
  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(reason);
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(reason, item->GetLastReason());

  // Cancel should kill it.
  item->Cancel(true);
  EXPECT_EQ(DownloadItem::CANCELLED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, item->GetLastReason());
}

// Destination errors that occur before the intermediate rename shouldn't cause
// the download to be marked as interrupted until after the intermediate rename.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Restart) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       new_intermediate_path));
  EXPECT_CALL(*download_file, Cancel())
      .Times(1);

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_TRUE(item->GetFullPath().empty());
  EXPECT_EQ(final_path, item->GetTargetFilePath());
}

// As above. But if the download can be resumed by continuing, then the
// intermediate path should be retained when the download is interrupted after
// the intermediate rename succeeds.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Continue) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       new_intermediate_path));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath(new_intermediate_path)));
  EXPECT_CALL(*download_file, Detach());

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(new_intermediate_path, item->GetFullPath());
  EXPECT_EQ(final_path, item->GetTargetFilePath());
}

// As above. If the intermediate rename fails, then the interrupt reason should
// be set to the destination error and the intermediate path should be empty.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Failed) {
  DownloadItemImpl* item = CreateDownloadItem();
  DownloadItemImplDelegate::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  EXPECT_CALL(*download_file, RenameAndUniquify(intermediate_path, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
                                       new_intermediate_path));
  EXPECT_CALL(*download_file, Cancel())
      .Times(1);

  callback.Run(final_path, DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, intermediate_path);
  RunAllPendingInMessageLoops();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, item->GetLastReason());
  EXPECT_TRUE(item->GetFullPath().empty());
  EXPECT_EQ(final_path, item->GetTargetFilePath());
}

TEST_F(DownloadItemTest, Canceled) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file = CallDownloadItemStart(item, nullptr);

  // Confirm cancel sets state properly.
  EXPECT_CALL(*download_file, Cancel());
  item->Cancel(true);
  EXPECT_EQ(DownloadItem::CANCELLED, item->GetState());
}

TEST_F(DownloadItemTest, FileRemoved) {
  DownloadItemImpl* item = CreateDownloadItem();

  EXPECT_FALSE(item->GetFileExternallyRemoved());
  item->OnDownloadedFileRemoved();
  EXPECT_TRUE(item->GetFileExternallyRemoved());
}

TEST_F(DownloadItemTest, DestinationUpdate) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);

  EXPECT_EQ(0l, item->CurrentSpeed());
  EXPECT_EQ("", item->GetHashState());
  EXPECT_EQ(0l, item->GetReceivedBytes());
  EXPECT_EQ(0l, item->GetTotalBytes());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());
  item->SetTotalBytes(100l);
  EXPECT_EQ(100l, item->GetTotalBytes());

  as_observer->DestinationUpdate(10, 20, "deadbeef");
  EXPECT_EQ(20l, item->CurrentSpeed());
  EXPECT_EQ("deadbeef", item->GetHashState());
  EXPECT_EQ(10l, item->GetReceivedBytes());
  EXPECT_EQ(100l, item->GetTotalBytes());
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());

  as_observer->DestinationUpdate(200, 20, "livebeef");
  EXPECT_EQ(20l, item->CurrentSpeed());
  EXPECT_EQ("livebeef", item->GetHashState());
  EXPECT_EQ(200l, item->GetReceivedBytes());
  EXPECT_EQ(0l, item->GetTotalBytes());
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());

  CleanupItem(item, file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, DestinationError) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, item->GetLastReason());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());

  EXPECT_CALL(*download_file, Cancel());
  as_observer->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
            item->GetLastReason());
}

TEST_F(DownloadItemTest, DestinationCompleted) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file = CallDownloadItemStart(item, nullptr);
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ("", item->GetHash());
  EXPECT_EQ("", item->GetHashState());
  EXPECT_FALSE(item->AllDataSaved());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());

  as_observer->DestinationUpdate(10, 20, "deadbeef");
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());  // Confirm reset.
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ("", item->GetHash());
  EXPECT_EQ("deadbeef", item->GetHashState());
  EXPECT_FALSE(item->AllDataSaved());

  as_observer->DestinationCompleted("livebeef");
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ("livebeef", item->GetHash());
  EXPECT_EQ("", item->GetHashState());
  EXPECT_TRUE(item->AllDataSaved());

  // Even though the DownloadItem receives a DestinationCompleted() event,
  // target determination hasn't completed, hence the download item is stuck in
  // TARGET_PENDING.
  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, EnabledActionsForNormalDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // InProgress
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  EXPECT_TRUE(item->CanShowInFolder());
  EXPECT_TRUE(item->CanOpenDownload());

  // Complete
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyTargetPath)));
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  RunAllPendingInMessageLoops();

  ASSERT_EQ(DownloadItem::COMPLETE, item->GetState());
  EXPECT_TRUE(item->CanShowInFolder());
  EXPECT_TRUE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForTemporaryDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  item->SetIsTemporary(true);

  // InProgress Temporary
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  ASSERT_TRUE(item->IsTemporary());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());

  // Complete Temporary
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyTargetPath)));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  RunAllPendingInMessageLoops();

  ASSERT_EQ(DownloadItem::COMPLETE, item->GetState());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForInterruptedDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  RunAllPendingInMessageLoops();

  ASSERT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_TRUE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForCancelledDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  EXPECT_CALL(*download_file, Cancel());
  item->Cancel(true);
  RunAllPendingInMessageLoops();

  ASSERT_EQ(DownloadItem::CANCELLED, item->GetState());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());
}

// Test various aspects of the delegate completion blocker.

// Just allowing completion.
TEST_F(DownloadItemTest, CompleteDelegate_ReturnTrue) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  EXPECT_CALL(*download_file,
              RenameAndAnnotate(base::FilePath(kDummyTargetPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyTargetPath)));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Just delaying completion.
TEST_F(DownloadItemTest, CompleteDelegate_BlockOnce) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  base::Closure delegate_callback;
  base::Closure copy_delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(DoAll(SaveArg<1>(&delegate_callback),
                      Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  ASSERT_FALSE(delegate_callback.is_null());
  copy_delegate_callback = delegate_callback;
  delegate_callback.Reset();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  copy_delegate_callback.Run();
  ASSERT_TRUE(delegate_callback.is_null());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  EXPECT_CALL(*download_file,
              RenameAndAnnotate(base::FilePath(kDummyTargetPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyTargetPath)));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Delay and set danger.
TEST_F(DownloadItemTest, CompleteDelegate_SetDanger) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  base::Closure delegate_callback;
  base::Closure copy_delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(DoAll(SaveArg<1>(&delegate_callback),
                      Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  ASSERT_FALSE(delegate_callback.is_null());
  copy_delegate_callback = delegate_callback;
  delegate_callback.Reset();
  EXPECT_FALSE(item->IsDangerous());
  item->OnContentCheckCompleted(
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  copy_delegate_callback.Run();
  ASSERT_TRUE(delegate_callback.is_null());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(item->IsDangerous());

  // Make sure the download doesn't complete until we've validated it.
  EXPECT_CALL(*download_file,
              RenameAndAnnotate(base::FilePath(kDummyTargetPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyTargetPath)));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(item->IsDangerous());

  item->ValidateDangerousDownload();
  EXPECT_EQ(DOWNLOAD_DANGER_TYPE_USER_VALIDATED, item->GetDangerType());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Just delaying completion twice.
TEST_F(DownloadItemTest, CompleteDelegate_BlockTwice) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  base::Closure delegate_callback;
  base::Closure copy_delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(item, _))
      .WillOnce(DoAll(SaveArg<1>(&delegate_callback),
                      Return(false)))
      .WillOnce(DoAll(SaveArg<1>(&delegate_callback),
                      Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(std::string());
  ASSERT_FALSE(delegate_callback.is_null());
  copy_delegate_callback = delegate_callback;
  delegate_callback.Reset();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  copy_delegate_callback.Run();
  ASSERT_FALSE(delegate_callback.is_null());
  copy_delegate_callback = delegate_callback;
  delegate_callback.Reset();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  copy_delegate_callback.Run();
  ASSERT_TRUE(delegate_callback.is_null());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  EXPECT_CALL(*download_file,
              RenameAndAnnotate(base::FilePath(kDummyTargetPath), _))
      .WillOnce(ScheduleRenameCallback(DOWNLOAD_INTERRUPT_REASON_NONE,
                                       base::FilePath(kDummyTargetPath)));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  RunAllPendingInMessageLoops();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

TEST_F(DownloadItemTest, StealDangerousDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  ASSERT_TRUE(item->IsDangerous());
  base::FilePath full_path(FILE_PATH_LITERAL("foo.txt"));
  base::FilePath returned_path;

  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(full_path));
  EXPECT_CALL(*download_file, Detach());
  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  base::WeakPtrFactory<DownloadItemTest> weak_ptr_factory(this);
  item->StealDangerousDownload(
      base::Bind(&DownloadItemTest::OnDownloadFileAcquired,
                 weak_ptr_factory.GetWeakPtr(),
                 base::Unretained(&returned_path)));
  RunAllPendingInMessageLoops();
  EXPECT_EQ(full_path, returned_path);
}

TEST_F(DownloadItemTest, StealInterruptedDangerousDownload) {
  base::FilePath returned_path;
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  base::FilePath full_path = item->GetFullPath();
  EXPECT_FALSE(full_path.empty());
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(Return(full_path));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
  ASSERT_TRUE(item->IsDangerous());

  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  base::WeakPtrFactory<DownloadItemTest> weak_ptr_factory(this);
  item->StealDangerousDownload(
      base::Bind(&DownloadItemTest::OnDownloadFileAcquired,
                 weak_ptr_factory.GetWeakPtr(),
                 base::Unretained(&returned_path)));
  RunAllPendingInMessageLoops();
  EXPECT_EQ(full_path, returned_path);
}

TEST_F(DownloadItemTest, StealInterruptedNonResumableDangerousDownload) {
  base::FilePath returned_path;
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  ASSERT_TRUE(item->IsDangerous());

  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  base::WeakPtrFactory<DownloadItemTest> weak_ptr_factory(this);
  item->StealDangerousDownload(
      base::Bind(&DownloadItemTest::OnDownloadFileAcquired,
                 weak_ptr_factory.GetWeakPtr(),
                 base::Unretained(&returned_path)));
  RunAllPendingInMessageLoops();
  EXPECT_TRUE(returned_path.empty());
}

namespace {

// The DownloadItemDestinationUpdateRaceTest fixture (defined below) is used to
// test for race conditions between download destination events received via the
// DownloadDestinationObserver interface, and the target determination logic.
//
// The general control flow for DownloadItemImpl looks like this:
//
// * Start() called, which in turn calls DownloadFile::Initialize().
//
//   Even though OnDownloadFileInitialized hasn't been called, there could now
//   be destination observer calls queued prior to the task that calls
//   OnDownloadFileInitialized. Let's call this point in the workflow "A".
//
// * DownloadItemImpl::OnDownloadFileInitialized() called.
//
// * Assuming the result is successful, DII now invokes the delegate's
//   DetermineDownloadTarget method.
//
//   At this point DonwnloadFile acts as the source of
//   DownloadDestinationObserver events, and may invoke callbacks. Let's call
//   this point in the workflow "B".
//
// * DII::OnDownloadTargetDetermined() invoked after delegate is done with
//   target determination.
//
// * DII attempts to rename the DownloadFile to its intermediate name.
//
//   More DownloadDestinationObserver events can happen here. Let's call this
//   point in the workflow "C".
//
// * DII::OnDownloadRenamedToIntermediateName() invoked. Assuming all went well,
//   DII is now in IN_PROGRESS state.
//
//   More DownloadDestinationObserver events can happen here. Let's call this
//   point in the workflow "D".
//
// The DownloadItemDestinationUpdateRaceTest works by generating various
// combinations of DownloadDestinationObserver events that might occur at the
// points "A", "B", "C", and "D" above. Each test in this suite cranks a
// DownloadItemImpl through the states listed above and invokes the events
// assigned to each position.

// This type of callback represents a call to a DownloadDestinationObserver
// method that's missing the DownloadDestinationObserver object. Currying this
// way allows us to bind a call prior to constructing the object on which the
// method would be invoked.  This is necessary since we are going to construct
// various permutations of observer calls that will then be applied to a
// DownloadItem in a state as yet undetermined.
using CurriedObservation =
    base::Callback<void(base::WeakPtr<DownloadDestinationObserver>)>;

// A list of observations that are to be made during some event in the
// DownloadItemImpl control flow. Ordering of the observations is significant.
using ObservationList = std::deque<CurriedObservation>;

// An ordered list of events.
//
// An "event" in this context refers to some stage in the DownloadItemImpl's
// workflow described as "A", "B", "C", or "D" above. An EventList is expected
// to always contains kEventCount events.
using EventList = std::deque<ObservationList>;

// Number of events in an EventList. This is always 4 for now as described
// above.
const int kEventCount = 4;

// The following functions help us with currying the calls to
// DownloadDestinationObserver. If std::bind was allowed along with
// std::placeholders, it is possible to avoid these functions, but currently
// Chromium doesn't allow using std::bind for good reasons.
void DestinationUpdateInvoker(
    int64_t bytes_so_far,
    int64_t bytes_per_sec,
    const std::string& hash_state,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  DVLOG(20) << "DestinationUpdate(bytes_so_far:" << bytes_so_far
            << ", bytes_per_sec:" << bytes_per_sec
            << ", hash_state:" << hash_state << ") observer:" << !!observer;
  if (observer)
    observer->DestinationUpdate(bytes_so_far, bytes_per_sec, hash_state);
}

void DestinationErrorInvoker(
    DownloadInterruptReason reason,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  DVLOG(20) << "DestinationError(reason:"
            << DownloadInterruptReasonToString(reason)
            << ") observer:" << !!observer;
  if (observer)
    observer->DestinationError(reason);
}

void DestinationCompletedInvoker(
    const std::string& final_hash,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  DVLOG(20) << "DestinationComplete(final_hash:" << final_hash
            << ") observer:" << !!observer;
  if (observer)
    observer->DestinationCompleted(final_hash);
}

// Given a set of observations (via the range |begin|..|end|), constructs a list
// of EventLists such that:
//
// * There are exactly |event_count| ObservationSets in each EventList.
//
// * Each ObservationList in each EventList contains a subrange (possibly empty)
//   of observations from the input range, in the same order as the input range.
//
// * The ordering of the ObservationList in each EventList is such that all
//   observations in one ObservationList occur earlier than all observations in
//   an ObservationList that follows it.
//
// * The list of EventLists together describe all the possible ways in which the
//   list of observations can be distributed into |event_count| events.
std::vector<EventList> DistributeObservationsIntoEvents(
    const std::vector<CurriedObservation>::iterator begin,
    const std::vector<CurriedObservation>::iterator end,
    int event_count) {
  std::vector<EventList> all_event_lists;
  for (auto partition = begin;; ++partition) {
    ObservationList first_group_of_observations(begin, partition);
    if (event_count > 1) {
      std::vector<EventList> list_of_subsequent_events =
          DistributeObservationsIntoEvents(partition, end, event_count - 1);
      for (const auto& subsequent_events : list_of_subsequent_events) {
        EventList event_list;
        event_list = subsequent_events;
        event_list.push_front(first_group_of_observations);
        all_event_lists.push_back(event_list);
      }
    } else {
      EventList event_list;
      event_list.push_front(first_group_of_observations);
      all_event_lists.push_back(event_list);
    }
    if (partition == end)
      break;
  }
  return all_event_lists;
}

// For the purpose of this tests, we are only concerned with 3 events:
//
// 1. Immediately after the DownloadFile is initialized.
// 2. Immediately after the DownloadTargetCallback is invoked.
// 3. Immediately after the intermediate file is renamed.
//
// We are going to take a couple of sets of DownloadDestinationObserver events
// and distribute them into the three events described above. And then we are
// going to invoke the observations while a DownloadItemImpl is carefully
// stepped through its stages.

std::vector<EventList> GenerateSuccessfulEventLists() {
  std::vector<CurriedObservation> all_observations;
  all_observations.push_back(
      base::Bind(&DestinationUpdateInvoker, 100, 100, "abc"));
  all_observations.push_back(
      base::Bind(&DestinationUpdateInvoker, 200, 100, "def"));
  all_observations.push_back(
      base::Bind(&DestinationCompletedInvoker, "final-hash-1"));
  return DistributeObservationsIntoEvents(all_observations.begin(),
                                          all_observations.end(), kEventCount);
}

std::vector<EventList> GenerateFailingEventLists() {
  std::vector<CurriedObservation> all_observations;
  all_observations.push_back(
      base::Bind(&DestinationUpdateInvoker, 100, 100, "abc"));
  all_observations.push_back(base::Bind(
      &DestinationErrorInvoker, DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
  return DistributeObservationsIntoEvents(all_observations.begin(),
                                          all_observations.end(), kEventCount);
}

class DownloadItemDestinationUpdateRaceTest
    : public DownloadItemTest,
      public ::testing::WithParamInterface<EventList> {
 public:
  DownloadItemDestinationUpdateRaceTest()
      : DownloadItemTest(),
        item_(CreateDownloadItem()),
        file_(new ::testing::StrictMock<MockDownloadFile>()),
        request_handle_(new ::testing::StrictMock<MockRequestHandle>()) {
    DCHECK_EQ(GetParam().size(), static_cast<unsigned>(kEventCount));
    EXPECT_CALL(*request_handle_, GetWebContents())
        .WillRepeatedly(Return(nullptr));
  }

 protected:
  const ObservationList& PreInitializeFileObservations() {
    return GetParam().front();
  }
  const ObservationList& PostInitializeFileObservations() {
    return *(GetParam().begin() + 1);
  }
  const ObservationList& PostTargetDeterminationObservations() {
    return *(GetParam().begin() + 2);
  }
  const ObservationList& PostIntermediateRenameObservations() {
    return *(GetParam().begin() + 3);
  }

  // Apply all the observations in |observations| to |observer|, but do so
  // asynchronously so that the events are applied in order behind any tasks
  // that are already scheduled.
  void ScheduleObservations(
      const ObservationList& observations,
      base::WeakPtr<DownloadDestinationObserver> observer) {
    for (const auto action : observations)
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(action, observer));
  }

  DownloadItemImpl* item_;
  scoped_ptr<MockDownloadFile> file_;
  scoped_ptr<MockRequestHandle> request_handle_;

  std::queue<base::Closure> successful_update_events_;
  std::queue<base::Closure> failing_update_events_;
};

INSTANTIATE_TEST_CASE_P(Success,
                        DownloadItemDestinationUpdateRaceTest,
                        ::testing::ValuesIn(GenerateSuccessfulEventLists()));

INSTANTIATE_TEST_CASE_P(Failure,
                        DownloadItemDestinationUpdateRaceTest,
                        ::testing::ValuesIn(GenerateFailingEventLists()));

}  // namespace

// Run through the DII workflow but the embedder cancels the download at target
// determination.
TEST_P(DownloadItemDestinationUpdateRaceTest, DownloadCancelledByUser) {
  // Expect that the download file and the request will be cancelled as a
  // result.
  EXPECT_CALL(*file_, Cancel());
  EXPECT_CALL(*request_handle_, CancelRequest());

  base::RunLoop download_start_loop;
  DownloadFile::InitializeCallback initialize_callback;
  EXPECT_CALL(*file_, Initialize(_))
      .WillOnce(DoAll(SaveArg<0>(&initialize_callback),
                      ScheduleClosure(download_start_loop.QuitClosure())));
  item_->Start(std::move(file_), std::move(request_handle_));
  download_start_loop.Run();

  base::WeakPtr<DownloadDestinationObserver> destination_observer =
      item_->DestinationObserverAsWeakPtr();

  ScheduleObservations(PreInitializeFileObservations(), destination_observer);
  RunAllPendingInMessageLoops();

  base::RunLoop initialize_completion_loop;
  DownloadItemImplDelegate::DownloadTargetCallback target_callback;
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(_, _))
      .WillOnce(
          DoAll(SaveArg<1>(&target_callback),
                ScheduleClosure(initialize_completion_loop.QuitClosure())));
  ScheduleObservations(PostInitializeFileObservations(), destination_observer);
  initialize_callback.Run(DOWNLOAD_INTERRUPT_REASON_NONE);
  initialize_completion_loop.Run();

  RunAllPendingInMessageLoops();

  ASSERT_FALSE(target_callback.is_null());
  ScheduleObservations(PostTargetDeterminationObservations(),
                       destination_observer);
  target_callback.Run(base::FilePath(),
                      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, base::FilePath());
  EXPECT_EQ(DownloadItem::CANCELLED, item_->GetState());
  RunAllPendingInMessageLoops();
}

// Run through the DII workflow, but the intermediate rename fails.
TEST_P(DownloadItemDestinationUpdateRaceTest, IntermediateRenameFails) {
  // Expect that the download file and the request will be cancelled as a
  // result.
  EXPECT_CALL(*file_, Cancel());
  EXPECT_CALL(*request_handle_, CancelRequest());

  // Intermediate rename loop is not used immediately, but let's set up the
  // DownloadFile expectations since we are about to transfer its ownership to
  // the DownloadItem.
  base::RunLoop intermediate_rename_loop;
  DownloadFile::RenameCompletionCallback intermediate_rename_callback;
  EXPECT_CALL(*file_, RenameAndUniquify(_, _))
      .WillOnce(DoAll(SaveArg<1>(&intermediate_rename_callback),
                      ScheduleClosure(intermediate_rename_loop.QuitClosure())));

  base::RunLoop download_start_loop;
  DownloadFile::InitializeCallback initialize_callback;
  EXPECT_CALL(*file_, Initialize(_))
      .WillOnce(DoAll(SaveArg<0>(&initialize_callback),
                      ScheduleClosure(download_start_loop.QuitClosure())));

  item_->Start(std::move(file_), std::move(request_handle_));
  download_start_loop.Run();
  base::WeakPtr<DownloadDestinationObserver> destination_observer =
      item_->DestinationObserverAsWeakPtr();

  ScheduleObservations(PreInitializeFileObservations(), destination_observer);
  RunAllPendingInMessageLoops();

  base::RunLoop initialize_completion_loop;
  DownloadItemImplDelegate::DownloadTargetCallback target_callback;
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(_, _))
      .WillOnce(
          DoAll(SaveArg<1>(&target_callback),
                ScheduleClosure(initialize_completion_loop.QuitClosure())));
  ScheduleObservations(PostInitializeFileObservations(), destination_observer);
  initialize_callback.Run(DOWNLOAD_INTERRUPT_REASON_NONE);
  initialize_completion_loop.Run();

  RunAllPendingInMessageLoops();
  ASSERT_FALSE(target_callback.is_null());

  ScheduleObservations(PostTargetDeterminationObservations(),
                       destination_observer);
  target_callback.Run(base::FilePath(kDummyTargetPath),
                      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                      base::FilePath(kDummyIntermediatePath));

  intermediate_rename_loop.Run();
  ASSERT_FALSE(intermediate_rename_callback.is_null());

  ScheduleObservations(PostIntermediateRenameObservations(),
                       destination_observer);
  intermediate_rename_callback.Run(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
                                   base::FilePath());
  RunAllPendingInMessageLoops();

  EXPECT_EQ(DownloadItem::INTERRUPTED, item_->GetState());
}

// Run through the DII workflow. Download file initialization, target
// determination and intermediate rename all succeed.
TEST_P(DownloadItemDestinationUpdateRaceTest, IntermediateRenameSucceeds) {
  // We expect either that the download will fail (in which case the request and
  // the download file will be cancelled), or it will succeed (in which case the
  // DownloadFile will Detach()). It depends on the list of observations that
  // are given to us.
  EXPECT_CALL(*file_, Cancel()).Times(::testing::AnyNumber());
  EXPECT_CALL(*request_handle_, CancelRequest()).Times(::testing::AnyNumber());
  EXPECT_CALL(*file_, Detach()).Times(::testing::AnyNumber());

  EXPECT_CALL(*file_, FullPath())
      .WillRepeatedly(Return(base::FilePath(kDummyIntermediatePath)));

  // Intermediate rename loop is not used immediately, but let's set up the
  // DownloadFile expectations since we are about to transfer its ownership to
  // the DownloadItem.
  base::RunLoop intermediate_rename_loop;
  DownloadFile::RenameCompletionCallback intermediate_rename_callback;
  EXPECT_CALL(*file_, RenameAndUniquify(_, _))
      .WillOnce(DoAll(SaveArg<1>(&intermediate_rename_callback),
                      ScheduleClosure(intermediate_rename_loop.QuitClosure())));

  base::RunLoop download_start_loop;
  DownloadFile::InitializeCallback initialize_callback;
  EXPECT_CALL(*file_, Initialize(_))
      .WillOnce(DoAll(SaveArg<0>(&initialize_callback),
                      ScheduleClosure(download_start_loop.QuitClosure())));

  item_->Start(std::move(file_), std::move(request_handle_));
  download_start_loop.Run();
  base::WeakPtr<DownloadDestinationObserver> destination_observer =
      item_->DestinationObserverAsWeakPtr();

  ScheduleObservations(PreInitializeFileObservations(), destination_observer);
  RunAllPendingInMessageLoops();

  base::RunLoop initialize_completion_loop;
  DownloadItemImplDelegate::DownloadTargetCallback target_callback;
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget(_, _))
      .WillOnce(
          DoAll(SaveArg<1>(&target_callback),
                ScheduleClosure(initialize_completion_loop.QuitClosure())));
  ScheduleObservations(PostInitializeFileObservations(), destination_observer);
  initialize_callback.Run(DOWNLOAD_INTERRUPT_REASON_NONE);
  initialize_completion_loop.Run();

  RunAllPendingInMessageLoops();
  ASSERT_FALSE(target_callback.is_null());

  ScheduleObservations(PostTargetDeterminationObservations(),
                       destination_observer);
  target_callback.Run(base::FilePath(kDummyTargetPath),
                      DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                      DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                      base::FilePath(kDummyIntermediatePath));

  intermediate_rename_loop.Run();
  ASSERT_FALSE(intermediate_rename_callback.is_null());

  // This may or may not be called, depending on whether there are any errors in
  // our action list.
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload(_, _))
      .Times(::testing::AnyNumber());

  ScheduleObservations(PostIntermediateRenameObservations(),
                       destination_observer);
  intermediate_rename_callback.Run(DOWNLOAD_INTERRUPT_REASON_NONE,
                                   base::FilePath(kDummyIntermediatePath));
  RunAllPendingInMessageLoops();

  // The state of the download depends on the observer events that were played
  // back to the DownloadItemImpl. Hence we can't establish a single expectation
  // here. On Debug builds, the DCHECKs will verify that the state transitions
  // were correct. On Release builds, tests are expected to run to completion
  // without crashing on success.
  EXPECT_TRUE(item_->GetState() == DownloadItem::IN_PROGRESS ||
              item_->GetState() == DownloadItem::INTERRUPTED);
  if (item_->GetState() == DownloadItem::INTERRUPTED)
    EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, item_->GetLastReason());

  item_->Cancel(true);
  RunAllPendingInMessageLoops();
}

TEST(MockDownloadItem, Compiles) {
  MockDownloadItem mock_item;
}

}  // namespace content
