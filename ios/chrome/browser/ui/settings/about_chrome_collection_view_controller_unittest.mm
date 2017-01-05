// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/about_chrome_collection_view_controller.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// An AboutChromeTableViewController that intercepts calls to
// |chromeExecuteCommand:| in order to test the commands.
@interface TestAboutChromeCollectionViewController
    : AboutChromeCollectionViewController

// The intercepted chrome command.
@property(nonatomic, readonly) OpenUrlCommand* command;

@end

@implementation TestAboutChromeCollectionViewController {
  base::scoped_nsobject<OpenUrlCommand> command_;
}

- (IBAction)chromeExecuteCommand:(id)sender {
  DCHECK([sender isKindOfClass:[OpenUrlCommand class]]);
  command_.reset([static_cast<OpenUrlCommand*>(sender) retain]);
}

- (OpenUrlCommand*)command {
  return command_;
}

@end

namespace {

class AboutChromeCollectionViewControllerTest
    : public CollectionViewControllerTest {
 public:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    return [[TestAboutChromeCollectionViewController alloc] init];
  }
};

TEST_F(AboutChromeCollectionViewControllerTest, TestModel) {
  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  CheckTextCellTitleWithId(IDS_IOS_OPEN_SOURCE_LICENSES, 0, 0);
  CheckTextCellTitleWithId(IDS_IOS_TERMS_OF_SERVICE, 0, 1);
  CheckTextCellTitleWithId(IDS_IOS_PRIVACY_POLICY, 0, 2);
}

TEST_F(AboutChromeCollectionViewControllerTest, TestOpenUrls) {
  CreateController();

  TestAboutChromeCollectionViewController* about_chrome_controller =
      static_cast<TestAboutChromeCollectionViewController*>(controller());
  [about_chrome_controller
                collectionView:about_chrome_controller.collectionView
      didSelectItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  EXPECT_TRUE([about_chrome_controller command]);
  EXPECT_EQ(GURL(kChromeUICreditsURL), [about_chrome_controller command].url);

  [about_chrome_controller
                collectionView:about_chrome_controller.collectionView
      didSelectItemAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]];
  EXPECT_TRUE([about_chrome_controller command]);
  EXPECT_EQ(GURL(kChromeUITermsURL), [about_chrome_controller command].url);

  [about_chrome_controller
                collectionView:about_chrome_controller.collectionView
      didSelectItemAtIndexPath:[NSIndexPath indexPathForItem:2 inSection:0]];
  EXPECT_TRUE([about_chrome_controller command]);
  EXPECT_EQ(GURL(l10n_util::GetStringUTF8(IDS_IOS_PRIVACY_POLICY_URL)),
            [about_chrome_controller command].url);
}

}  // namespace
