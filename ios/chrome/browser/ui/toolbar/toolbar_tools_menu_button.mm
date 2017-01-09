// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_tools_menu_button.h"

#include "ios/chrome/browser/ui/toolbar/toolbar_button_tints.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

@interface ToolbarToolsMenuButton ()
// Updates the tint configuration based on the button's situation, e.g. whether
// the tools menu is visible or not.
- (void)updateTintOfButton;
@end

@interface ToolbarToolsMenuButton () {
  // The style of the toolbar the button is in.
  ToolbarControllerStyle style_;
  // Whether the tools menu is visible.
  BOOL toolsMenuVisible_;
  // Whether the reading list contains unseen items.
  BOOL readingListContainsUnseenItems_;
}
@end

@implementation ToolbarToolsMenuButton

- (instancetype)initWithFrame:(CGRect)frame
                        style:(ToolbarControllerStyle)style {
  if (self = [super initWithFrame:frame]) {
    style_ = style;

    [self setTintColor:toolbar::NormalButtonTint(style_)
              forState:UIControlStateNormal];
    [self setTintColor:toolbar::HighlighButtonTint(style_)
              forState:UIControlStateHighlighted];

    [self setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -3, 0, 0)];
    UIImage* image = [UIImage imageNamed:@"toolbar_tools"];
    image = [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [self setImage:image forState:UIControlStateNormal];
  }
  return self;
}

- (void)setToolsMenuIsVisible:(BOOL)toolsMenuVisible {
  toolsMenuVisible_ = toolsMenuVisible;
  [self updateTintOfButton];
}

- (void)setReadingListContainsUnseenItems:(BOOL)readingListContainsUnseenItems {
  readingListContainsUnseenItems_ = readingListContainsUnseenItems;
  [self updateTintOfButton];
}

#pragma mark - Private

- (void)updateTintOfButton {
  if (toolsMenuVisible_ || readingListContainsUnseenItems_) {
    [self setTintColor:toolbar::HighlighButtonTint(style_)
              forState:UIControlStateNormal];
  } else {
    [self setTintColor:toolbar::NormalButtonTint(style_)
              forState:UIControlStateNormal];
  }
}

@end
