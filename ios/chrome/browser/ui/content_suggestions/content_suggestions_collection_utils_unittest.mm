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
  void SetAsPortrait() {
    orientation_swizzler_ = base::MakeUnique<ScopedBlockSwizzler>(
        [UIApplication class], @selector(statusBarOrientation),
        ^UIInterfaceOrientation(id self) {
          return UIInterfaceOrientationPortrait;
        });
  }
  void SetAsLandscape() {
    orientation_swizzler_ = base::MakeUnique<ScopedBlockSwizzler>(
        [UIApplication class], @selector(statusBarOrientation),
        ^UIInterfaceOrientation(id self) {
          return UIInterfaceOrientationLandscapeLeft;
        });
  }

 private:
  std::unique_ptr<ScopedBlockSwizzler> device_type_swizzler_;
  std::unique_ptr<ScopedBlockSwizzler> orientation_swizzler_;
};

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
  CGFloat height = doodleHeight(YES);
  CGFloat topMargin = doodleTopMargin();

  // Test.
  EXPECT_EQ(120, height);
  EXPECT_EQ(82, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhonePortrait) {
  // Setup.
  SetAsIPhone();
  SetAsPortrait();

  // Action.
  CGFloat heightLogo = doodleHeight(YES);
  CGFloat heightNoLogo = doodleHeight(NO);
  CGFloat topMargin = doodleTopMargin();

  // Test.
  EXPECT_EQ(120, heightLogo);
  EXPECT_EQ(60, heightNoLogo);
  EXPECT_EQ(56, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, doodleFrameIPhoneLandscape) {
  // Setup.
  SetAsIPhone();
  SetAsLandscape();

  // Action.
  CGFloat heightLogo = doodleHeight(YES);
  CGFloat heightNoLogo = doodleHeight(NO);
  CGFloat topMargin = doodleTopMargin();

  // Test.
  EXPECT_EQ(120, heightLogo);
  EXPECT_EQ(60, heightNoLogo);
  EXPECT_EQ(56, topMargin);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPad) {
  // Setup.
  SetAsIPad();
  CGFloat width = 500;
  CGFloat largeIPadWidth = 1366;
  CGFloat margin = centeredTilesMarginForWidth(width);

  // Action.
  CGFloat resultWidth = searchFieldWidth(width);
  CGFloat resultWidthLargeIPad = searchFieldWidth(largeIPadWidth);
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(82, topMargin);
  EXPECT_EQ(width - 2 * margin, resultWidth);
  EXPECT_EQ(largeIPadWidth - 400, resultWidthLargeIPad);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhonePortrait) {
  // Setup.
  SetAsIPhone();
  SetAsPortrait();
  CGFloat width = 500;
  CGFloat margin = centeredTilesMarginForWidth(width);

  // Action.
  CGFloat resultWidth = searchFieldWidth(width);
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(16, topMargin);
  EXPECT_EQ(width - 2 * margin, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, searchFieldFrameIPhoneLandscape) {
  // Setup.
  SetAsIPhone();
  SetAsLandscape();
  CGFloat width = 500;
  CGFloat margin = centeredTilesMarginForWidth(width);

  // Action.
  CGFloat resultWidth = searchFieldWidth(width);
  CGFloat topMargin = searchFieldTopMargin();

  // Test.
  EXPECT_EQ(16, topMargin);
  EXPECT_EQ(width - 2 * margin, resultWidth);
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPad) {
  // Setup.
  SetAsIPad();

  // Action, tests.
  EXPECT_EQ(350, heightForLogoHeader(YES, YES));
  EXPECT_EQ(406, heightForLogoHeader(YES, NO));
}

TEST_F(ContentSuggestionsCollectionUtilsTest, heightForLogoHeaderIPhone) {
  // Setup.
  SetAsIPhone();

  // Action, tests.
  EXPECT_EQ(258, heightForLogoHeader(YES, YES));
  EXPECT_EQ(258, heightForLogoHeader(YES, NO));
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
