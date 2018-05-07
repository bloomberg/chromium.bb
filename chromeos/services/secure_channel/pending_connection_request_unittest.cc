// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_connection_request.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/secure_channel/fake_connection_delegate.h"
#include "chromeos/services/secure_channel/fake_pending_connection_request_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

const char kTestReadableRequestTypeForLogging[] = "Test Request Type";
const char kTestFeature[] = "testFeature";

enum class TestFailureDetail {
  kReasonWhichCausesRequestToBecomeInactive,
  kReasonWhichDoesNotCauseRequestToBecomeInactive
};

// Since PendingConnectionRequest is templatized, a concrete implementation is
// needed for its test.
class TestPendingConnectionRequest
    : public PendingConnectionRequest<TestFailureDetail> {
 public:
  TestPendingConnectionRequest(
      const std::string& feature,
      mojom::ConnectionDelegatePtr connection_delegate_ptr,
      PendingConnectionRequestDelegate* delegate)
      : PendingConnectionRequest<TestFailureDetail>(
            feature,
            kTestReadableRequestTypeForLogging,
            delegate,
            std::move(connection_delegate_ptr)) {}
  ~TestPendingConnectionRequest() override = default;

  // PendingConnectionRequest<TestFailureDetail>:
  void HandleConnectionFailure(TestFailureDetail failure_detail) override {
    switch (failure_detail) {
      case TestFailureDetail::kReasonWhichCausesRequestToBecomeInactive:
        StopRequestDueToConnectionFailures();
        break;
      case TestFailureDetail::kReasonWhichDoesNotCauseRequestToBecomeInactive:
        // Do nothing.
        break;
    }
  }
};

}  // namespace

class SecureChannelPendingConnectionRequestTest : public testing::Test {
 protected:
  SecureChannelPendingConnectionRequestTest() = default;
  ~SecureChannelPendingConnectionRequestTest() override = default;

  void SetUp() override {
    fake_connection_delegate_ = std::make_unique<FakeConnectionDelegate>();
    fake_pending_connection_request_delegate_ =
        std::make_unique<FakePendingConnectionRequestDelegate>();
    test_pending_connection_request_ =
        std::make_unique<TestPendingConnectionRequest>(
            kTestFeature, fake_connection_delegate_->GenerateInterfacePtr(),
            fake_pending_connection_request_delegate_.get());
  }

  TestPendingConnectionRequest* test_pending_connection_request() {
    return test_pending_connection_request_.get();
  }

  const base::Optional<
      PendingConnectionRequestDelegate::FailedConnectionReason>&
  GetFailedConnectionReason() {
    return fake_pending_connection_request_delegate_
        ->GetFailedConnectionReasonForId(
            test_pending_connection_request_->GetRequestId());
  }

  void DisconnectConnectionDelegatePtr() {
    base::RunLoop run_loop;
    fake_pending_connection_request_delegate_
        ->set_closure_for_next_delegate_callback(run_loop.QuitClosure());
    fake_connection_delegate_->DisconnectGeneratedPtrs();
    run_loop.Run();
  }

 private:
  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<FakeConnectionDelegate> fake_connection_delegate_;
  std::unique_ptr<FakePendingConnectionRequestDelegate>
      fake_pending_connection_request_delegate_;

  std::unique_ptr<TestPendingConnectionRequest>
      test_pending_connection_request_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelPendingConnectionRequestTest);
};

TEST_F(SecureChannelPendingConnectionRequestTest,
       HandleConnectionFailureWhichCausesRequestToBecomeInactive) {
  test_pending_connection_request()->HandleConnectionFailure(
      TestFailureDetail::kReasonWhichCausesRequestToBecomeInactive);
  EXPECT_FALSE(test_pending_connection_request()->is_active());
  EXPECT_EQ(
      PendingConnectionRequestDelegate::FailedConnectionReason::kRequestFailed,
      *GetFailedConnectionReason());
}

TEST_F(SecureChannelPendingConnectionRequestTest,
       HandleConnectionFailureWhichDoesNotCauseRequestToBecomeInactive) {
  // Repeat 5 connection failures, none of which should cause the request to
  // become inactive.
  for (int i = 0; i < 5; ++i) {
    test_pending_connection_request()->HandleConnectionFailure(
        TestFailureDetail::kReasonWhichDoesNotCauseRequestToBecomeInactive);
    EXPECT_TRUE(test_pending_connection_request()->is_active());
    EXPECT_FALSE(GetFailedConnectionReason());
  }
}

TEST_F(SecureChannelPendingConnectionRequestTest,
       ConnectionDelegateInvalidated) {
  DisconnectConnectionDelegatePtr();
  EXPECT_FALSE(test_pending_connection_request()->is_active());
  EXPECT_EQ(PendingConnectionRequestDelegate::FailedConnectionReason::
                kRequestCanceledByClient,
            *GetFailedConnectionReason());
}

}  // namespace secure_channel

}  // namespace chromeos
