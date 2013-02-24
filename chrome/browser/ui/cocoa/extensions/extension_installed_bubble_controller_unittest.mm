// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#import "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/extensions/extension_installed_bubble_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "webkit/glue/image_decoder.h"

using extensions::Extension;

// ExtensionInstalledBubbleController with removePageActionPreview overridden
// to a no-op, because pageActions are not yet hooked up in the test browser.
@interface ExtensionInstalledBubbleControllerForTest :
    ExtensionInstalledBubbleController {
}

// Do nothing, because browser window is not set up with page actions
// for unit testing.
- (void)removePageActionPreview;

@end

@implementation ExtensionInstalledBubbleControllerForTest

- (void)removePageActionPreview { }

@end

namespace keys = extension_manifest_keys;

class ExtensionInstalledBubbleControllerTest : public CocoaProfileTest {

 public:
  virtual void SetUp() {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());
    window_ = browser()->window()->GetNativeWindow();
    icon_ = LoadTestIcon();
    CommandLine command_line(CommandLine::NO_PROGRAM);
    extension_service_ = static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile()))->CreateExtensionService(
            &command_line, base::FilePath(), false);
  }

  // Load test icon from extension test directory.
  SkBitmap LoadTestIcon() {
    base::FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions").AppendASCII("icon1.png");

    std::string file_contents;
    file_util::ReadFileToString(path, &file_contents);
    const unsigned char* data =
        reinterpret_cast<const unsigned char*>(file_contents.data());

    SkBitmap bitmap;
    webkit_glue::ImageDecoder decoder;
    bitmap = decoder.Decode(data, file_contents.length());

    return bitmap;
  }

  // Create a skeletal framework of either page action or browser action
  // type.  This extension only needs to have a type and a name to initialize
  // the ExtensionInstalledBubble for unit testing.
  scoped_refptr<Extension> CreateExtension(
        extension_installed_bubble::ExtensionType type) {
    base::FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions").AppendASCII("dummy");

    DictionaryValue extension_input_value;
    extension_input_value.SetString(keys::kVersion, "1.0.0.0");
    if (type == extension_installed_bubble::kPageAction) {
      extension_input_value.SetString(keys::kName, "page action extension");
      DictionaryValue* action = new DictionaryValue;
      action->SetString(keys::kPageActionId, "ExtensionActionId");
      action->SetString(keys::kPageActionDefaultTitle, "ExtensionActionTitle");
      action->SetString(keys::kPageActionDefaultIcon, "image1.png");
      ListValue* action_list = new ListValue;
      action_list->Append(action);
      extension_input_value.Set(keys::kPageActions, action_list);
    } else if (type == extension_installed_bubble::kBrowserAction) {
      extension_input_value.SetString(keys::kName, "browser action extension");
      DictionaryValue* browser_action = new DictionaryValue;
      // An empty dictionary is enough to create a Browser Action.
      extension_input_value.Set(keys::kBrowserAction, browser_action);
    } else if (type == extension_installed_bubble::kApp) {
      extension_input_value.SetString(keys::kName, "test app");
      extension_input_value.SetString(keys::kLaunchWebURL,
                                      "http://www.example.com");
    }

    std::string error;
    scoped_refptr<Extension> extension =
        Extension::Create(path, extensions::Manifest::INVALID_LOCATION,
                          extension_input_value, Extension::NO_FLAGS, &error);
    extension_service_->AddExtension(extension);
    return extension;
  }

  // Required to initialize the extension installed bubble.
  NSWindow* window_;  // weak, owned by CocoaProfileTest.

  ExtensionService* extension_service_;

  // Skeleton extension to be tested; reinitialized for each test.
  scoped_refptr<Extension> extension_;

  // The icon_ to be loaded into the bubble window.
  SkBitmap icon_;
};

// Confirm that window sizes are set correctly for a page action extension.
TEST_F(ExtensionInstalledBubbleControllerTest, PageActionTest) {
  extension_ = CreateExtension(extension_installed_bubble::kPageAction);
  ExtensionInstalledBubbleControllerForTest* controller =
      [[ExtensionInstalledBubbleControllerForTest alloc]
          initWithParentWindow:window_
                     extension:extension_.get()
                        bundle:NULL
                       browser:browser()
                          icon:icon_];
  EXPECT_TRUE(controller);

  // Initialize window without having to calculate tabstrip locations.
  [controller initializeWindow];
  EXPECT_TRUE([controller window]);

  int height = [controller calculateWindowHeight];
  // Height should equal the vertical padding + height of all messages.
  int correctHeight = 2 * extension_installed_bubble::kOuterVerticalMargin +
      2 * extension_installed_bubble::kInnerVerticalMargin +
      NSHeight([controller headingFrame]) +
      NSHeight([controller frameOfHowToUse]) +
      NSHeight([controller frameOfHowToManage]);
  if ([controller showSyncPromo]) {
    correctHeight += NSHeight([controller frameOfSigninPromo]) +
                     extension_installed_bubble::kInnerVerticalMargin;
  }
  EXPECT_EQ(height, correctHeight);
  [controller setMessageFrames:height];

  NSRect msg1Frame = [controller headingFrame];
  NSRect msg2Frame = [controller frameOfHowToUse];
  NSRect msg3Frame = [controller frameOfHowToManage];
  NSRect msg4Frame = [controller frameOfSigninPromo];

  int next_y = extension_installed_bubble::kOuterVerticalMargin;
  if ([controller showSyncPromo]) {
    // Bottom message should be kOuterVerticalMargin pixels above window edge.
    EXPECT_EQ(next_y, NSMinY(msg4Frame));
    next_y = NSMinY(msg4Frame) + NSHeight(msg4Frame) +
            extension_installed_bubble::kInnerVerticalMargin;
  }

  // HowToManage frame should be kInnerVerticalMargin pixels above sync promo,
  // unless sync promo is hidden, then kOuterVerticalMargin pixels above.
  EXPECT_EQ(next_y, NSMinY(msg3Frame));

  // Page action message should be kInnerVerticalMargin pixels above bottom msg.
  EXPECT_EQ(NSMinY(msg3Frame) + NSHeight(msg3Frame) +
                extension_installed_bubble::kInnerVerticalMargin,
            NSMinY(msg2Frame));

  // Top message should be kInnerVerticalMargin pixels above Page action msg.
  EXPECT_EQ(NSMinY(msg2Frame) + NSHeight(msg2Frame) +
                extension_installed_bubble::kInnerVerticalMargin,
            NSMinY(msg1Frame));

  [controller setPageActionPreviewShowing:NO];
  [controller close];
}

TEST_F(ExtensionInstalledBubbleControllerTest, BrowserActionTest) {
  extension_ = CreateExtension(extension_installed_bubble::kBrowserAction);
  ExtensionInstalledBubbleControllerForTest* controller =
      [[ExtensionInstalledBubbleControllerForTest alloc]
          initWithParentWindow:window_
                     extension:extension_.get()
                        bundle:NULL
                       browser:browser()
                          icon:icon_];
  EXPECT_TRUE(controller);

  // Initialize window without having to calculate tabstrip locations.
  [controller initializeWindow];
  EXPECT_TRUE([controller window]);

  int height = [controller calculateWindowHeight];
  // Height should equal the vertical padding + height of all messages.
  int correctHeight = 2 * extension_installed_bubble::kOuterVerticalMargin +
      2 * extension_installed_bubble::kInnerVerticalMargin +
      NSHeight([controller headingFrame]) +
      NSHeight([controller frameOfHowToUse]) +
      NSHeight([controller frameOfHowToManage]);
  if ([controller showSyncPromo]) {
    correctHeight += NSHeight([controller frameOfSigninPromo]) +
                     extension_installed_bubble::kInnerVerticalMargin;
  }
  EXPECT_EQ(height, correctHeight);
  [controller setMessageFrames:height];

  NSRect msg1Frame = [controller headingFrame];
  NSRect msg2Frame = [controller frameOfHowToUse];
  NSRect msg3Frame = [controller frameOfHowToManage];
  NSRect msg4Frame = [controller frameOfSigninPromo];

  int next_y = extension_installed_bubble::kOuterVerticalMargin;
  if ([controller showSyncPromo]) {
    // Bottom message should be kOuterVerticalMargin pixels above window edge.
    EXPECT_EQ(next_y, NSMinY(msg4Frame));
    next_y = NSMinY(msg4Frame) + NSHeight(msg4Frame) +
            extension_installed_bubble::kInnerVerticalMargin;
  }

  // HowToManage frame should be kInnerVerticalMargin pixels above sync promo,
  // unless sync promo is hidden, then kOuterVerticalMargin pixels above.
  EXPECT_EQ(next_y, NSMinY(msg3Frame));

  // Browser action message should be kInnerVerticalMargin pixels above bottom
  // msg.
  EXPECT_EQ(NSMinY(msg3Frame) + NSHeight(msg3Frame) +
                extension_installed_bubble::kInnerVerticalMargin,
            NSMinY(msg2Frame));

  // Top message should be kInnerVerticalMargin pixels above Browser action msg.
  EXPECT_EQ(NSMinY(msg2Frame) + NSHeight(msg2Frame) +
                extension_installed_bubble::kInnerVerticalMargin,
            NSMinY(msg1Frame));

  [controller close];
}

TEST_F(ExtensionInstalledBubbleControllerTest, ParentClose) {
  extension_ = CreateExtension(extension_installed_bubble::kBrowserAction);
  ExtensionInstalledBubbleControllerForTest* controller =
      [[ExtensionInstalledBubbleControllerForTest alloc]
          initWithParentWindow:window_
                     extension:extension_.get()
                        bundle:NULL
                       browser:browser()
                          icon:icon_];
  EXPECT_TRUE(controller);

  // Bring up the window and disable close animation.
  [controller showWindow:nil];
  NSWindow* bubbleWindow = [controller window];
  ASSERT_TRUE([bubbleWindow isKindOfClass:[InfoBubbleWindow class]]);
  [static_cast<InfoBubbleWindow*>(bubbleWindow) setDelayOnClose:NO];

  // Observe whether the bubble window closes.
  NSString* notification = NSWindowWillCloseNotification;
  id observer = [OCMockObject observerMock];
  [[observer expect] notificationWithName:notification object:bubbleWindow];
  [[NSNotificationCenter defaultCenter]
    addMockObserver:observer name:notification object:bubbleWindow];

  // The bubble window goes from visible to not-visible.
  EXPECT_TRUE([bubbleWindow isVisible]);
  [window_ close];
  EXPECT_FALSE([bubbleWindow isVisible]);

  [[NSNotificationCenter defaultCenter] removeObserver:observer];

  // Check that the appropriate notification was received.
  EXPECT_OCMOCK_VERIFY(observer);
}

TEST_F(ExtensionInstalledBubbleControllerTest, AppTest) {
  extension_ = CreateExtension(extension_installed_bubble::kApp);
  ExtensionInstalledBubbleControllerForTest* controller =
      [[ExtensionInstalledBubbleControllerForTest alloc]
          initWithParentWindow:window_
                     extension:extension_.get()
                        bundle:NULL
                       browser:browser()
                          icon:icon_];
  EXPECT_TRUE(controller);
  [controller initializeWindow];
  EXPECT_TRUE([controller window]);

  int height = [controller calculateWindowHeight];

  // Make sure there is always enough room for the icon and margin.
  int minHeight = extension_installed_bubble::kIconSize +
    (2 * extension_installed_bubble::kOuterVerticalMargin);
  EXPECT_GT(height, minHeight);

  // Make sure the "show me" link is visible.
  EXPECT_FALSE([[controller appInstalledShortcutLink] isHidden]);

  [controller close];
}

