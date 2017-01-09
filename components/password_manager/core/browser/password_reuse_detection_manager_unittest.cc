// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detection_manager.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using testing::AnyNumber;
using testing::_;

namespace password_manager {

namespace {

constexpr size_t kMaxNumberOfCharactersToStore = 30;

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(GetPasswordStore, PasswordStore*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

class PasswordReuseDetectionManagerTest : public ::testing::Test {
 public:
  PasswordReuseDetectionManagerTest() {}
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStore>;
    CHECK(store_->Init(syncer::SyncableService::StartSyncFlare()));
  }
  void TearDown() override {
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

 protected:
  // It's needed for an initialisation of thread runners that are used in
  // MockPasswordStore.
  base::MessageLoop message_loop_;
  MockPasswordManagerClient client_;
  scoped_refptr<MockPasswordStore> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseDetectionManagerTest);
};

// Verify that CheckReuse is called on each key pressed event with an argument
// equal to the last 30 keystrokes typed after the last main frame navigaion.
TEST_F(PasswordReuseDetectionManagerTest, CheckReuseCalled) {
  const GURL gurls[] = {GURL("https://www.example.com"),
                        GURL("https://www.otherexample.com")};
  const base::string16 input[] = {
      base::ASCIIToUTF16(
          "1234567890abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVXYZ"),
      base::ASCIIToUTF16("?<>:'{}ABCDEF")};

  EXPECT_CALL(client_, GetPasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  for (size_t test = 0; test < arraysize(gurls); ++test) {
    manager.DidNavigateMainFrame(gurls[test]);
    for (size_t i = 0; i < input[test].size(); ++i) {
      base::string16 expected_input = input[test].substr(0, i + 1);
      if (expected_input.size() > kMaxNumberOfCharactersToStore)
        expected_input = expected_input.substr(expected_input.size() -
                                               kMaxNumberOfCharactersToStore);
      EXPECT_CALL(
          *store_,
          CheckReuse(expected_input, gurls[test].GetOrigin().spec(), &manager));
      manager.OnKeyPressed(input[test].substr(i, 1));
      testing::Mock::VerifyAndClearExpectations(store_.get());
    }
  }
}

}  // namespace

}  // namespace password_manager
