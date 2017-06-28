// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_utils.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ContentSuggestionsViewControllerUtilsTest : public PlatformTest {
 public:
  ContentSuggestionsViewControllerUtilsTest() {
    collection_ = OCMClassMock([UICollectionView class]);
    suggestions_view_controller_ =
        OCMClassMock([ContentSuggestionsViewController class]);
    OCMStub([suggestions_view_controller_ collectionView])
        .andReturn(collection_);
  }

  id MockSuggestionsViewController() { return suggestions_view_controller_; }

 private:
  id suggestions_view_controller_;
  id collection_;
};

TEST_F(ContentSuggestionsViewControllerUtilsTest,
       UpwardFromMiddleScreenToMiddleScreen) {
  // Setup.
  id suggestionsViewController = MockSuggestionsViewController();
  OCMExpect([suggestionsViewController setScrolledToTop:YES]);
  CGPoint targetOffset = CGPointMake(0, 80);

  // Action.
  [ContentSuggestionsViewControllerUtils
      viewControllerWillEndDragging:suggestionsViewController
                        withYOffset:10
                      pinnedYOffset:100
                     draggingUpward:YES
                targetContentOffset:&targetOffset];

  // Test.
  EXPECT_OCMOCK_VERIFY(suggestionsViewController);
  EXPECT_EQ(10, targetOffset.y);
}

TEST_F(ContentSuggestionsViewControllerUtilsTest,
       DownwardFromMiddleScreenToMiddleScreen) {
  // Setup.
  id suggestionsViewController = MockSuggestionsViewController();
  OCMExpect([suggestionsViewController setScrolledToTop:NO]);
  CGPoint targetOffset = CGPointMake(0, 20);

  // Action.
  [ContentSuggestionsViewControllerUtils
      viewControllerWillEndDragging:suggestionsViewController
                        withYOffset:10
                      pinnedYOffset:100
                     draggingUpward:NO
                targetContentOffset:&targetOffset];

  // Test.
  EXPECT_OCMOCK_VERIFY(suggestionsViewController);
  EXPECT_EQ(10, targetOffset.y);
}

TEST_F(ContentSuggestionsViewControllerUtilsTest,
       FromAboveTheOmniboxToOmnibox) {
  // Setup.
  id suggestionsViewController = MockSuggestionsViewController();
  OCMExpect([suggestionsViewController setScrolledToTop:YES]);
  CGPoint targetOffset = CGPointMake(0, 20);

  // Action.
  [ContentSuggestionsViewControllerUtils
      viewControllerWillEndDragging:suggestionsViewController
                        withYOffset:100
                      pinnedYOffset:50
                     draggingUpward:NO
                targetContentOffset:&targetOffset];

  // Test.
  EXPECT_OCMOCK_VERIFY(suggestionsViewController);
  EXPECT_EQ(100, targetOffset.y);
}

TEST_F(ContentSuggestionsViewControllerUtilsTest, EndAboveOmnibox) {
  // Setup.
  id suggestionsViewController = MockSuggestionsViewController();
  OCMExpect([suggestionsViewController setScrolledToTop:YES]);
  CGPoint targetOffset = CGPointMake(0, 50);

  // Action.
  [ContentSuggestionsViewControllerUtils
      viewControllerWillEndDragging:suggestionsViewController
                        withYOffset:100
                      pinnedYOffset:20
                     draggingUpward:NO
                targetContentOffset:&targetOffset];

  // Test.
  EXPECT_OCMOCK_VERIFY(suggestionsViewController);
  EXPECT_EQ(50, targetOffset.y);
}

TEST_F(ContentSuggestionsViewControllerUtilsTest, Overscroll) {
  // Setup.
  id suggestionsViewController = MockSuggestionsViewController();
  OCMExpect([suggestionsViewController setScrolledToTop:NO]);
  CGPoint targetOffset = CGPointMake(0, 50);

  // Action.
  [ContentSuggestionsViewControllerUtils
      viewControllerWillEndDragging:suggestionsViewController
                        withYOffset:-100
                      pinnedYOffset:20
                     draggingUpward:NO
                targetContentOffset:&targetOffset];

  // Test.
  EXPECT_OCMOCK_VERIFY(suggestionsViewController);
  EXPECT_EQ(50, targetOffset.y);
}
}
