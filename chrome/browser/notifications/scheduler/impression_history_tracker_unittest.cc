// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/scheduler/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::Invoke;
using StoreEntries = std::vector<std::unique_ptr<notifications::ClientState>>;

namespace notifications {
namespace {

const char kGuid1[] = "guid1";

struct TestCase {
  // Input data that will be pushed to the target class.
  std::vector<test::ImpressionTestData> input;

  // Expected output data.
  std::vector<test::ImpressionTestData> expected;
};

Impression CreateImpression(const base::Time& create_time,
                            const std::string& guid) {
  return {create_time,
          UserFeedback::kNoFeedback,
          ImpressionResult::kInvalid,
          false /* integrated */,
          SchedulerTaskTime::kMorning,
          guid,
          SchedulerClientType::kTest1};
}

TestCase CreateDefaultTestCase() {
  TestCase test_case;
  test_case.input = {{SchedulerClientType::kTest1,
                      2 /* current_max_daily_show */,
                      {},
                      base::nullopt /* suppression_info */}};
  test_case.expected = test_case.input;
  return test_case;
}

class MockImpressionStore : public CollectionStore<ClientState> {
 public:
  MockImpressionStore() {}

  MOCK_METHOD1(InitAndLoad, void(CollectionStore<ClientState>::LoadCallback));
  MOCK_METHOD3(Add,
               void(const std::string&,
                    const ClientState&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD3(Update,
               void(const std::string&,
                    const ClientState&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD2(Delete,
               void(const std::string&, base::OnceCallback<void(bool)>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockImpressionStore);
};

class MockDelegate : public ImpressionHistoryTracker::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() = default;
  MOCK_METHOD0(OnImpressionUpdated, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

// TODO(xingliu): Add more test cases following the test doc.
class ImpressionHistoryTrackerTest : public ::testing::Test {
 public:
  ImpressionHistoryTrackerTest() : store_(nullptr), delegate_(nullptr) {}
  ~ImpressionHistoryTrackerTest() override = default;

  void SetUp() override {
    config_.impression_expiration = base::TimeDelta::FromDays(28);
    config_.suppression_duration = base::TimeDelta::FromDays(56);
  }

 protected:
  // Creates the tracker and push in data.
  void InitTrackerWithData(const TestCase& test_case) {
    StoreEntries entries;
    test::AddImpressionTestData(test_case.input, &entries);

    auto store = std::make_unique<MockImpressionStore>();
    store_ = store.get();
    delegate_ = std::make_unique<MockDelegate>();
    impression_trakcer_ = std::make_unique<ImpressionHistoryTrackerImpl>(
        config_, std::move(store));

    // Initialize the store and call the callback.
    EXPECT_CALL(*store_, InitAndLoad(_))
        .WillOnce(
            Invoke([&entries](base::OnceCallback<void(bool, StoreEntries)> cb) {
              std::move(cb).Run(true, std::move(entries));
            }));
    base::RunLoop loop;
    impression_trakcer_->Init(
        delegate_.get(), base::BindOnce(
                             [](base::RepeatingClosure closure, bool success) {
                               EXPECT_TRUE(success);
                               std::move(closure).Run();
                             },
                             loop.QuitClosure()));
    loop.Run();
  }

  // Verifies the |expected_test_data| matches the internal states.
  void VerifyClientStates(const TestCase& test_case) {
    std::map<SchedulerClientType, const ClientState*> client_states;
    impression_trakcer_->GetClientStates(&client_states);

    ImpressionHistoryTracker::ClientStates expected_client_states;
    test::AddImpressionTestData(test_case.expected, &expected_client_states);

    DCHECK_EQ(expected_client_states.size(), client_states.size());
    for (const auto& expected : expected_client_states) {
      auto output_it = client_states.find(expected.first);
      DCHECK(output_it != client_states.end());
      EXPECT_EQ(*expected.second, *output_it->second)
          << "Unmatch client states: \n"
          << "Expected: \n"
          << expected.second->DebugPrint() << " \n"
          << "Acutual: \n"
          << output_it->second->DebugPrint();
    }
  }

  const SchedulerConfig& config() const { return config_; }
  MockImpressionStore* store() { return store_; }
  MockDelegate* delegate() { return delegate_.get(); }
  ImpressionHistoryTracker* tracker() { return impression_trakcer_.get(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  SchedulerConfig config_;
  std::unique_ptr<ImpressionHistoryTracker> impression_trakcer_;
  MockImpressionStore* store_;
  std::unique_ptr<MockDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTrackerTest);
};

// Verifies expired impression will be deleted.
TEST_F(ImpressionHistoryTrackerTest, DeleteExpiredImpression) {
  TestCase test_case;
  auto expired_create_time = base::Time::Now() - base::TimeDelta::FromDays(1) -
                             config().impression_expiration;
  auto not_expired_time = base::Time::Now() + base::TimeDelta::FromDays(1) -
                          config().impression_expiration;
  Impression expired{expired_create_time,         UserFeedback::kNoFeedback,
                     ImpressionResult::kInvalid,  false /* integrated */,
                     SchedulerTaskTime::kMorning, "guid1",
                     SchedulerClientType::kTest1};
  Impression not_expired{not_expired_time,
                         UserFeedback::kNoFeedback,
                         ImpressionResult::kInvalid,
                         false /* integrated */,
                         SchedulerTaskTime::kMorning,
                         "guid2",
                         SchedulerClientType::kTest1};

  // The impressions in the input should be sorted by creation time when gets
  // loaded to memory.
  test_case.input = {{SchedulerClientType::kTest1,
                      2 /* current_max_daily_show */,
                      {expired, not_expired, expired},
                      base::nullopt /* suppression_info */}};

  // Expired impression created in |expired_create_time| should be deleted.
  // No change expected on the next impression, which is not expired and no user
  // feedback .
  test_case.expected = {{SchedulerClientType::kTest1,
                         2 /* current_max_daily_show */,
                         {not_expired},
                         base::nullopt /* suppression_info */}};

  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _));
  EXPECT_CALL(*delegate(), OnImpressionUpdated());
  tracker()->AnalyzeImpressionHistory();
  VerifyClientStates(test_case);
}

// If impression has been deleted, click should have no result.
TEST_F(ImpressionHistoryTrackerTest, ClickNoImpression) {
  TestCase test_case = CreateDefaultTestCase();
  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _)).Times(0);
  EXPECT_CALL(*delegate(), OnImpressionUpdated()).Times(0);
  tracker()->OnClick(kGuid1);
  VerifyClientStates(test_case);
}

struct UserActionTestParam {
  ImpressionResult impression_result = ImpressionResult::kInvalid;
  UserFeedback user_feedback = UserFeedback::kNoFeedback;
  int current_max_daily_show = 0;
  base::Optional<ActionButtonType> button_type;
  base::Optional<SuppressionInfo> suppression_info;
};

class ImpressionHistoryTrackerUserActionTest
    : public ImpressionHistoryTrackerTest,
      public ::testing::WithParamInterface<UserActionTestParam> {
 public:
  ImpressionHistoryTrackerUserActionTest() = default;
  ~ImpressionHistoryTrackerUserActionTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImpressionHistoryTrackerUserActionTest);
};

// TODO(xingliu): Add test for unhelpful/dismiss, need to use base::Clock to
// mock base::Time::Now().
const UserActionTestParam kUserActionTestParams[] = {
    {ImpressionResult::kPositive, UserFeedback::kClick, 3, base::nullopt,
     base::nullopt},
    {ImpressionResult::kPositive, UserFeedback::kHelpful, 3,
     ActionButtonType::kHelpful, base::nullopt}};

// User actions like clicks should update the ClientState data accordingly.
TEST_P(ImpressionHistoryTrackerUserActionTest, UserAction) {
  TestCase test_case = CreateDefaultTestCase();
  Impression impression = CreateImpression(base::Time::Now(), kGuid1);
  DCHECK(!test_case.input.empty());
  test_case.input.front().impressions.emplace_back(impression);

  impression.impression = GetParam().impression_result;
  impression.integrated = true;
  impression.feedback = GetParam().user_feedback;

  test_case.expected.front().current_max_daily_show =
      GetParam().current_max_daily_show;
  test_case.expected.front().impressions.emplace_back(impression);
  test_case.expected.front().suppression_info = GetParam().suppression_info;

  InitTrackerWithData(test_case);
  EXPECT_CALL(*store(), Update(_, _, _));
  EXPECT_CALL(*delegate(), OnImpressionUpdated());

  // Trigger user action.
  if (GetParam().user_feedback == UserFeedback::kClick)
    tracker()->OnClick(kGuid1);
  else if (GetParam().button_type.has_value())
    tracker()->OnActionClick(kGuid1, GetParam().button_type.value());

  VerifyClientStates(test_case);
}

INSTANTIATE_TEST_SUITE_P(,
                         ImpressionHistoryTrackerUserActionTest,
                         testing::ValuesIn(kUserActionTestParams));

}  // namespace

}  // namespace notifications
