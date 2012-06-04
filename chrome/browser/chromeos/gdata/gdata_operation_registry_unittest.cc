// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

namespace {

class MockOperation : public GDataOperationRegistry::Operation,
                      public base::SupportsWeakPtr<MockOperation> {
 public:
  MockOperation(GDataOperationRegistry* registry,
                GDataOperationRegistry::OperationType type,
                const FilePath& path,
                bool expect_cancel)
      : GDataOperationRegistry::Operation(registry, type, path),
        expect_cancel_(expect_cancel),
        cancel_called_(false) {}

  ~MockOperation() {
    EXPECT_EQ(expect_cancel_, cancel_called_);
  }

  virtual void DoCancel() OVERRIDE {
    cancel_called_ = true;
  }

  // Make them public so that they can be called from test cases.
  using GDataOperationRegistry::Operation::NotifyStart;
  using GDataOperationRegistry::Operation::NotifyProgress;
  using GDataOperationRegistry::Operation::NotifyFinish;

 private:
  bool expect_cancel_;
  bool cancel_called_;
};

class MockUploadOperation : public MockOperation {
 public:
  MockUploadOperation(GDataOperationRegistry* registry,
                      bool expect_cancel)
      : MockOperation(registry,
                      GDataOperationRegistry::OPERATION_UPLOAD,
                      FilePath("/dummy/upload"),
                      expect_cancel) {}
};

class MockDownloadOperation : public MockOperation {
 public:
  MockDownloadOperation(GDataOperationRegistry* registry,
                        bool expect_cancel)
      : MockOperation(registry,
                      GDataOperationRegistry::OPERATION_DOWNLOAD,
                      FilePath("/dummy/download"),
                      expect_cancel) {}
};

class MockOtherOperation : public MockOperation {
 public:
  MockOtherOperation(GDataOperationRegistry* registry,
                     bool expect_cancel)
      : MockOperation(registry,
                      GDataOperationRegistry::OPERATION_OTHER,
                      FilePath("/dummy/other"),
                      expect_cancel) {}
};

class TestObserver : public GDataOperationRegistry::Observer {
 public:
  virtual void OnProgressUpdate(
      const std::vector<GDataOperationRegistry::ProgressStatus>& list)
      OVERRIDE {
    status_ = list;
  }

  const std::vector<GDataOperationRegistry::ProgressStatus>& status() const {
    return status_;
  }

 private:
  std::vector<GDataOperationRegistry::ProgressStatus> status_;
};

}  // namespace

class GDataOperationRegistryTest : public testing::Test {
 protected:
  GDataOperationRegistryTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(GDataOperationRegistryTest, OneSuccess) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry, false /* no cancel */))->AsWeakPtr();
  EXPECT_EQ(0U, observer.status().size());
  op1->NotifyStart();
  EXPECT_EQ(1U, observer.status().size());
  EXPECT_EQ(-1LL, observer.status()[0].progress_total);
  op1->NotifyProgress(0, 100);
  EXPECT_EQ(1U, observer.status().size());
  EXPECT_EQ(0LL, observer.status()[0].progress_current);
  EXPECT_EQ(100LL, observer.status()[0].progress_total);
  op1->NotifyProgress(100, 100);
  EXPECT_EQ(1U, observer.status().size());
  EXPECT_EQ(100LL, observer.status()[0].progress_current);
  EXPECT_EQ(100LL, observer.status()[0].progress_total);
  op1->NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  EXPECT_EQ(1U, observer.status().size()); // contains "COMPLETED" notification
  EXPECT_EQ(0U, registry.GetProgressStatusList().size()); // then it is removed
  EXPECT_EQ(NULL, op1.get()); // deleted
}

TEST_F(GDataOperationRegistryTest, OneCancel) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry, true /* cancel */))->AsWeakPtr();
  EXPECT_EQ(0U, observer.status().size());
  op1->NotifyStart();
  EXPECT_EQ(1U, observer.status().size());
  EXPECT_EQ(-1LL, observer.status()[0].progress_total);
  op1->NotifyProgress(0, 100);
  EXPECT_EQ(1U, observer.status().size());
  EXPECT_EQ(0LL, observer.status()[0].progress_current);
  EXPECT_EQ(100LL, observer.status()[0].progress_total);
  registry.CancelAll();
  EXPECT_EQ(1U, observer.status().size());
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
}

TEST_F(GDataOperationRegistryTest, TwoSuccess) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry, false /* no cancel */))->AsWeakPtr();
  base::WeakPtr<MockOperation> op2 =
      (new MockDownloadOperation(&registry,
                                 false /* no cancel */))->AsWeakPtr();
  EXPECT_EQ(0U, observer.status().size());
  op1->NotifyStart();
  op1->NotifyProgress(0, 100);
  EXPECT_EQ(1U, observer.status().size());
  op2->NotifyStart();
  op2->NotifyProgress(0, 200);
  EXPECT_EQ(2U, observer.status().size());
  EXPECT_EQ(100LL, observer.status()[0].progress_total);
  EXPECT_EQ(200LL, observer.status()[1].progress_total);
  op1->NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  EXPECT_EQ(2U, observer.status().size());
  EXPECT_EQ(1U, registry.GetProgressStatusList().size());
  EXPECT_EQ(200LL, observer.status()[1].progress_total);
  op2->NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  EXPECT_EQ(1U, observer.status().size());
  EXPECT_EQ(200LL, observer.status()[0].progress_total);
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
  EXPECT_EQ(NULL, op2.get()); // deleted
}

TEST_F(GDataOperationRegistryTest, ThreeCancel) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry, true /* cancel */))->AsWeakPtr();
  base::WeakPtr<MockOperation> op2 =
      (new MockDownloadOperation(&registry,
                                 true /* cancel */))->AsWeakPtr();
  base::WeakPtr<MockOperation> op3 =
      (new MockOtherOperation(&registry,
                              true /* cancel */))->AsWeakPtr();
  EXPECT_EQ(0U, observer.status().size());
  op1->NotifyStart();
  EXPECT_EQ(1U, observer.status().size());
  op2->NotifyStart();
  EXPECT_EQ(2U, observer.status().size());
  op3->NotifyStart();
  EXPECT_EQ(2U, observer.status().size()); // only upload/download is reported.
  registry.CancelAll();
  EXPECT_EQ(1U, observer.status().size()); // holds the last one "COMPLETED"
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
  EXPECT_EQ(NULL, op2.get()); // deleted
  EXPECT_EQ(NULL, op3.get()); // deleted. CancelAll cares all operations.
}

TEST_F(GDataOperationRegistryTest, RestartOperation) {
  TestObserver observer;
  GDataOperationRegistry registry;
  registry.AddObserver(&observer);

  base::WeakPtr<MockOperation> op1 =
      (new MockUploadOperation(&registry, false /* cancel */))->AsWeakPtr();
  op1->NotifyStart();
  EXPECT_EQ(1U, registry.GetProgressStatusList().size());
  op1->NotifyStart(); // restart
  EXPECT_EQ(1U, registry.GetProgressStatusList().size());
  op1->NotifyProgress(0, 200);
  op1->NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  EXPECT_EQ(0U, registry.GetProgressStatusList().size());
  EXPECT_EQ(NULL, op1.get()); // deleted
}

}  // namespace gdata
