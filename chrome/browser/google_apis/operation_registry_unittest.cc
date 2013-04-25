// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/operation_registry.h"

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

class MockOperation : public OperationRegistry::Operation,
                      public base::SupportsWeakPtr<MockOperation> {
 public:
  explicit MockOperation(OperationRegistry* registry)
      : OperationRegistry::Operation(
            registry,
            base::FilePath(FILE_PATH_LITERAL("/dummy/download"))) {
  }

  MOCK_METHOD0(DoCancel, void());

  // Make them public so that they can be called from test cases.
  using OperationRegistry::Operation::NotifyStart;
  using OperationRegistry::Operation::NotifyFinish;
  using OperationRegistry::Operation::NotifySuspend;
  using OperationRegistry::Operation::NotifyResume;
};

}  // namespace

class OperationRegistryTest : public testing::Test {
 protected:
  OperationRegistryTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(OperationRegistryTest, OneSuccess) {
  OperationRegistry registry;

  base::WeakPtr<MockOperation> op1 =
      (new MockOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);

  op1->NotifyStart();
  op1->NotifyFinish(OPERATION_COMPLETED);
  EXPECT_EQ(NULL, op1.get());  // deleted
}

TEST_F(OperationRegistryTest, OneCancel) {
  OperationRegistry registry;

  base::WeakPtr<MockOperation> op1 =
      (new MockOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel());

  op1->NotifyStart();
  registry.CancelAll();
  EXPECT_EQ(NULL, op1.get());  // deleted
}

TEST_F(OperationRegistryTest, TwoSuccess) {
  OperationRegistry registry;

  base::WeakPtr<MockOperation> op1 =
      (new MockOperation(&registry))->AsWeakPtr();
  base::WeakPtr<MockOperation> op2 =
      (new MockOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);
  EXPECT_CALL(*op2, DoCancel()).Times(0);

  op1->NotifyStart();
  op2->NotifyStart();
  op1->NotifyFinish(OPERATION_COMPLETED);
  op2->NotifyFinish(OPERATION_COMPLETED);
  EXPECT_EQ(NULL, op1.get());  // deleted
  EXPECT_EQ(NULL, op2.get());  // deleted
}

TEST_F(OperationRegistryTest, RestartOperation) {
  OperationRegistry registry;

  base::WeakPtr<MockOperation> op1 =
      (new MockOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);

  op1->NotifyStart();
  op1->NotifyStart();  // restart
  op1->NotifyFinish(OPERATION_COMPLETED);
  EXPECT_EQ(NULL, op1.get());  // deleted
}

TEST_F(OperationRegistryTest, SuspendCancel) {
  OperationRegistry registry;

  // Suspend-then-resume is a hack in OperationRegistry to tie physically
  // split but logically single operation (= chunked uploading split into
  // multiple HTTP requests). When the "logically-single" operation is
  // canceled between the two physical operations,
  //    |----op1----| CANCEL! |----op2----|
  // the cancellation is notified to the callback function associated with
  // op2, not op1. This is because, op1's callback is already invoked at this
  // point to notify the completion of the physical operation. Completion
  // callback must not be called more than once.

  base::WeakPtr<MockOperation> op1 =
      (new MockOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);

  op1->NotifyStart();
  op1->NotifySuspend();
  registry.CancelAll();
  EXPECT_EQ(NULL, op1.get());  // deleted

  base::WeakPtr<MockOperation> op2 =
      (new MockOperation(&registry))->AsWeakPtr();
  EXPECT_CALL(*op2, DoCancel()).Times(1);

  op2->NotifyResume();
  EXPECT_EQ(NULL, op2.get());  // deleted
}

}  // namespace google_apis
