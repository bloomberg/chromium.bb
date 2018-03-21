// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
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
// changes.
TEST(LifecycleUnitBaseTest, ChangeVisibilityNotifiesObservers) {
  testing::StrictMock<MockLifecycleUnitObserver> observer;
  DummyLifecycleUnit lifecycle_unit;
  lifecycle_unit.AddObserver(&observer);

  // Observer is notified when the visibility changes.
  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &lifecycle_unit, content::Visibility::HIDDEN));
  lifecycle_unit.OnLifecycleUnitVisibilityChanged(content::Visibility::HIDDEN);
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &lifecycle_unit, content::Visibility::OCCLUDED));
  lifecycle_unit.OnLifecycleUnitVisibilityChanged(
      content::Visibility::OCCLUDED);
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_CALL(observer, OnLifecycleUnitVisibilityChanged(
                            &lifecycle_unit, content::Visibility::VISIBLE));
  lifecycle_unit.OnLifecycleUnitVisibilityChanged(content::Visibility::VISIBLE);
  testing::Mock::VerifyAndClear(&observer);

  lifecycle_unit.RemoveObserver(&observer);
}
}  // namespace resource_coordinator
