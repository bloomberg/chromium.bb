// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;

namespace policy {

namespace {

constexpr char kClientId1[] = "fake-client-id-1";
constexpr char kClientId2[] = "fake-client-id-2";
constexpr char kEnrollmentToken1[] = "fake-enrollment-token-1";
constexpr char kEnrollmentToken2[] = "fake-enrollment-token-2";
constexpr char kDMToken1[] = "fake-dm-token-1";
constexpr char kDMToken2[] = "fake-dm-token-2";

}  // namespace

class MockBrowserDMTokenStorage : public BrowserDMTokenStorage {
 public:
  MockBrowserDMTokenStorage() {
    set_test_client_id(kClientId1);
    set_test_enrollment_token(kEnrollmentToken1);
    set_test_dm_token(kDMToken1);
  }

  // BrowserDMTokenStorage override
  std::string InitClientId() override { return test_client_id_; }
  std::string InitEnrollmentToken() override { return test_enrollment_token_; }
  std::string InitDMToken() override { return test_dm_token_; }

  void set_test_client_id(std::string test_client_id) {
    test_client_id_ = test_client_id;
  }
  void set_test_enrollment_token(std::string test_enrollment_token) {
    test_enrollment_token_ = test_enrollment_token;
  }
  void set_test_dm_token(std::string test_dm_token) {
    test_dm_token_ = test_dm_token;
  }

 private:
  std::string test_client_id_;
  std::string test_enrollment_token_;
  std::string test_dm_token_;
};

class BrowserDMTokenStorageTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(BrowserDMTokenStorageTest, RetrieveClientId) {
  MockBrowserDMTokenStorage storage;
  EXPECT_EQ(kClientId1, storage.RetrieveClientId());

  // The client ID value should be cached in memory and not read from the system
  // again.
  storage.set_test_client_id(kClientId2);
  EXPECT_EQ(kClientId1, storage.RetrieveClientId());
}

TEST_F(BrowserDMTokenStorageTest, RetrieveEnrollmentToken) {
  MockBrowserDMTokenStorage storage;
  EXPECT_EQ(kEnrollmentToken1, storage.RetrieveEnrollmentToken());

  // The enrollment token should be cached in memory and not read from the
  // system again.
  storage.set_test_enrollment_token(kEnrollmentToken2);
  EXPECT_EQ(kEnrollmentToken1, storage.RetrieveEnrollmentToken());
}

TEST_F(BrowserDMTokenStorageTest, RetrieveDMToken) {
  MockBrowserDMTokenStorage storage;
  EXPECT_EQ(kDMToken1, storage.RetrieveDMToken());

  // The DM token should be cached in memory and not read from the system again.
  storage.set_test_dm_token(kDMToken2);
  EXPECT_EQ(kDMToken1, storage.RetrieveDMToken());
}

class TestStoreDMTokenDelegate {
 public:
  TestStoreDMTokenDelegate() : called_(false), success_(true) {}
  ~TestStoreDMTokenDelegate() {}

  void OnDMTokenStored(bool success) {
    run_loop_.Quit();
    called_ = true;
    success_ = success;
  }

  bool WasCalled() {
    bool was_called = called_;
    called_ = false;
    return was_called;
  }

  bool success() { return success_; }

  void Wait() { run_loop_.Run(); }

 private:
  bool called_;
  bool success_;
  base::RunLoop run_loop_;
};

TEST_F(BrowserDMTokenStorageTest, StoreDMToken) {
  MockBrowserDMTokenStorage storage;
  TestStoreDMTokenDelegate delegate;

  storage.StoreDMToken(
      kDMToken1, base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenStored,
                                base::Unretained(&delegate)));

  delegate.Wait();

  EXPECT_TRUE(delegate.WasCalled());
  EXPECT_FALSE(delegate.success());
}

}  // namespace policy
