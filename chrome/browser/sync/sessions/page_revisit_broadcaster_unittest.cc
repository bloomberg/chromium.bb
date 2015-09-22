// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/page_revisit_broadcaster.h"

#include "components/sync_driver/revisit/page_visit_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace browser_sync {

class PageRevisitBroadcasterTest : public ::testing::Test {
 protected:
  sync_driver::PageVisitObserver::TransitionType Convert(
      const ui::PageTransition conversionInput) {
    return PageRevisitBroadcaster::ConvertTransitionEnum(conversionInput);
  }

  void Check(const sync_driver::PageVisitObserver::TransitionType expected,
             const ui::PageTransition conversionInput) {
    EXPECT_EQ(expected, Convert(conversionInput));
  }

  void Check(const sync_driver::PageVisitObserver::TransitionType expected,
             const int32 conversionInput) {
    Check(expected, ui::PageTransitionFromInt(conversionInput));
  }
};

TEST_F(PageRevisitBroadcasterTest, ConvertPageInteraction) {
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK);
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_BLOCKED);
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FORWARD_BACK);
  // Don't check ui::PAGE_TRANSITION_FROM_ADDRESS_BAR, this is actually a copy
  // and paste action when combined with ui::PAGE_TRANSITION_LINK.
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_HOME_PAGE);
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_API);
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_START);
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CHAIN_END);
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_SERVER_REDIRECT);
  Check(sync_driver::PageVisitObserver::kTransitionPage,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_IS_REDIRECT_MASK);
}

TEST_F(PageRevisitBroadcasterTest, ConvertOmniboxURL) {
  Check(sync_driver::PageVisitObserver::kTransitionOmniboxUrl,
        ui::PAGE_TRANSITION_TYPED);
  Check(sync_driver::PageVisitObserver::kTransitionOmniboxUrl,
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

TEST_F(PageRevisitBroadcasterTest, ConvertOmniboxDefaultSearch) {
  Check(sync_driver::PageVisitObserver::kTransitionOmniboxDefaultSearch,
        ui::PAGE_TRANSITION_GENERATED);
  Check(sync_driver::PageVisitObserver::kTransitionOmniboxDefaultSearch,
        ui::PAGE_TRANSITION_GENERATED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

TEST_F(PageRevisitBroadcasterTest, ConvertOmniboxTemplateSearch) {
  Check(sync_driver::PageVisitObserver::kTransitionOmniboxTemplateSearch,
        ui::PAGE_TRANSITION_KEYWORD);
  Check(sync_driver::PageVisitObserver::kTransitionOmniboxTemplateSearch,
        ui::PAGE_TRANSITION_KEYWORD | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  Check(sync_driver::PageVisitObserver::kTransitionOmniboxTemplateSearch,
        ui::PAGE_TRANSITION_KEYWORD_GENERATED);
  Check(sync_driver::PageVisitObserver::kTransitionOmniboxTemplateSearch,
        ui::PAGE_TRANSITION_KEYWORD_GENERATED |
            ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

TEST_F(PageRevisitBroadcasterTest, ConvertBookmark) {
  Check(sync_driver::PageVisitObserver::kTransitionBookmark,
        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
}

TEST_F(PageRevisitBroadcasterTest, ConvertCopyPaste) {
  Check(sync_driver::PageVisitObserver::kTransitionCopyPaste,
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
}

TEST_F(PageRevisitBroadcasterTest, ConvertForwardBackward) {
  Check(sync_driver::PageVisitObserver::kTransitionForwardBackward,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL | ui::PAGE_TRANSITION_FORWARD_BACK);
}

TEST_F(PageRevisitBroadcasterTest, ConvertRestore) {
  Check(sync_driver::PageVisitObserver::kTransitionRestore,
        ui::PAGE_TRANSITION_RELOAD);
}

TEST_F(PageRevisitBroadcasterTest, ConvertUnknown) {
  Check(sync_driver::PageVisitObserver::kTransitionUnknown,
        ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  Check(sync_driver::PageVisitObserver::kTransitionUnknown,
        ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  Check(sync_driver::PageVisitObserver::kTransitionUnknown,
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
}

}  // namespace browser_sync
