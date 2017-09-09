// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detection_manager.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
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
    CHECK(store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr));
  }
  void TearDown() override {
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

 protected:
  // It's needed for an initialisation of thread runners that are used in
  // MockPasswordStore.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
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

// Verify that the keystroke buffer is cleared after 10 seconds of user
// inactivity.
TEST_F(PasswordReuseDetectionManagerTest,
       CheckThatBufferClearedAfterInactivity) {
  EXPECT_CALL(client_, GetPasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock);
  base::Time now = base::Time::Now();
  clock->SetNow(now);
  base::SimpleTestClock* clock_weak = clock.get();
  manager.SetClockForTesting(std::move(clock));

  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("1"), _, _));
  manager.OnKeyPressed(base::ASCIIToUTF16("1"));

  // Simulate 10 seconds of inactivity.
  clock_weak->SetNow(now + base::TimeDelta::FromSeconds(10));
  // Expect that a keystroke typed before inactivity is cleared.
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("2"), _, _));
  manager.OnKeyPressed(base::ASCIIToUTF16("2"));
}

// Verify that the keystroke buffer is cleared after user presses enter.
TEST_F(PasswordReuseDetectionManagerTest, CheckThatBufferClearedAfterEnter) {
  EXPECT_CALL(client_, GetPasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("1"), _, _));
  manager.OnKeyPressed(base::ASCIIToUTF16("1"));

  base::string16 enter_text(1, ui::VKEY_RETURN);
  EXPECT_CALL(*store_, CheckReuse(_, _, _)).Times(0);
  manager.OnKeyPressed(enter_text);

  // Expect only a keystroke typed after enter.
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("2"), _, _));
  manager.OnKeyPressed(base::ASCIIToUTF16("2"));
}

// Verify that after reuse found, no reuse checking happens till next main frame
// navigation.
TEST_F(PasswordReuseDetectionManagerTest, NoReuseCheckingAfterReuseFound) {
  EXPECT_CALL(client_, GetPasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  // Simulate that reuse found.
  manager.OnReuseFound(0ul, true, {}, 0);

  // Expect no checking of reuse.
  EXPECT_CALL(*store_, CheckReuse(_, _, _)).Times(0);
  manager.OnKeyPressed(base::ASCIIToUTF16("1"));

  // Expect that after main frame navigation checking is restored.
  manager.DidNavigateMainFrame(GURL("https://www.example.com"));
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("1"), _, _));
  manager.OnKeyPressed(base::ASCIIToUTF16("1"));
}

}  // namespace

}  // namespace password_manager
