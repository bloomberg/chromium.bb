// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/properties.h"

#include "base/observer_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class DummyNode;

class DummyObserver {
 public:
  DummyObserver() {}
  ~DummyObserver() {}

  MOCK_METHOD1(NotifyAlways, void(DummyNode*));
  MOCK_METHOD1(NotifyOnlyOnChanges, void(DummyNode*));
  MOCK_METHOD1(NotifyAlwaysConst, void(const DummyNode*));
  MOCK_METHOD1(NotifyOnlyOnChangesConst, void(const DummyNode*));
};

class DummyNode {
 public:
  DummyNode() {}
  ~DummyNode() {}

  void AddObserver(DummyObserver* observer) {
    observers_.AddObserver(observer);
    new_observers_.push_back(observer);
  }

  base::ObserverList<DummyObserver>::Unchecked& observers() {
    return observers_;
  }

  const std::vector<DummyObserver*>& GetObservers() { return new_observers_; }

  bool observed_always() const { return observed_always_.value(); }
  bool observed_only_on_changes() const {
    return observed_only_on_changes_.value();
  }

  void SetObservedAlways(bool value) {
    observed_always_.SetAndNotify(this, value);
  }
  bool SetObservedOnlyOnChanges(bool value) {
    return observed_only_on_changes_.SetAndMaybeNotify(this, value);
  }

 private:
  using ObservedProperty =
      ObservedPropertyImpl<DummyNode, DummyObserver, DummyNode, DummyObserver>;

  ObservedProperty::NotifiesAlways<bool,
                                   &DummyObserver::NotifyAlways,
                                   &DummyObserver::NotifyAlwaysConst>
      observed_always_{false};
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &DummyObserver::NotifyOnlyOnChanges,
      &DummyObserver::NotifyOnlyOnChangesConst>
      observed_only_on_changes_{false};

  base::ObserverList<DummyObserver>::Unchecked observers_;
  std::vector<DummyObserver*> new_observers_;
};

class GraphPropertiesTest : public ::testing::Test {
 public:
  GraphPropertiesTest() {}
  ~GraphPropertiesTest() override {}

  void SetUp() override {
    node_.AddObserver(&observer_);
    ::testing::Test::SetUp();
  }

  DummyObserver observer_;
  DummyNode node_;
};

}  // namespace

TEST_F(GraphPropertiesTest, ObservedAlwaysProperty) {
  EXPECT_EQ(false, node_.observed_always());

  EXPECT_CALL(observer_, NotifyAlways(&node_));
  EXPECT_CALL(observer_, NotifyAlwaysConst(&node_));
  node_.SetObservedAlways(false);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(false, node_.observed_always());

  EXPECT_CALL(observer_, NotifyAlways(&node_));
  EXPECT_CALL(observer_, NotifyAlwaysConst(&node_));
  node_.SetObservedAlways(true);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(true, node_.observed_always());

  EXPECT_CALL(observer_, NotifyAlways(&node_));
  EXPECT_CALL(observer_, NotifyAlwaysConst(&node_));
  node_.SetObservedAlways(true);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(true, node_.observed_always());

  testing::Mock::VerifyAndClear(&observer_);
}

TEST_F(GraphPropertiesTest, ObservedOnlyOnChangesProperty) {
  EXPECT_EQ(false, node_.observed_only_on_changes());

  EXPECT_FALSE(node_.SetObservedOnlyOnChanges(false));
  EXPECT_EQ(false, node_.observed_only_on_changes());

  EXPECT_CALL(observer_, NotifyOnlyOnChanges(&node_));
  EXPECT_CALL(observer_, NotifyOnlyOnChangesConst(&node_));
  EXPECT_TRUE(node_.SetObservedOnlyOnChanges(true));
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_EQ(true, node_.observed_only_on_changes());

  EXPECT_FALSE(node_.SetObservedOnlyOnChanges(true));
  EXPECT_EQ(true, node_.observed_only_on_changes());

  testing::Mock::VerifyAndClear(&observer_);
}

}  // namespace performance_manager
