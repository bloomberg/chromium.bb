// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#import "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/extensions/extension_installed_bubble_controller.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "webkit/glue/image_decoder.h"

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

class ExtensionInstalledBubbleControllerTest : public CocoaTest {

 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    browser_ = helper_.browser();
    window_ = helper_.CreateBrowserWindow()->GetNativeHandle();
    icon_ = LoadTestIcon();
  }

  virtual void TearDown() {
    helper_.CloseBrowserWindow();
    CocoaTest::TearDown();
  }

  // Load test icon from extension test directory.
  SkBitmap LoadTestIcon() {
    FilePath path;
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
    FilePath path;
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
    } else {
      extension_input_value.SetString(keys::kName, "browser action extension");
      DictionaryValue* browser_action = new DictionaryValue;
      // An empty dictionary is enough to create a Browser Action.
      extension_input_value.Set(keys::kBrowserAction, browser_action);
    }

    std::string error;
    return Extension::Create(path, Extension::INVALID, extension_input_value,
                             Extension::STRICT_ERROR_CHECKS, &error);
  }

  // Allows us to create the window and browser for testing.
  BrowserTestHelper helper_;

  // Required to initialize the extension installed bubble.
  NSWindow* window_;  // weak, owned by BrowserTestHelper.

  // Required to initialize the extension installed bubble.
  Browser* browser_;  // weak, owned by BrowserTestHelper.

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
                       browser:browser_
                          icon:icon_];
  EXPECT_TRUE(controller);

  // Initialize window without having to calculate tabstrip locations.
  [controller initializeWindow];
  EXPECT_TRUE([controller window]);

  int height = [controller calculateWindowHeight];
  // Height should equal the vertical padding + height of all messages.
  int correctHeight = 2 * extension_installed_bubble::kOuterVerticalMargin +
      2 * extension_installed_bubble::kInnerVerticalMargin +
      [controller getExtensionInstalledMsgFrame].size.height +
      [controller getExtensionInstalledInfoMsgFrame].size.height +
      [controller getExtraInfoMsgFrame].size.height;
  EXPECT_EQ(height, correctHeight);

  [controller setMessageFrames:height];
  NSRect msg3Frame = [controller getExtensionInstalledInfoMsgFrame];
  // Bottom message should be kOuterVerticalMargin pixels above window edge.
  EXPECT_EQ(msg3Frame.origin.y,
      extension_installed_bubble::kOuterVerticalMargin);
  NSRect msg2Frame = [controller getExtraInfoMsgFrame];
  // Pageaction message should be kInnerVerticalMargin pixels above bottom msg.
  EXPECT_EQ(msg2Frame.origin.y,
            msg3Frame.origin.y + msg3Frame.size.height +
            extension_installed_bubble::kInnerVerticalMargin);
  NSRect msg1Frame = [controller getExtensionInstalledMsgFrame];
  // Top message should be kInnerVerticalMargin pixels above Pageaction msg.
  EXPECT_EQ(msg1Frame.origin.y,
            msg2Frame.origin.y + msg2Frame.size.height +
            extension_installed_bubble::kInnerVerticalMargin);

  [controller setPageActionRemoved:YES];
  [controller close];
}

TEST_F(ExtensionInstalledBubbleControllerTest, BrowserActionTest) {
  extension_ = CreateExtension(extension_installed_bubble::kBrowserAction);
  ExtensionInstalledBubbleControllerForTest* controller =
      [[ExtensionInstalledBubbleControllerForTest alloc]
          initWithParentWindow:window_
                     extension:extension_.get()
                       browser:browser_
                          icon:icon_];
  EXPECT_TRUE(controller);

  // Initialize window without having to calculate tabstrip locations.
  [controller initializeWindow];
  EXPECT_TRUE([controller window]);

  int height = [controller calculateWindowHeight];
  // Height should equal the vertical padding + height of all messages.
  int correctHeight = 2 * extension_installed_bubble::kOuterVerticalMargin +
      extension_installed_bubble::kInnerVerticalMargin +
      [controller getExtensionInstalledMsgFrame].size.height +
      [controller getExtensionInstalledInfoMsgFrame].size.height;
  EXPECT_EQ(height, correctHeight);

  [controller setMessageFrames:height];
  NSRect msg3Frame = [controller getExtensionInstalledInfoMsgFrame];
  // Bottom message should start kOuterVerticalMargin pixels above window edge.
  EXPECT_EQ(msg3Frame.origin.y,
      extension_installed_bubble::kOuterVerticalMargin);
  NSRect msg1Frame = [controller getExtensionInstalledMsgFrame];
  // Top message should start kInnerVerticalMargin pixels above top of
  //  extensionInstalled message, because page action message is hidden.
  EXPECT_EQ(msg1Frame.origin.y,
            msg3Frame.origin.y + msg3Frame.size.height +
                extension_installed_bubble::kInnerVerticalMargin);

  [controller close];
}
