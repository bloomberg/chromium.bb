// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/request_registry.h"

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/request_registry.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

class MockRequest : public RequestRegistry::Request,
                    public base::SupportsWeakPtr<MockRequest> {
 public:
  explicit MockRequest(RequestRegistry* registry)
      : RequestRegistry::Request(
          registry,
          base::FilePath(FILE_PATH_LITERAL("/dummy/download"))) {
  }

  MOCK_METHOD0(DoCancel, void());

  // Make them public so that they can be called from test cases.
  using RequestRegistry::Request::NotifyStart;
  using RequestRegistry::Request::NotifyFinish;
  using RequestRegistry::Request::NotifySuspend;
  using RequestRegistry::Request::NotifyResume;
};

}  // namespace

class RequestRegistryTest : public testing::Test {
 protected:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(RequestRegistryTest, OneSuccess) {
  RequestRegistry registry;

  base::WeakPtr<MockRequest> op1 =
      (new MockRequest(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);

  op1->NotifyStart();
  op1->NotifyFinish(REQUEST_COMPLETED);
  EXPECT_EQ(NULL, op1.get());  // deleted
}

TEST_F(RequestRegistryTest, OneCancel) {
  RequestRegistry registry;

  base::WeakPtr<MockRequest> op1 =
      (new MockRequest(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel());

  op1->NotifyStart();
  registry.CancelAll();
  EXPECT_EQ(NULL, op1.get());  // deleted
}

TEST_F(RequestRegistryTest, TwoSuccess) {
  RequestRegistry registry;

  base::WeakPtr<MockRequest> op1 =
      (new MockRequest(&registry))->AsWeakPtr();
  base::WeakPtr<MockRequest> op2 =
      (new MockRequest(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);
  EXPECT_CALL(*op2, DoCancel()).Times(0);

  op1->NotifyStart();
  op2->NotifyStart();
  op1->NotifyFinish(REQUEST_COMPLETED);
  op2->NotifyFinish(REQUEST_COMPLETED);
  EXPECT_EQ(NULL, op1.get());  // deleted
  EXPECT_EQ(NULL, op2.get());  // deleted
}

TEST_F(RequestRegistryTest, RestartRequest) {
  RequestRegistry registry;

  base::WeakPtr<MockRequest> op1 =
      (new MockRequest(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);

  op1->NotifyStart();
  op1->NotifyStart();  // restart
  op1->NotifyFinish(REQUEST_COMPLETED);
  EXPECT_EQ(NULL, op1.get());  // deleted
}

TEST_F(RequestRegistryTest, SuspendCancel) {
  RequestRegistry registry;

  // Suspend-then-resume is a hack in RequestRegistry to tie physically
  // split but logically single request (= chunked uploading split into
  // multiple HTTP requests). When the "logically-single" request is
  // canceled between the two physical requests,
  //    |----op1----| CANCEL! |----op2----|
  // the cancellation is notified to the callback function associated with
  // op2, not op1. This is because, op1's callback is already invoked at this
  // point to notify the completion of the physical request. Completion
  // callback must not be called more than once.

  base::WeakPtr<MockRequest> op1 =
      (new MockRequest(&registry))->AsWeakPtr();
  EXPECT_CALL(*op1, DoCancel()).Times(0);

  op1->NotifyStart();
  op1->NotifySuspend();
  registry.CancelAll();
  EXPECT_EQ(NULL, op1.get());  // deleted

  base::WeakPtr<MockRequest> op2 =
      (new MockRequest(&registry))->AsWeakPtr();
  EXPECT_CALL(*op2, DoCancel()).Times(1);

  op2->NotifyResume();
  EXPECT_EQ(NULL, op2.get());  // deleted
}

}  // namespace google_apis
