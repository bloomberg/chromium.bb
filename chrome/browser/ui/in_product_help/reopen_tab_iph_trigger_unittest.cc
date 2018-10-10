// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/reopen_tab_iph_trigger.h"

#include "base/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

using feature_engagement::test::MockTracker;

namespace in_product_help {

namespace {

void DismissImmediately(ReopenTabIPHTrigger* trigger) {
  trigger->HelpDismissed();
}

void DismissAndSetFlag(ReopenTabIPHTrigger* trigger, bool* triggered) {
  *triggered = true;
  trigger->HelpDismissed();
}

}  // namespace

TEST(ReopenTabIPHTriggerTest, TriggersIPH) {
  NiceMock<MockTracker> mock_tracker;

  // We expect to send the backend our trigger event and ask if we should
  // display. After the backend replying yes, we should tell the backend IPH was
  // dismissed by the user.
  EXPECT_CALL(
      mock_tracker,
      NotifyEvent(Eq(feature_engagement::events::kReopenTabConditionsMet)))
      .Times(1);
  EXPECT_CALL(mock_tracker, ShouldTriggerHelpUI(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_tracker, Dismissed(_)).Times(1);

  // Instantiate IPH and send sequence of user interactions.
  base::SimpleTestTickClock clock;
  ReopenTabIPHTrigger reopen_tab_iph(&mock_tracker, &clock);

  reopen_tab_iph.SetShowHelpCallback(
      base::BindRepeating(DismissImmediately, &reopen_tab_iph));

  auto activation_time = clock.NowTicks();
  clock.Advance(ReopenTabIPHTrigger::kTabMinimumActiveDuration);
  reopen_tab_iph.ActiveTabClosed(activation_time);
  reopen_tab_iph.NewTabOpened();
  reopen_tab_iph.OmniboxFocused();
}

TEST(ReopenTabIPHTriggerTest, RespectsBackendShouldTrigger) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(mock_tracker, ShouldTriggerHelpUI(_))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_tracker, Dismissed(_)).Times(0);

  base::SimpleTestTickClock clock;
  ReopenTabIPHTrigger reopen_tab_iph(&mock_tracker, &clock);

  reopen_tab_iph.SetShowHelpCallback(
      base::BindRepeating(DismissImmediately, &reopen_tab_iph));

  auto activation_time = clock.NowTicks();
  clock.Advance(ReopenTabIPHTrigger::kTabMinimumActiveDuration);
  reopen_tab_iph.ActiveTabClosed(activation_time);
  reopen_tab_iph.NewTabOpened();
  reopen_tab_iph.OmniboxFocused();
}

TEST(ReopenTabIPHTriggerTest, TabNotActiveLongEnough) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(mock_tracker, NotifyEvent(_)).Times(0);
  EXPECT_CALL(mock_tracker, ShouldTriggerHelpUI(_)).Times(0);

  base::SimpleTestTickClock clock;
  ReopenTabIPHTrigger reopen_tab_iph(&mock_tracker, &clock);

  auto activation_time = clock.NowTicks();
  clock.Advance(ReopenTabIPHTrigger::kTabMinimumActiveDuration / 2);
  reopen_tab_iph.ActiveTabClosed(activation_time);
  reopen_tab_iph.NewTabOpened();
  reopen_tab_iph.OmniboxFocused();
}

TEST(ReopenTabIPHTriggerTest, RespectsTimeouts) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(mock_tracker, NotifyEvent(_)).Times(0);
  EXPECT_CALL(mock_tracker, ShouldTriggerHelpUI(_)).Times(0);

  base::SimpleTestTickClock clock;
  ReopenTabIPHTrigger reopen_tab_iph(&mock_tracker, &clock);

  reopen_tab_iph.SetShowHelpCallback(
      base::BindRepeating(DismissImmediately, &reopen_tab_iph));

  auto activation_time = clock.NowTicks();
  clock.Advance(ReopenTabIPHTrigger::kTabMinimumActiveDuration);
  reopen_tab_iph.ActiveTabClosed(activation_time);
  clock.Advance(ReopenTabIPHTrigger::kNewTabOpenedTimeout);
  reopen_tab_iph.NewTabOpened();
  reopen_tab_iph.OmniboxFocused();

  activation_time = clock.NowTicks();
  clock.Advance(ReopenTabIPHTrigger::kTabMinimumActiveDuration);
  reopen_tab_iph.ActiveTabClosed(activation_time);
  reopen_tab_iph.NewTabOpened();
  clock.Advance(ReopenTabIPHTrigger::kOmniboxFocusedTimeout);
  reopen_tab_iph.OmniboxFocused();
}

TEST(ReopenTabIPHTriggerTest, TriggersTwice) {
  NiceMock<MockTracker> mock_tracker;

  EXPECT_CALL(
      mock_tracker,
      NotifyEvent(Eq(feature_engagement::events::kReopenTabConditionsMet)))
      .Times(2);
  EXPECT_CALL(mock_tracker, ShouldTriggerHelpUI(_))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tracker, Dismissed(_)).Times(2);

  base::SimpleTestTickClock clock;
  ReopenTabIPHTrigger reopen_tab_iph(&mock_tracker, &clock);

  bool triggered = false;
  reopen_tab_iph.SetShowHelpCallback(
      base::BindRepeating(DismissAndSetFlag, &reopen_tab_iph, &triggered));

  auto activation_time = clock.NowTicks();
  clock.Advance(ReopenTabIPHTrigger::kTabMinimumActiveDuration);
  reopen_tab_iph.ActiveTabClosed(activation_time);
  reopen_tab_iph.NewTabOpened();
  reopen_tab_iph.OmniboxFocused();

  EXPECT_TRUE(triggered);
  triggered = false;

  clock.Advance(ReopenTabIPHTrigger::kTabMinimumActiveDuration);
  reopen_tab_iph.ActiveTabClosed(activation_time);
  reopen_tab_iph.NewTabOpened();
  reopen_tab_iph.OmniboxFocused();

  EXPECT_TRUE(triggered);
}

}  // namespace in_product_help
