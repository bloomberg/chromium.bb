// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tools_menu/tools_menu_context.h"

#import "base/ios/weak_nsobject.h"
#import "base/logging.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notifier.h"

@implementation ToolsMenuContext {
  base::WeakNSObject<UIView> _displayView;
  base::WeakNSObject<UIButton> _toolsMenuButton;
  base::WeakNSObject<ReadingListMenuNotifier> _readingListMenuNotifier;
}

@synthesize inTabSwitcher = _inTabSwitcher;
@synthesize noOpenedTabs = _noOpenedTabs;
@synthesize inIncognito = _inIncognito;

- (instancetype)initWithDisplayView:(UIView*)displayView {
  if (self = [super init]) {
    _displayView.reset(displayView);
    _readingListMenuNotifier.reset();
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (UIEdgeInsets)toolsButtonInsets {
  return self.toolsMenuButton ? [self.toolsMenuButton imageEdgeInsets]
                              : UIEdgeInsetsZero;
}

- (CGRect)sourceRect {
  // Set the origin for the tools popup to the horizontal center of the tools
  // menu button.
  return self.toolsMenuButton
             ? [self.displayView convertRect:self.toolsMenuButton.bounds
                                    fromView:self.toolsMenuButton]
             : CGRectZero;
}

- (void)setToolsMenuButton:(UIButton*)toolsMenuButton {
  _toolsMenuButton.reset(toolsMenuButton);
}

- (UIButton*)toolsMenuButton {
  return _toolsMenuButton;
}

- (UIView*)displayView {
  return _displayView;
}

- (void)setReadingListMenuNotifier:
    (ReadingListMenuNotifier*)readingListMenuNotifier {
  _readingListMenuNotifier.reset(readingListMenuNotifier);
}

- (ReadingListMenuNotifier*)readingListMenuNotifier {
  return _readingListMenuNotifier;
}

@end
