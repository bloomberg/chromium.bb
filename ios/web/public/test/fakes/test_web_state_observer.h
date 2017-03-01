// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_

#include "ios/web/public/web_state/web_state_observer.h"

class GURL;

namespace web {

class WebState;

// Test observer to check that the WebStateObserver methods are called as
// expected.
class TestWebStateObserver : public WebStateObserver {
 public:
  TestWebStateObserver(WebState* web_state) : WebStateObserver(web_state) {}

  // Methods returning true if the corresponding WebStateObserver method has
  // been called.
  bool provisional_navigation_started_called() const {
    return provisional_navigation_started_called_;
  };
  bool navigation_items_pruned_called() const {
    return navigation_items_pruned_called_;
  }
  bool navigation_item_changed_called() const {
    return navigation_item_changed_called_;
  }
  bool navigation_item_committed_called() const {
    return navigation_item_committed_called_;
  }
  bool page_loaded_called_with_success() const {
    return page_loaded_called_with_success_;
  }
  bool history_state_changed_called() const {
    return history_state_changed_called_;
  }
  bool did_finish_navigation_called() const {
    return did_finish_navigation_called_;
  }
  bool title_was_set_called() const { return title_was_set_called_; }
  bool web_state_destroyed_called() const {
    return web_state_destroyed_called_;
  }

 private:
  // WebStateObserver implementation:
  void ProvisionalNavigationStarted(const GURL& url) override;
  void NavigationItemsPruned(size_t pruned_item_count) override;
  void NavigationItemChanged() override;
  void NavigationItemCommitted(const LoadCommittedDetails&) override;
  void DidFinishNavigation(NavigationContext* navigation_context) override;
  void PageLoaded(PageLoadCompletionStatus load_completion_status) override;
  void TitleWasSet() override;
  void WebStateDestroyed() override;

  bool provisional_navigation_started_called_ = false;
  bool navigation_items_pruned_called_ = false;
  bool navigation_item_changed_called_ = false;
  bool navigation_item_committed_called_ = false;
  bool page_loaded_called_with_success_ = false;
  bool history_state_changed_called_ = false;
  bool did_finish_navigation_called_ = false;
  bool title_was_set_called_ = false;
  bool web_state_destroyed_called_ = false;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_OBSERVER_H_
