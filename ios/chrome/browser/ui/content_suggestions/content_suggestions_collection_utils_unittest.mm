// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/test/base/scoped_block_swizzler.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace content_suggestions {

class ContentSuggestionsCollectionUtilsTest : public PlatformTest {
 public:
  void SetAsIPad() {
    device_type_swizzler_ = base::MakeUnique<ScopedBlockSwizzler>(
        [UIDevice class], @selector(userInterfaceIdiom),
        ^UIUserInterfaceIdiom(id self) {
          return UIUserInterfaceIdiomPad;
        });
  }
  void SetAsIPhone() {
    device_type_swizzler_ = base::MakeUnique<ScopedBlockSwizzler>(
        [UIDevice class], @selector(userInterfaceIdiom),
        ^UIUserInterfaceIdiom(id self) {
          return UIUserInterfaceIdiomPhone;
        });
  }

 private:
  std::unique_ptr<ScopedBlockSwizzler> device_type_swizzler_;
};

TEST_F(ContentSuggestionsCollectionUtilsTest, orientationFramePortrait) {
  // Setup.
  CGRect rect1 = CGRectMake(10, 10, 0, 10);
  CGRect rect2 = CGRectMake(20, 20, 0, 20);
  const CGRect rects[2] = {rect1, rect2};

  // Action.
  CGRect result = getOrientationFrame(rects, 400);

  // Tests.
  rect1.size.width = 380;
  EXPECT_TRUE(CGRectEqualToRect(rect1, result));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, orientationFrameLandscapeIPad) {
  // Setup.
  SetAsIPad();
  CGRect rect1 = CGRectMake(10, 10, 0, 10);
  CGRect rect2 = CGRectMake(20, 20, 0, 20);
  const CGRect rects[2] = {rect1, rect2};
  std::unique_ptr<ScopedBlockSwizzler> orientation_swizzler =
      base::MakeUnique<ScopedBlockSwizzler>(
          [UIApplication class], @selector(statusBarOrientation),
          ^UIInterfaceOrientation(id self) {
            return UIInterfaceOrientationLandscapeLeft;
          });

  // Action.
  CGRect result = getOrientationFrame(rects, 400);

  // Tests.
  rect1.size.width = 380;
  EXPECT_TRUE(CGRectEqualToRect(rect1, result));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, orientationFrameLandscapeIPhone) {
  // Setup.
  SetAsIPhone();
  CGRect rect1 = CGRectMake(10, 10, 0, 10);
  CGRect rect2 = CGRectMake(20, 20, 0, 20);
  const CGRect rects[2] = {rect1, rect2};
  std::unique_ptr<ScopedBlockSwizzler> orientation_swizzler =
      base::MakeUnique<ScopedBlockSwizzler>(
          [UIApplication class], @selector(statusBarOrientation),
          ^UIInterfaceOrientation(id self) {
            return UIInterfaceOrientationLandscapeLeft;
          });

  // Action.
  CGRect result = getOrientationFrame(rects, 400);

  // Tests.
  rect2.size.width = 360;
  EXPECT_TRUE(CGRectEqualToRect(rect2, result));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, centeredTilesMarginIPhone6) {
  // Setup.
  SetAsIPhone();

  // Action.
  CGFloat result = centeredTilesMarginForWidth(374);

  // Tests.
  EXPECT_EQ(17, result);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, centeredTilesMarginIPad) {
  // Setup.
  SetAsIPad();

  // Action.
  CGFloat result = centeredTilesMarginForWidth(700);

  // Tests.
  EXPECT_EQ(168, result);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPad) {
  // Setup.
  SetAsIPad();

  // Action.
  CGRect result = doodleFrame(500, YES);

  // Test.
  EXPECT_TRUE(CGRectEqualToRect(CGRectMake(0, 82, 500, 120), result));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhone) {
  // Setup.
  SetAsIPhone();

  // Action.
  CGRect result = doodleFrame(500, YES);

  // Test.
  EXPECT_TRUE(CGRectEqualToRect(CGRectMake(0, 66, 500, 120), result));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPad) {
  // Setup.
  SetAsIPad();
  CGFloat width = 500;
  CGFloat margin = centeredTilesMarginForWidth(width);

  // Action.
  CGRect result = searchFieldFrame(width, YES);

  // Test.
  EXPECT_TRUE(
      CGRectEqualToRect(CGRectMake(margin, 284, 500 - 2 * margin, 50), result));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhone) {
  // Setup.
  SetAsIPhone();
  CGFloat width = 500;
  CGFloat margin = centeredTilesMarginForWidth(width);

  // Action.
  CGRect result = searchFieldFrame(width, YES);

  // Test.
  EXPECT_TRUE(
      CGRectEqualToRect(CGRectMake(margin, 218, 500 - 2 * margin, 50), result));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPad) {
  // Setup.
  SetAsIPad();

  // Action, tests.
  EXPECT_EQ(350, heightForLogoHeader(500, YES, YES));
  EXPECT_EQ(406, heightForLogoHeader(500, YES, NO));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPhone) {
  // Setup.
  SetAsIPhone();

  // Action, tests.
  EXPECT_EQ(284, heightForLogoHeader(500, YES, YES));
  EXPECT_EQ(284, heightForLogoHeader(500, YES, NO));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, SizeIPhone6) {
  // Setup.
  SetAsIPhone();

  // Test.
  EXPECT_EQ(4U, numberOfTilesForWidth(360));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, SizeIPhone5) {
  // Setup.
  SetAsIPhone();

  // Test.
  EXPECT_EQ(3U, numberOfTilesForWidth(320));
}

// Test for iPad portrait and iPhone landscape.
TEST_F(ContentSuggestionsCollectionUtilsTest, SizeLarge) {
  // Test.
  EXPECT_EQ(4U, numberOfTilesForWidth(720));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, SizeIPadSplit) {
  // Setup.
  SetAsIPad();

  // Test.
  EXPECT_EQ(3U, numberOfTilesForWidth(360));
}

}  // namespace content_suggestions
