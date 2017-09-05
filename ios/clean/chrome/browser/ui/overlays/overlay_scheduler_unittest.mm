// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/overlays/overlay_scheduler.h"

#include "base/memory/ptr_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/clean/chrome/browser/ui/commands/tab_grid_commands.h"
#import "ios/clean/chrome/browser/ui/overlays/browser_overlay_queue.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_queue_manager.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_scheduler_observer.h"
#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_coordinator.h"
#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_parent_coordinator.h"
#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_queue.h"
#import "ios/clean/chrome/browser/ui/overlays/test_helpers/test_overlay_queue_observer.h"
#import "ios/clean/chrome/browser/ui/overlays/web_state_overlay_queue.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test observer for the scheduler.
class TestOverlaySchedulerObserver : public OverlaySchedulerObserver {
 public:
  TestOverlaySchedulerObserver(WebStateList* web_state_list)
      : web_state_list_(web_state_list),
        active_index_(WebStateList::kInvalidIndex) {}

  void OverlaySchedulerWillShowOverlay(OverlayScheduler* scheduler,
                                       web::WebState* web_state) override {
    active_index_ = web_state_list_->GetIndexOfWebState(web_state);
  }

  int active_index() { return active_index_; }

 private:
  WebStateList* web_state_list_;
  int active_index_;
};

// Test fixture for OverlayScheduler.
class OverlaySchedulerTest : public PlatformTest {
 public:
  OverlaySchedulerTest()
      : PlatformTest(),
        browser_(ios::ChromeBrowserState::FromBrowserState(&browser_state_)),
        observer_(&browser_.web_state_list()) {
    OverlayQueueManager::CreateForBrowser(&browser_);
    OverlayScheduler::CreateForBrowser(&browser_);
    scheduler()->SetQueueManager(manager());
    scheduler()->AddObserver(&observer_);
  }

  ~OverlaySchedulerTest() override {
    scheduler()->RemoveObserver(&observer_);
    scheduler()->Disconnect();
  }

  Browser* browser() { return &browser_; }
  WebStateList* web_state_list() { return &browser_.web_state_list(); }
  TestOverlaySchedulerObserver* observer() { return &observer_; }
  OverlayQueueManager* manager() {
    return OverlayQueueManager::FromBrowser(browser());
  }
  OverlayScheduler* scheduler() {
    return OverlayScheduler::FromBrowser(browser());
  }

 private:
  web::TestBrowserState browser_state_;
  Browser browser_;
  TestOverlaySchedulerObserver observer_;
  TestOverlayQueue queue_;
};

// Tests that adding an overlay to the BrowserOverlayQueue triggers successful
// presentation.
TEST_F(OverlaySchedulerTest, AddBrowserOverlay) {
  BrowserOverlayQueue* queue = BrowserOverlayQueue::FromBrowser(browser());
  ASSERT_TRUE(queue);
  TestOverlayParentCoordinator* parent =
      [[TestOverlayParentCoordinator alloc] init];
  TestOverlayCoordinator* overlay = [[TestOverlayCoordinator alloc] init];
  queue->AddBrowserOverlay(overlay, parent);
  EXPECT_EQ(parent.presentedOverlay, overlay);
  EXPECT_TRUE(scheduler()->IsShowingOverlay());
  [overlay stop];
  EXPECT_EQ(parent.presentedOverlay, nil);
  EXPECT_FALSE(scheduler()->IsShowingOverlay());
}

// Tests that adding an overlay to a WebStateOverlayQueue triggers successful
// presentation.
TEST_F(OverlaySchedulerTest, AddWebStateOverlay) {
  web_state_list()->InsertWebState(0, base::MakeUnique<web::TestWebState>(),
                                   WebStateList::INSERT_FORCE_INDEX,
                                   WebStateOpener());
  ASSERT_EQ(web_state_list()->count(), 1);
  web::WebState* web_state = web_state_list()->GetWebStateAt(0);
  WebStateOverlayQueue* queue = WebStateOverlayQueue::FromWebState(web_state);
  ASSERT_TRUE(queue);
  TestOverlayParentCoordinator* parent =
      [[TestOverlayParentCoordinator alloc] init];
  queue->SetWebStateParentCoordinator(parent);
  TestOverlayCoordinator* overlay = [[TestOverlayCoordinator alloc] init];
  queue->AddWebStateOverlay(overlay);
  EXPECT_EQ(parent.presentedOverlay, overlay);
  EXPECT_TRUE(scheduler()->IsShowingOverlay());
  [overlay stop];
  EXPECT_EQ(parent.presentedOverlay, nil);
  EXPECT_FALSE(scheduler()->IsShowingOverlay());
}

// Tests that attempting to present an overlay for an inactive WebState will
// correctly switch the active WebState before presenting.
TEST_F(OverlaySchedulerTest, SwitchWebStateForOverlay) {
  // Add two WebStates and activate the second.
  web_state_list()->InsertWebState(0, base::MakeUnique<web::TestWebState>(),
                                   WebStateList::INSERT_FORCE_INDEX,
                                   WebStateOpener());
  ASSERT_EQ(web_state_list()->count(), 1);
  web_state_list()->InsertWebState(1, base::MakeUnique<web::TestWebState>(),
                                   WebStateList::INSERT_FORCE_INDEX,
                                   WebStateOpener());
  web_state_list()->ActivateWebStateAt(1);
  // Add an overlay to the queue corresponding with the first WebState.
  WebStateOverlayQueue* queue =
      WebStateOverlayQueue::FromWebState(web_state_list()->GetWebStateAt(0));
  ASSERT_TRUE(queue);
  TestOverlayParentCoordinator* parent =
      [[TestOverlayParentCoordinator alloc] init];
  queue->SetWebStateParentCoordinator(parent);
  TestOverlayCoordinator* overlay = [[TestOverlayCoordinator alloc] init];
  queue->AddWebStateOverlay(overlay);
  // Verify that the overlay is presented and the active WebState index is
  // updated.
  EXPECT_EQ(parent.presentedOverlay, overlay);
  EXPECT_TRUE(scheduler()->IsShowingOverlay());
  EXPECT_EQ(observer()->active_index(), 0);
  [overlay stop];
  EXPECT_EQ(parent.presentedOverlay, nil);
  EXPECT_FALSE(scheduler()->IsShowingOverlay());
}

// Tests that attempting to present an overlay for an inactive WebState will
// correctly switch the active WebState before presenting.
TEST_F(OverlaySchedulerTest, SwitchWebStateForQueuedOverlays) {
  // Add two WebStates and activate the second.
  web_state_list()->InsertWebState(0, base::MakeUnique<web::TestWebState>(),
                                   WebStateList::INSERT_FORCE_INDEX,
                                   WebStateOpener());
  ASSERT_EQ(web_state_list()->count(), 1);
  web_state_list()->InsertWebState(1, base::MakeUnique<web::TestWebState>(),
                                   WebStateList::INSERT_FORCE_INDEX,
                                   WebStateOpener());
  web_state_list()->ActivateWebStateAt(1);
  // Add an overlay to the queue corresponding with the first WebState.
  WebStateOverlayQueue* queue_0 =
      WebStateOverlayQueue::FromWebState(web_state_list()->GetWebStateAt(0));
  ASSERT_TRUE(queue_0);
  TestOverlayParentCoordinator* parent_0 =
      [[TestOverlayParentCoordinator alloc] init];
  queue_0->SetWebStateParentCoordinator(parent_0);
  TestOverlayCoordinator* overlay_0 = [[TestOverlayCoordinator alloc] init];
  queue_0->AddWebStateOverlay(overlay_0);
  EXPECT_EQ(parent_0.presentedOverlay, overlay_0);
  EXPECT_TRUE(scheduler()->IsShowingOverlay());
  EXPECT_EQ(observer()->active_index(), 0);
  // Add an overlay to the queue corresponding with the second WebState.
  WebStateOverlayQueue* queue_1 =
      WebStateOverlayQueue::FromWebState(web_state_list()->GetWebStateAt(1));
  ASSERT_TRUE(queue_1);
  TestOverlayParentCoordinator* parent_1 =
      [[TestOverlayParentCoordinator alloc] init];
  queue_1->SetWebStateParentCoordinator(parent_1);
  TestOverlayCoordinator* overlay_1 = [[TestOverlayCoordinator alloc] init];
  queue_1->AddWebStateOverlay(overlay_1);
  // Stop the first overlay and verify that the second overlay has been
  // presented and the active index updated.
  [overlay_0 stop];
  EXPECT_EQ(parent_1.presentedOverlay, overlay_1);
  EXPECT_TRUE(scheduler()->IsShowingOverlay());
  EXPECT_EQ(observer()->active_index(), 1);
  EXPECT_EQ(parent_0.presentedOverlay, nil);
  [overlay_1 stop];
  EXPECT_EQ(parent_1.presentedOverlay, nil);
  EXPECT_FALSE(scheduler()->IsShowingOverlay());
}
