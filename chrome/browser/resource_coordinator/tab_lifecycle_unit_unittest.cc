// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/tabs/tab_strip_model_impl.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

constexpr base::TimeDelta kShortDelay = base::TimeDelta::FromSeconds(1);

class MockTabLifecycleObserver : public TabLifecycleObserver {
 public:
  MockTabLifecycleObserver() = default;

  MOCK_METHOD2(OnDiscardedStateChange,
               void(content::WebContents* contents, bool is_discarded));
  MOCK_METHOD2(OnAutoDiscardableStateChange,
               void(content::WebContents* contents, bool is_auto_discardable));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabLifecycleObserver);
};

}  // namespace

class TabLifecycleUnitTest : public ChromeRenderViewHostTestHarness {
 protected:
  using TabLifecycleUnit = TabLifecycleUnitSource::TabLifecycleUnit;

  TabLifecycleUnitTest() : scoped_set_tick_clock_for_testing_(&test_clock_) {
    observers_.AddObserver(&observer_);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_strip_model_ = std::make_unique<TabStripModelImpl>(
        &tab_strip_model_delegate_, profile());
    tab_strip_model_->AppendWebContents(web_contents(), true);
  }

  void TearDown() override {
    tab_strip_model_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  testing::StrictMock<MockTabLifecycleObserver> observer_;
  base::ObserverList<TabLifecycleObserver> observers_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  base::SimpleTestTickClock test_clock_;

 private:
  TestTabStripModelDelegate tab_strip_model_delegate_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitTest);
};

TEST_F(TabLifecycleUnitTest, SetFocused) {
  test_clock_.Advance(kShortDelay);
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents(),
                                      tab_strip_model_.get());
  EXPECT_EQ(base::TimeTicks(),
            tab_lifecycle_unit.GetSortKey().last_focused_time);

  test_clock_.Advance(kShortDelay);
  tab_lifecycle_unit.SetFocused(true);
  EXPECT_EQ(base::TimeTicks::Max(),
            tab_lifecycle_unit.GetSortKey().last_focused_time);

  test_clock_.Advance(kShortDelay);
  tab_lifecycle_unit.SetFocused(false);
  EXPECT_EQ(test_clock_.NowTicks(),
            tab_lifecycle_unit.GetSortKey().last_focused_time);
}

TEST_F(TabLifecycleUnitTest, AutoDiscardable) {
  TabLifecycleUnit tab_lifecycle_unit(&observers_, web_contents(),
                                      tab_strip_model_.get());
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());

  EXPECT_CALL(observer_, OnAutoDiscardableStateChange(web_contents(), false));
  tab_lifecycle_unit.SetAutoDiscardable(false);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_FALSE(tab_lifecycle_unit.IsAutoDiscardable());

  EXPECT_CALL(observer_, OnAutoDiscardableStateChange(web_contents(), true));
  tab_lifecycle_unit.SetAutoDiscardable(true);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_TRUE(tab_lifecycle_unit.IsAutoDiscardable());
}

}  // namespace resource_coordinator
