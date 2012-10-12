// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_extension_prompt_view_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_view_controller.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"

@implementation WebIntentExtensionPromptViewController

- (id)init {
  if ((self = [super init])) {
    scoped_nsobject<NSView> view(
        [[FlippedView alloc] initWithFrame:NSZeroRect]);
    [self setView:view];
  }
  return self;
}

- (void)setNavigator:(content::PageNavigator*)navigator
            delegate:(ExtensionInstallPrompt::Delegate*)delegate
              prompt:(const ExtensionInstallPrompt::Prompt&)prompt {
  if (viewController_.get())
    return;

  viewController_.reset([[ExtensionInstallViewController alloc]
      initWithNavigator:navigator
               delegate:delegate
                 prompt:prompt]);
  [[self view] addSubview:[viewController_ view]];
}

- (void)clear {
  [[viewController_ view] removeFromSuperview];
  viewController_.reset();
}

- (ExtensionInstallViewController*)viewController {
  return viewController_;
}

- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth {
  if (!viewController_.get())
    return NSZeroSize;
  return [[viewController_ view] frame].size;
}

- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame {
  NSRect bounds = [[self view] bounds];
  NSRect viewFrame;
  viewFrame.size = [[viewController_ view] frame].size;
  viewFrame.origin.x = NSMidX(bounds) - viewFrame.size.width / 2.0;
  viewFrame.origin.y = NSMidY(bounds) - viewFrame.size.height / 2.0;
  [[viewController_ view] setFrame:viewFrame];
}

@end
