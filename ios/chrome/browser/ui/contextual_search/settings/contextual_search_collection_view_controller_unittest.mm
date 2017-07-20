// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/contextual_search/settings/contextual_search_collection_view_controller.h"

#include <memory>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/contextual_search/touch_to_search_permissions_mediator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ContextualSearchCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  ContextualSearchCollectionViewControllerTest() {
    TestChromeBrowserState::Builder browserStateBuilder;
    browser_state_ = browserStateBuilder.Build();
    tts_permissions_ = [[TouchToSearchPermissionsMediator alloc]
        initWithBrowserState:browser_state_.get()];
  }

  CollectionViewController* InstantiateController() override {
    return [[ContextualSearchCollectionViewController alloc]
        initWithPermissions:tts_permissions_];
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  TouchToSearchPermissionsMediator* tts_permissions_;
};

TEST_F(ContextualSearchCollectionViewControllerTest,
       TestModelContextualSearchOff) {
  [tts_permissions_ setPreferenceState:TouchToSearch::DISABLED];
  CreateController();
  CheckController();
  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTitleWithId(NO, IDS_IOS_CONTEXTUAL_SEARCH_TITLE, 0, 0);
}

TEST_F(ContextualSearchCollectionViewControllerTest,
       TestModelContextualSearchOn) {
  [tts_permissions_ setPreferenceState:TouchToSearch::ENABLED];
  CreateController();
  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTitleWithId(YES, IDS_IOS_CONTEXTUAL_SEARCH_TITLE, 0,
                                     0);
}

TEST_F(ContextualSearchCollectionViewControllerTest,
       TestModelContextualSearchUndecided) {
  [tts_permissions_ setPreferenceState:TouchToSearch::UNDECIDED];
  CreateController();
  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckSwitchCellStateAndTitleWithId(YES, IDS_IOS_CONTEXTUAL_SEARCH_TITLE, 0,
                                     0);
}

}  // namespace
