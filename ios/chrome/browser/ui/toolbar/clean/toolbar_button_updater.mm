// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"

#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarButtonUpdater ()

// Keeps track of whether or not the back button's images have been reversed.
@property(nonatomic, assign) ToolbarButtonMode backButtonMode;

// Keeps track of whether or not the forward button's images have been
// reversed.
@property(nonatomic, assign) ToolbarButtonMode forwardButtonMode;

@end

@implementation ToolbarButtonUpdater

@synthesize backButton = _backButton;
@synthesize forwardButton = _forwardButton;
@synthesize backButtonMode = _backButtonMode;
@synthesize forwardButtonMode = _forwardButtonMode;

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    _backButtonMode = ToolbarButtonModeNormal;
    _forwardButtonMode = ToolbarButtonModeNormal;
  }
  return self;
}

#pragma mark - TabHistoryPositioner

- (CGPoint)originPointForToolbarButton:(ToolbarButtonType)toolbarButton {
  UIButton* historyButton =
      toolbarButton ? self.backButton : self.forwardButton;

  // Set the origin for the tools popup to the leading side of the bottom of the
  // pressed buttons.
  CGRect buttonBounds = [historyButton.imageView bounds];
  CGPoint leadingBottomCorner = CGPointMake(CGRectGetLeadingEdge(buttonBounds),
                                            CGRectGetMaxY(buttonBounds));
  CGPoint origin = [historyButton.imageView convertPoint:leadingBottomCorner
                                                  toView:historyButton.window];
  return origin;
}

#pragma mark - TabHistoryUIUpdater

- (void)updateUIForTabHistoryPresentationFrom:(ToolbarButtonType)button {
  UIButton* historyButton = button ? self.backButton : self.forwardButton;
  // Keep the button pressed by swapping the normal and highlighted images.
  [self setImagesForNavButton:historyButton withTabHistoryVisible:YES];
}

- (void)updateUIForTabHistoryWasDismissed {
  [self setImagesForNavButton:self.backButton withTabHistoryVisible:NO];
  [self setImagesForNavButton:self.forwardButton withTabHistoryVisible:NO];
}

#pragma mark - Private

- (void)setImagesForNavButton:(UIButton*)button
        withTabHistoryVisible:(BOOL)tabHistoryVisible {
  BOOL isBackButton = button == self.backButton;
  ToolbarButtonMode newMode =
      tabHistoryVisible ? ToolbarButtonModeReversed : ToolbarButtonModeNormal;
  if (isBackButton && newMode == self.backButtonMode)
    return;
  if (!isBackButton && newMode == self.forwardButtonMode)
    return;

  UIImage* normalImage = [button imageForState:UIControlStateNormal];
  UIImage* highlightedImage = [button imageForState:UIControlStateHighlighted];
  [button setImage:highlightedImage forState:UIControlStateNormal];
  [button setImage:normalImage forState:UIControlStateHighlighted];
  if (isBackButton)
    self.backButtonMode = newMode;
  else
    self.forwardButtonMode = newMode;
}

@end
