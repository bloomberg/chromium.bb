// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/init_aware_model.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "components/feature_engagement_tracker/internal/proto/event.pb.h"
#include "components/feature_engagement_tracker/internal/test/event_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SaveArg;
using testing::Sequence;

namespace feature_engagement_tracker {

namespace {

class MockModel : public Model {
 public:
  MockModel() = default;
  ~MockModel() override = default;

  // Model implementation.
  MOCK_METHOD2(Initialize,
               void(const OnModelInitializationFinished&, uint32_t));
  MOCK_CONST_METHOD0(IsReady, bool());
  MOCK_CONST_METHOD1(GetEvent, Event*(const std::string&));
  MOCK_METHOD2(IncrementEvent, void(const std::string&, uint32_t));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModel);
};

class InitAwareModelTest : public testing::Test {
 public:
  InitAwareModelTest() : mocked_model_(nullptr) {
    load_callback_ = base::Bind(&InitAwareModelTest::OnModelInitialized,
                                base::Unretained(this));
  }

  ~InitAwareModelTest() override = default;

  void SetUp() override {
    auto mocked_model = base::MakeUnique<MockModel>();
    mocked_model_ = mocked_model.get();
    model_ = base::MakeUnique<InitAwareModel>(std::move(mocked_model));
  }

 protected:
  void OnModelInitialized(bool success) { load_success_ = success; }

  std::unique_ptr<InitAwareModel> model_;
  MockModel* mocked_model_;

  // Load callback tracking.
  base::Optional<bool> load_success_;
  Model::OnModelInitializationFinished load_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InitAwareModelTest);
};

}  // namespace

TEST_F(InitAwareModelTest, PassThroughIsReady) {
  EXPECT_CALL(*mocked_model_, IsReady()).Times(1);
  model_->IsReady();
}

TEST_F(InitAwareModelTest, PassThroughGetEvent) {
  Event foo;
  foo.set_name("foo");
  test::SetEventCountForDay(&foo, 1, 1);

  EXPECT_CALL(*mocked_model_, GetEvent(foo.name()))
      .WillRepeatedly(Return(&foo));
  EXPECT_CALL(*mocked_model_, GetEvent("bar")).WillRepeatedly(Return(nullptr));

  test::VerifyEventsEqual(&foo, model_->GetEvent(foo.name()));
  EXPECT_EQ(nullptr, model_->GetEvent("bar"));
}

TEST_F(InitAwareModelTest, PassThroughIncrementEvent) {
  EXPECT_CALL(*mocked_model_, IsReady()).WillRepeatedly(Return(true));

  Sequence sequence;
  EXPECT_CALL(*mocked_model_, IncrementEvent("foo", 0U)).InSequence(sequence);
  EXPECT_CALL(*mocked_model_, IncrementEvent("bar", 1U)).InSequence(sequence);

  model_->IncrementEvent("foo", 0U);
  model_->IncrementEvent("bar", 1U);
}

TEST_F(InitAwareModelTest, QueuedIncrementEvent) {
  {
    EXPECT_CALL(*mocked_model_, IsReady()).WillRepeatedly(Return(false));
    EXPECT_CALL(*mocked_model_, IncrementEvent(_, _)).Times(0);

    model_->IncrementEvent("foo", 0U);
    model_->IncrementEvent("bar", 1U);
  }

  Model::OnModelInitializationFinished callback;
  EXPECT_CALL(*mocked_model_, Initialize(_, 2U))
      .WillOnce(SaveArg<0>(&callback));
  model_->Initialize(load_callback_, 2U);

  {
    Sequence sequence;
    EXPECT_CALL(*mocked_model_, IncrementEvent("foo", 0U))
        .Times(1)
        .InSequence(sequence);
    EXPECT_CALL(*mocked_model_, IncrementEvent("bar", 1U))
        .Times(1)
        .InSequence(sequence);

    callback.Run(true);
    EXPECT_TRUE(load_success_.value());
  }

  EXPECT_CALL(*mocked_model_, IsReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mocked_model_, IncrementEvent("qux", 3U)).Times(1);
  model_->IncrementEvent("qux", 3U);
}

}  // namespace feature_engagement_tracker
