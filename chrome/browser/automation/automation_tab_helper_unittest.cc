// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/basictypes.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/automation/mock_tab_event_observer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

class AutomationTabHelperTest : public TabContentsWrapperTestHarness {
 public:
  virtual void SetUp() {
    TabContentsWrapperTestHarness::SetUp();
    mock_observer_.StartObserving(tab_helper());
  }

 protected:
  // These are here so that we don't have to add each test as a
  // |AutomationTabHelper| friend.
  void StartLoading() {
    tab_helper()->DidStartLoading();
  }

  void StopLoading() {
    tab_helper()->DidStopLoading();
  }

  void TabContentsDestroyed() {
    tab_helper()->TabContentsDestroyed(contents_wrapper()->tab_contents());
  }

  void WillPerformClientRedirect(int64 frame_id) {
    tab_helper()->OnWillPerformClientRedirect(frame_id, 0);
  }

  void CompleteOrCancelClientRedirect(int64 frame_id) {
    tab_helper()->OnDidCompleteOrCancelClientRedirect(frame_id);
  }

  AutomationTabHelper* tab_helper() {
    return contents_wrapper()->automation_tab_helper();
  }

  MockTabEventObserver mock_observer_;
};

ACTION_P2(CheckHasPendingLoads, tab_helper, has_pending_loads) {
  EXPECT_EQ(has_pending_loads, tab_helper->has_pending_loads());
}

TEST_F(AutomationTabHelperTest, AddAndRemoveObserver) {
  testing::MockFunction<void()> check;

  {
    testing::InSequence expect_in_sequence;
    EXPECT_CALL(mock_observer_, OnFirstPendingLoad(_));
    EXPECT_CALL(check, Call());
  }

  StartLoading();
  check.Call();
  tab_helper()->RemoveObserver(&mock_observer_);
  StartLoading();
}

TEST_F(AutomationTabHelperTest, StartStopLoading) {
  testing::MockFunction<void()> check;

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()))
      .WillOnce(CheckHasPendingLoads(tab_helper(), true));
  EXPECT_CALL(check, Call());
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()))
      .WillOnce(CheckHasPendingLoads(tab_helper(), false));

  EXPECT_FALSE(tab_helper()->has_pending_loads());
  StartLoading();
  EXPECT_TRUE(tab_helper()->has_pending_loads());
  check.Call();
  StopLoading();
  EXPECT_FALSE(tab_helper()->has_pending_loads());
}

TEST_F(AutomationTabHelperTest, DuplicateLoads) {
  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()));
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()));

  StartLoading();
  StartLoading();
  StopLoading();
  StopLoading();
}

TEST_F(AutomationTabHelperTest, ClientRedirect) {
  testing::MockFunction<void()> check;

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()))
      .WillOnce(CheckHasPendingLoads(tab_helper(), true));
  EXPECT_CALL(check, Call());
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()))
      .WillOnce(CheckHasPendingLoads(tab_helper(), false));

  WillPerformClientRedirect(1);
  EXPECT_TRUE(tab_helper()->has_pending_loads());
  check.Call();
  CompleteOrCancelClientRedirect(1);
  EXPECT_FALSE(tab_helper()->has_pending_loads());
}

TEST_F(AutomationTabHelperTest, DiscardExtraClientRedirects) {
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()));
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()));

  WillPerformClientRedirect(1);
  WillPerformClientRedirect(1);
  CompleteOrCancelClientRedirect(1);
  EXPECT_FALSE(tab_helper()->has_pending_loads());
  CompleteOrCancelClientRedirect(1);
  CompleteOrCancelClientRedirect(2);
}

TEST_F(AutomationTabHelperTest, StartStopLoadingWithClientRedirect) {
  testing::MockFunction<void()> check;

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()));
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()));

  StartLoading();
  WillPerformClientRedirect(1);
  CompleteOrCancelClientRedirect(1);
  StopLoading();
}

TEST_F(AutomationTabHelperTest, ClientRedirectBeforeLoad) {
  testing::MockFunction<void()> check;

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()));
  EXPECT_CALL(check, Call());
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()));

  StartLoading();
  WillPerformClientRedirect(1);
  CompleteOrCancelClientRedirect(1);
  EXPECT_TRUE(tab_helper()->has_pending_loads());
  check.Call();
  StopLoading();
}

TEST_F(AutomationTabHelperTest, ClientRedirectAfterLoad) {
  testing::MockFunction<void()> check;

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()));
  EXPECT_CALL(check, Call());
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()));

  StartLoading();
  WillPerformClientRedirect(1);
  StopLoading();
  EXPECT_TRUE(tab_helper()->has_pending_loads());
  check.Call();
  CompleteOrCancelClientRedirect(1);
  EXPECT_FALSE(tab_helper()->has_pending_loads());
}

TEST_F(AutomationTabHelperTest, AllFramesMustFinishRedirects) {
  testing::MockFunction<void()> check;

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()));
  EXPECT_CALL(check, Call());
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()));

  WillPerformClientRedirect(1);
  WillPerformClientRedirect(2);
  CompleteOrCancelClientRedirect(1);
  check.Call();
  EXPECT_TRUE(tab_helper()->has_pending_loads());
  CompleteOrCancelClientRedirect(2);
  EXPECT_FALSE(tab_helper()->has_pending_loads());
}

TEST_F(AutomationTabHelperTest, DestroyedTabStopsLoading) {
  testing::MockFunction<void()> check;

  testing::InSequence expect_in_sequence;
  EXPECT_CALL(mock_observer_, OnFirstPendingLoad(contents()));
  EXPECT_CALL(mock_observer_, OnNoMorePendingLoads(contents()));

  StartLoading();
  WillPerformClientRedirect(1);
  TabContentsDestroyed();
  EXPECT_FALSE(tab_helper()->has_pending_loads());
}
