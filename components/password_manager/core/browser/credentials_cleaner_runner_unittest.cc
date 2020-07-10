// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credentials_cleaner_runner.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class MockCredentialsCleaner : public CredentialsCleaner {
 public:
  MockCredentialsCleaner() = default;
  ~MockCredentialsCleaner() override = default;
  MOCK_METHOD0(NeedsCleaning, bool());
  MOCK_METHOD1(StartCleaning, void(Observer* observer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCredentialsCleaner);
};

class DestructionCheckableCleanerRunner : public CredentialsCleanerRunner {
 public:
  // |object_destroyed| is used to inform caller that the object destroyed
  // itself.
  explicit DestructionCheckableCleanerRunner(bool* object_destroyed)
      : object_destroyed_(object_destroyed) {}

 private:
  ~DestructionCheckableCleanerRunner() override { *object_destroyed_ = true; }

  // The class will assign true to the pointee on destruction.
  bool* object_destroyed_;
};

class CredentialsCleanerRunnerTest : public ::testing::Test {
 public:
  CredentialsCleanerRunnerTest() = default;

  ~CredentialsCleanerRunnerTest() override { EXPECT_TRUE(runner_destroyed_); }

 protected:
  CredentialsCleanerRunner* GetRunner() { return cleaning_tasks_runner_; }

 private:
  bool runner_destroyed_ = false;

  // This is a non-owning pointer, because the runner will delete itself.
  DestructionCheckableCleanerRunner* cleaning_tasks_runner_ =
      new DestructionCheckableCleanerRunner(&runner_destroyed_);

  DISALLOW_COPY_AND_ASSIGN(CredentialsCleanerRunnerTest);
};

// This test would fail if the object doesn't delete itself.
TEST_F(CredentialsCleanerRunnerTest, EmptyTasks) {
  GetRunner()->StartCleaning();
}

// In this test we check that credential clean-ups runner executes the clean-up
// tasks in the order they were added.
TEST_F(CredentialsCleanerRunnerTest, NonEmptyTasks) {
  static constexpr int kCleanersCount = 5;

  auto* cleaning_tasks_runner = GetRunner();
  std::vector<MockCredentialsCleaner*> raw_cleaners;
  for (int i = 0; i < kCleanersCount; ++i) {
    auto cleaner = std::make_unique<MockCredentialsCleaner>();
    raw_cleaners.push_back(cleaner.get());
    EXPECT_CALL(*cleaner, NeedsCleaning).WillOnce(::testing::Return(true));
    cleaning_tasks_runner->MaybeAddCleaningTask(std::move(cleaner));
  }

  ::testing::InSequence dummy;
  for (MockCredentialsCleaner* cleaner : raw_cleaners)
    EXPECT_CALL(*cleaner, StartCleaning(cleaning_tasks_runner));

  cleaning_tasks_runner->StartCleaning();
  for (int i = 0; i < kCleanersCount; ++i)
    cleaning_tasks_runner->CleaningCompleted();
}

}  // namespace password_manager
