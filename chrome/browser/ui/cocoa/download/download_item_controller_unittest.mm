// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/download/download_item_controller.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#import "chrome/browser/ui/cocoa/download/download_item_button.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#include "content/public/test/mock_download_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using ::testing::Return;
using ::testing::ReturnRefOfCopy;

@interface DownloadItemController (DownloadItemControllerTest)
- (void)verifyProgressViewIsVisible:(bool)visible;
- (void)verifyDangerousDownloadPromptIsVisible:(bool)visible;
@end

@implementation DownloadItemController (DownloadItemControllerTest)
- (void)verifyProgressViewIsVisible:(bool)visible {
  EXPECT_EQ(visible, ![progressView_ isHidden]);
}

- (void)verifyDangerousDownloadPromptIsVisible:(bool)visible {
  EXPECT_EQ(visible, ![dangerousDownloadView_ isHidden]);
}
@end

@interface DownloadItemButton(DownloadItemButtonTest)

- (BOOL)showingContextMenu;

@end

namespace {

class DownloadItemControllerTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    id shelf_controller =
        [OCMockObject mockForClass:[DownloadShelfController class]];
    shelf_.reset([shelf_controller retain]);

    download_item_.reset(new ::testing::NiceMock<content::MockDownloadItem>);
    ON_CALL(*download_item_, GetState())
        .WillByDefault(Return(content::DownloadItem::IN_PROGRESS));
    ON_CALL(*download_item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath()));
    ON_CALL(*download_item_, GetDangerType())
        .WillByDefault(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    ON_CALL(*download_item_, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(base::FilePath()));
    ON_CALL(*download_item_, GetLastReason())
        .WillByDefault(Return(content::DOWNLOAD_INTERRUPT_REASON_NONE));
    ON_CALL(*download_item_, GetURL()).WillByDefault(ReturnRefOfCopy(GURL()));
    ON_CALL(*download_item_, GetReferrerUrl())
        .WillByDefault(ReturnRefOfCopy(GURL()));
    ON_CALL(*download_item_, GetTargetDisposition()).WillByDefault(
        Return(content::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
  }

  void TearDown() override {
    download_item_.reset();
    [(id)shelf_ verify];
    CocoaProfileTest::TearDown();
  }

  DownloadItemController* CreateItemController() {
    // In OSX 10.10, the owner of a nib file is retain/autoreleased during the
    // initialization of the nib. Wrapping the constructor in an
    // autoreleasepool ensures that tests can control the destruction timing of
    // the DownloadItemController.
    @autoreleasepool {
      base::RunLoop run_loop;
      base::scoped_nsobject<DownloadItemController> item(
          [[DownloadItemController alloc] initWithDownload:download_item_.get()
                                                     shelf:shelf_.get()
                                                 navigator:NULL]);

      [[test_window() contentView] addSubview:[item view]];
      run_loop.RunUntilIdle();
      return item.release();
    }
  }

 protected:
  std::unique_ptr<content::MockDownloadItem> download_item_;
  base::scoped_nsobject<DownloadShelfController> shelf_;
};

TEST_F(DownloadItemControllerTest, ShelfNotifiedOfOpenedDownload) {
  base::scoped_nsobject<DownloadItemController> item(CreateItemController());
  [[(id)shelf_ expect] downloadWasOpened:[OCMArg any]];
  download_item_->NotifyObserversDownloadOpened();
}

TEST_F(DownloadItemControllerTest, RemovesSelfWhenDownloadIsDestroyed) {
  base::scoped_nsobject<DownloadItemController> item(CreateItemController());
  [[(id)shelf_ expect] remove:[OCMArg any]];
  download_item_.reset();
}

TEST_F(DownloadItemControllerTest, NormalDownload) {
  base::scoped_nsobject<DownloadItemController> item(CreateItemController());

  [item verifyProgressViewIsVisible:true];
  [item verifyDangerousDownloadPromptIsVisible:false];
}

TEST_F(DownloadItemControllerTest, DangerousDownload) {
  [[(id)shelf_ expect] layoutItems];
  ON_CALL(*download_item_, GetDangerType())
      .WillByDefault(Return(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  ON_CALL(*download_item_, IsDangerous()).WillByDefault(Return(true));
  base::scoped_nsobject<DownloadItemController> item(CreateItemController());

  [item verifyProgressViewIsVisible:false];
  [item verifyDangerousDownloadPromptIsVisible:true];
}

TEST_F(DownloadItemControllerTest, NormalDownloadBecomesDangerous) {
  base::scoped_nsobject<DownloadItemController> item(CreateItemController());

  [item verifyProgressViewIsVisible:true];
  [item verifyDangerousDownloadPromptIsVisible:false];

  // The download is now marked as dangerous.
  [[(id)shelf_ expect] layoutItems];
  ON_CALL(*download_item_, GetDangerType())
      .WillByDefault(Return(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
  ON_CALL(*download_item_, IsDangerous()).WillByDefault(Return(true));
  download_item_->NotifyObserversDownloadUpdated();

  [item verifyProgressViewIsVisible:false];
  [item verifyDangerousDownloadPromptIsVisible:true];
  [(id)shelf_ verify];

  // And then marked as safe again.
  [[(id)shelf_ expect] layoutItems];
  ON_CALL(*download_item_, GetDangerType())
      .WillByDefault(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  ON_CALL(*download_item_, IsDangerous()).WillByDefault(Return(false));
  download_item_->NotifyObserversDownloadUpdated();

  [item verifyProgressViewIsVisible:true];
  [item verifyDangerousDownloadPromptIsVisible:false];
  [(id)shelf_ verify];
}

TEST_F(DownloadItemControllerTest, DismissesContextMenuWhenRemovedFromWindow) {
  base::scoped_nsobject<DownloadItemController> item(CreateItemController());
  DownloadItemButton* downloadItemButton = nil;
  for (NSView *nextSubview in [[item view] subviews]) {
    if ([nextSubview isKindOfClass:[DownloadItemButton class]]) {
      downloadItemButton = static_cast<DownloadItemButton *>(nextSubview);
      break;
    }
  }

  // showContextMenu: calls [NSMenu popUpContextMenu:...], which blocks until
  // the menu is dismissed. Use a block to cancel the menu while waiting for
  // [NSMenu popUpContextMenu:...] to return (this block will execute on the
  // main thread, on the next pass through the run loop).
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    EXPECT_TRUE([downloadItemButton showingContextMenu]);
    // Simulate the item's removal from the shelf. Ideally we would call an
    // actual shelf removal method like [item remove] but the shelf and
    // download item are mock objects.
    [downloadItemButton removeFromSuperview];
  }];
  // The unit test will stop here until the block causes the DownloadItemButton
  // to dismiss the menu.
  [item showContextMenu:nil];
}


}  // namespace
