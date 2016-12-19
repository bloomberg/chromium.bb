// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/no_tabs/no_tabs_controller.h"

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/background_generator.h"
#import "ios/chrome/browser/ui/no_tabs/no_tabs_controller_testing.h"
#import "ios/chrome/browser/ui/no_tabs/no_tabs_toolbar_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_context.h"
#include "ios/chrome/browser/ui/ui_util.h"

@interface NoTabsController (PrivateMethods)
// Creates and installs the toolbar for the No-Tabs UI.
- (void)installNoTabsToolbarInView:(UIView*)view;
// Creates and installs the background view for the No-Tabs UI.
- (void)installNoTabsBackgroundInView:(UIView*)view;
// Returns the frame of a button's image view.
- (CGRect)imageFrameForButton:(UIButton*)button inView:(UIView*)view;
// Creates and installs the UIImageView that is used to animate the mode toggle
// button.
- (void)installNoTabsAnimationToggleImageForButton:(UIButton*)button
                                            inView:(UIView*)view;
@end

@implementation NoTabsController {
  // The No-Tabs toolbar.
  base::scoped_nsobject<NoTabsToolbarController> toolbarController_;

  // The parent view of the No-Tabs UI. Must outlive this object.
  UIView* parentView_;  // weak

  // The background view behind the No-Tabs UI.
  base::scoped_nsobject<UIView> backgroundView_;

  // The image view used to animate the mode toggle button when entering or
  // leaving the No-Tabs UI.
  base::scoped_nsobject<UIImageView> modeToggleImageView_;

  // The ending frame for the mode toggle button animation.  Equal to CGRectZero
  // if the mode toggle button is not being animated.
  CGRect toggleImageEndFrame_;

  base::mac::ObjCPropertyReleaser propertyReleaser_NoTabsController_;
}

// Designated initializer.
- (id)initWithView:(UIView*)view {
  self = [super init];
  if (self) {
    propertyReleaser_NoTabsController_.Init(self, [NoTabsController class]);

    parentView_ = view;
    [self installNoTabsToolbarInView:view];
    [self installNoTabsBackgroundInView:view];
  }
  return self;
}

// Passes the call through to the toolbar controller.
- (void)setHasModeToggleSwitch:(BOOL)hasModeToggle {
  [toolbarController_ setHasModeToggleSwitch:hasModeToggle];
}

// Prepares the given button for animation.
- (void)installAnimationImageForButton:(UIButton*)button
                                inView:(UIView*)view
                                  show:(BOOL)show {
  UIButton* fromButton = nil;
  UIButton* toButton = nil;

  if (show) {
    fromButton = button;
    toButton = [toolbarController_ modeToggleButton];
  } else {
    fromButton = [toolbarController_ modeToggleButton];
    toButton = button;
  }

  [self installNoTabsAnimationToggleImageForButton:fromButton inView:view];
  toggleImageEndFrame_ = [self imageFrameForButton:toButton inView:view];
}

- (void)showToolsMenuPopup {
  base::scoped_nsobject<ToolsMenuContext> context(
      [[ToolsMenuContext alloc] initWithDisplayView:parentView_]);
  [context setNoOpenedTabs:YES];
  [toolbarController_ showToolsMenuPopupWithContext:context];
}

- (void)dismissToolsMenuPopup {
  [toolbarController_ dismissToolsMenuPopup];
}

- (void)prepareForShowAnimation {
  CGFloat toolbarHeight = CGRectGetHeight([[toolbarController_ view] frame]);

  // The toolbar icons start offscreen and animate in.
  [toolbarController_
      setControlsTransform:CGAffineTransformMakeTranslation(0, -toolbarHeight)];
}

- (void)showNoTabsUI {
  [toolbarController_ setControlsTransform:CGAffineTransformIdentity];

  if (modeToggleImageView_.get()) {
    DCHECK(!CGRectEqualToRect(toggleImageEndFrame_, CGRectZero));
    [modeToggleImageView_ setFrame:toggleImageEndFrame_];
    toggleImageEndFrame_ = CGRectZero;
  }
}

- (void)showAnimationDidFinish {
  // When the animation is finished, remove the temporary toggle switch image.
  [modeToggleImageView_ removeFromSuperview];
  modeToggleImageView_.reset();
}

- (void)prepareForDismissAnimation {
  // Hide the toolbar mode toggle button so that it does not visibly animate
  // out.
  [[toolbarController_ modeToggleButton] setHidden:YES];
}

- (void)dismissNoTabsUI {
  CGFloat toolbarHeight = [[toolbarController_ view] frame].size.height;
  [toolbarController_
      setControlsTransform:CGAffineTransformMakeTranslation(0, -toolbarHeight)];

  if (modeToggleImageView_.get()) {
    DCHECK(!CGRectEqualToRect(toggleImageEndFrame_, CGRectZero));
    [modeToggleImageView_ setFrame:toggleImageEndFrame_];
    toggleImageEndFrame_ = CGRectZero;
  }
}

- (void)dismissAnimationDidFinish {
  [modeToggleImageView_ removeFromSuperview];
  modeToggleImageView_.reset();
  [backgroundView_ removeFromSuperview];
  backgroundView_.reset();
  [[toolbarController_ view] removeFromSuperview];
  toolbarController_.reset();
}

// Creates and installs the toolbar for the No-Tabs UI.
- (void)installNoTabsToolbarInView:(UIView*)view {
  toolbarController_.reset([[NoTabsToolbarController alloc] initWithNoTabs]);
  UIView* toolbarView = [toolbarController_ view];
  CGFloat toolbarHeight = CGRectGetHeight(toolbarView.frame);

  CGRect toolbarFrame = [view bounds];
  toolbarFrame.origin.y = StatusBarHeight();
  toolbarFrame.size.height = toolbarHeight;
  toolbarView.frame = toolbarFrame;
  [view addSubview:toolbarView];
}

// Creates and installs the background view for the No-Tabs UI.
- (void)installNoTabsBackgroundInView:(UIView*)view {
  DCHECK(toolbarController_);
  UIView* toolbarView = [toolbarController_ view];

  backgroundView_.reset([[UIView alloc] initWithFrame:view.bounds]);
  [backgroundView_ setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                        UIViewAutoresizingFlexibleHeight)];
  InstallBackgroundInView(backgroundView_);
  [view insertSubview:backgroundView_ belowSubview:toolbarView];
}

// Returns the frame of a button's image view.
- (CGRect)imageFrameForButton:(UIButton*)button inView:(UIView*)view {
  // Convert the existing button's bounds to the coordinate system of |view| and
  // trim away any padding present in the button.
  CGRect rect = UIEdgeInsetsInsetRect(button.bounds, button.imageEdgeInsets);
  return [view convertRect:rect fromView:button];
}

// Installs the UIImageView that is used to animate the mode toggle switch while
// the No-Tabs UI is shown or hidden.  This image view is initially set up to
// exactly cover the image of an existing button.
- (void)installNoTabsAnimationToggleImageForButton:(UIButton*)button
                                            inView:(UIView*)view {
  UIImage* image = [button imageForState:UIControlStateNormal];
  modeToggleImageView_.reset([[UIImageView alloc] initWithImage:image]);
  [modeToggleImageView_
      setAutoresizingMask:(UIViewAutoresizingFlexibleLeftMargin |
                           UIViewAutoresizingFlexibleTopMargin)];
  [modeToggleImageView_ setFrame:[self imageFrameForButton:button inView:view]];
  [view addSubview:modeToggleImageView_];
}

@end

#pragma mark - TestingAdditions

@implementation NoTabsController (JustForTesting)
- (UIButton*)modeToggleButton {
  return [toolbarController_ modeToggleButton];
}

- (UIView*)toolbarView {
  return [toolbarController_ view];
}

- (UIView*)backgroundView {
  return backgroundView_.get();
}
@end
