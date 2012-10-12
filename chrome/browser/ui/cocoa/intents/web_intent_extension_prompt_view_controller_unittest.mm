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
  const CGFloat margin = 10;
  NSRect inner_frame = NSMakeRect(margin, margin, 100, 50);

  // Layout with empty view.
  NSSize empty_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  EXPECT_EQ(0, empty_size.width);
  EXPECT_EQ(0, empty_size.height);
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];

  // Layout with a install promp.
  chrome::MockExtensionInstallPromptDelegate delegate;
  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionInstallPrompt(extension_);
  [view_controller_ setNavigator:browser()
                        delegate:&delegate
                          prompt:prompt];
  NSSize new_size =
      [view_controller_ minimumSizeForInnerWidth:NSWidth(inner_frame)];
  EXPECT_GE(new_size.width, empty_size.width);
  EXPECT_GT(new_size.height, empty_size.height);
  inner_frame.size.height = new_size.height;
  [view_ setFrame:NSInsetRect(inner_frame, -margin, -margin)];
  [view_controller_ layoutSubviewsWithinFrame:inner_frame];
}
