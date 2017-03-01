// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/test_web_state_observer.h"

#include "ios/web/public/web_state/web_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web {

void TestWebStateObserver::ProvisionalNavigationStarted(const GURL&) {
  provisional_navigation_started_called_ = true;
}

void TestWebStateObserver::NavigationItemsPruned(size_t) {
  navigation_items_pruned_called_ = true;
}

void TestWebStateObserver::NavigationItemChanged() {
  navigation_item_changed_called_ = true;
}

void TestWebStateObserver::NavigationItemCommitted(
    const LoadCommittedDetails& load_details) {
  navigation_item_committed_called_ = true;
}

void TestWebStateObserver::DidFinishNavigation(NavigationContext*) {
  did_finish_navigation_called_ = true;
}

void TestWebStateObserver::PageLoaded(PageLoadCompletionStatus status) {
  page_loaded_called_with_success_ =
      status == PageLoadCompletionStatus::SUCCESS;
}

void TestWebStateObserver::TitleWasSet() {
  title_was_set_called_ = true;
}

void TestWebStateObserver::WebStateDestroyed() {
  EXPECT_TRUE(web_state()->IsBeingDestroyed());
  web_state_destroyed_called_ = true;
  Observe(nullptr);
}

}  // namespace web
