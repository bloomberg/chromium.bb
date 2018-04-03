// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "content/public/browser/visibility.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class MockLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  MockLifecycleUnitObserver() = default;

  MOCK_METHOD1(OnLifecycleUnitStateChanged, void(LifecycleUnit*));
  MOCK_METHOD2(OnLifecycleUnitVisibilityChanged,
               void(LifecycleUnit*, content::Visibility));
  MOCK_METHOD1(OnLifecycleUnitDestroyed, void(LifecycleUnit*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLifecycleUnitObserver);
};

class DummyLifecycleUnit : public LifecycleUnitBase {
 public:
  using LifecycleUnitBase::SetState;
  using LifecycleUnitBase::OnLifecycleUnitVisibilityChanged;

  DummyLifecycleUnit() = default;
  ~DummyLifecycleUnit() override { OnLifecycleUnitDestroyed(); }

  // LifecycleUnit:
  TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() override {
    return nullptr;
  }
  base::string16 GetTitle() const override { return base::string16(); }
  std::string GetIconURL() const override { return std::string(); }
  SortKey GetSortKey() const override { return SortKey(); }
  State GetState() const override { return State::LOADED; }
  bool Freeze() override { return false; }
  int GetEstimatedMemoryFreedOnDiscardKB() const override { return 0; }
  bool CanDiscard(DiscardReason reason) const override { return false; }
  bool Discard(DiscardReason discard_reason) override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyLifecycleUnit);
};

}  // namespace

// Verify that GetID() returns different ids for different LifecycleUnits, but
// always the same id for the same LifecycleUnit.
TEST(LifecycleUnitBaseTest, GetID) {
  DummyLifecycleUnit a;
  DummyLifecycleUnit b;
  DummyLifecycleUnit c;

  EXPECT_NE(a.GetID(), b.GetID());
  EXPECT_NE(a.GetID(), c.GetID());
  EXPECT_NE(b.GetID(), c.GetID());

  EXPECT_EQ(a.GetID(), a.GetID());
  EXPECT_EQ(b.GetID(), b.GetID());
  EXPECT_EQ(c.GetID(), c.GetID());
}

// Verify that observers are notified when the state changes and when the
// LifecycleUnit is destroyed.
TEST(LifecycleUnitBaseTest, SetStateNotifiesObservers) {
  testing::StrictMock<MockLifecycleUnitObserver> observer;
  DummyLifecycleUnit lifecycle_unit;
  lifecycle_unit.AddObserver(&observer);

  // Observer is notified when the state changes.
  EXPECT_CALL(observer, OnLifecycleUnitStateChanged(&lifecycle_unit));
  lifecycle_unit.SetState(LifecycleUnit::State::DISCARDED);
  testing::Mock::VerifyAndClear(&observer);

  // Observer isn't notified when the state stays the same.
  lifecycle_unit.SetState(LifecycleUnit::State::DISCARDED);

  lifecycle_unit.RemoveObserver(&observer);
}

// Verify that observers are notified when the LifecycleUnit is destroyed.
TEST(LifecycleUnitBaseTest, DestroyNotifiesObservers) {
  testing::StrictMock<MockLifecycleUnitObserver> observer;
  {
    DummyLifecycleUnit lifecycle_unit;
    lifecycle_unit.AddObserver(&observer);
    EXPECT_CALL(observer, OnLifecycleUnitDestroyed(&lifecycle_unit));
  }
  testing::Mock::VerifyAndClear(&observer);
}

// Verify that observers are notified when the visibility of the LifecyleUnit
// changes. Verify that when visibility changes |last_visibility_change_time_|
// is updated with the time of the change.
TEST(LifecycleUnitBaseTest, VisibilityChangeNotifiesObserversAndUpdatesTime) {
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(&test_clock_);
  testing::StrictMock<MockLifecycleUnitObserver> observer;
  DummyLifecycleUnit lifecycle_unit;
  lifecycle_unit.AddObserver(&observer);

  // Observer is notified when the visibility changes.
  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &lifecycle_unit, content::Visibility::HIDDEN))
      .WillOnce(testing::Invoke([&](LifecycleUnit* lifecycle_unit,
                                    content::Visibility visibility) {
        EXPECT_EQ(NowTicks(), lifecycle_unit->GetLastVisibilityChangeTime());
      }));

  lifecycle_unit.OnLifecycleUnitVisibilityChanged(content::Visibility::HIDDEN);
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &lifecycle_unit, content::Visibility::OCCLUDED))
      .WillOnce(testing::Invoke([&](LifecycleUnit* lifecycle_unit,
                                    content::Visibility visibility) {
        EXPECT_EQ(NowTicks(), lifecycle_unit->GetLastVisibilityChangeTime());
      }));

  lifecycle_unit.OnLifecycleUnitVisibilityChanged(
      content::Visibility::OCCLUDED);
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &lifecycle_unit, content::Visibility::VISIBLE))
      .WillOnce(testing::Invoke([&](LifecycleUnit* lifecycle_unit,
                                    content::Visibility visibility) {
        EXPECT_EQ(NowTicks(), lifecycle_unit->GetLastVisibilityChangeTime());
      }));

  lifecycle_unit.OnLifecycleUnitVisibilityChanged(content::Visibility::VISIBLE);
  testing::Mock::VerifyAndClear(&observer);

  lifecycle_unit.RemoveObserver(&observer);
}

}  // namespace resource_coordinator
