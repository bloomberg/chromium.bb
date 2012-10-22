// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_prompt_test_utils.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_extension_prompt_view_controller.h"
#include "chrome/common/extensions/extension.h"

class WebIntentExtensionPromptViewControllerTest : public CocoaProfileTest {
 public:
  WebIntentExtensionPromptViewControllerTest() {
    view_controller_.reset(
        [[WebIntentExtensionPromptViewController alloc] init]);
    view_.reset([[view_controller_ view] retain]);
    [[test_window() contentView] addSubview:view_];
    extension_ = chrome::LoadInstallPromptExtension();
  }

 protected:
  scoped_nsobject<WebIntentExtensionPromptViewController> view_controller_;
  scoped_nsobject<NSView> view_;
  scoped_refptr<extensions::Extension> extension_;
};

TEST_VIEW(WebIntentExtensionPromptViewControllerTest, view_)

TEST_F(WebIntentExtensionPromptViewControllerTest, Layout) {
  // Layout with no extension install prompt set.
  [view_controller_ sizeToFitAndLayout];
  NSSize empty_size = [[view_controller_ view] bounds].size;
  EXPECT_EQ(0, empty_size.width);
  EXPECT_EQ(0, empty_size.height);

  // Layout with a install promp.
  chrome::MockExtensionInstallPromptDelegate delegate;
  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionInstallPrompt(extension_);
  [view_controller_ setNavigator:browser()
                        delegate:&delegate
                          prompt:prompt];
  [view_controller_ sizeToFitAndLayout];
  NSSize new_size = [[view_controller_ view] bounds].size;
  NSRect extension_rect = [[[view_controller_ viewController] view] frame];
  EXPECT_EQ(0, NSMinX(extension_rect));
  EXPECT_EQ(0, NSMinY(extension_rect));
  // The intent view controller should be the same size as the extension
  // install prompt.
  EXPECT_EQ(NSWidth(extension_rect), new_size.width);
  EXPECT_EQ(NSHeight(extension_rect), new_size.height);
}
