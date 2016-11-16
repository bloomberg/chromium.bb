// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_mouse_tracker.h"

#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#include "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/tracking_area.h"

namespace {

// Additional height threshold added at the toolbar's bottom. This is to mimic
// threshold the mouse position needs to be at before the menubar automatically
// hides.
const CGFloat kTrackingAreaAdditionalThreshold = 20;

}  // namespace

@interface FullscreenToolbarMouseTracker () {
  // The frame for the tracking area. The value is the toolbar's frame with
  // additional height added at the bottom.
  NSRect trackingAreaFrame_;

  // The tracking area associated with the toolbar. This tracking area is used
  // to keep the toolbar active if the menubar had animated out but the mouse
  // is still on the toolbar.
  base::scoped_nsobject<CrTrackingArea> trackingArea_;

  // The content view for the window.
  NSView* contentView_;  // weak

  // The owner of this class.
  FullscreenToolbarController* owner_;  // weak

  // The object managing the fullscreen toolbar's animations.
  FullscreenToolbarAnimationController* animationController_;  // weak
}

@end

@implementation FullscreenToolbarMouseTracker

- (instancetype)
initWithFullscreenToolbarController:(FullscreenToolbarController*)owner
                animationController:
                    (FullscreenToolbarAnimationController*)animationController {
  if ((self = [super init])) {
    owner_ = owner;
    animationController_ = animationController;
  }

  return self;
}

- (void)dealloc {
  [self removeTrackingArea];
  [super dealloc];
}

- (void)updateTrackingArea {
  // Remove the tracking area if the toolbar isn't fully shown.
  if (!ui::IsCGFloatEqual([owner_ toolbarFraction], 1.0)) {
    [self removeTrackingArea];
    return;
  }

  if (trackingArea_) {
    // If |trackingArea_|'s rect matches |trackingAreaFrame_|, quit early.
    if (NSEqualRects(trackingAreaFrame_, [trackingArea_ rect]))
      return;

    [self removeTrackingArea];
  }

  BrowserWindowController* bwc = [owner_ browserWindowController];
  contentView_ = [[bwc window] contentView];

  trackingArea_.reset([[CrTrackingArea alloc]
      initWithRect:trackingAreaFrame_
           options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow
             owner:self
          userInfo:nil]);

  [contentView_ addTrackingArea:trackingArea_];
}

- (void)updateToolbarFrame:(NSRect)frame {
  NSRect contentBounds = [contentView_ bounds];
  trackingAreaFrame_ = frame;
  trackingAreaFrame_.origin.y -= kTrackingAreaAdditionalThreshold;
  trackingAreaFrame_.size.height =
      NSMaxY(contentBounds) - trackingAreaFrame_.origin.y;

  [self updateTrackingArea];
}

- (void)removeTrackingArea {
  if (!trackingArea_)
    return;

  DCHECK(contentView_);
  [contentView_ removeTrackingArea:trackingArea_];
  trackingArea_.reset();
  contentView_ = nil;
}

- (BOOL)mouseInsideTrackingArea {
  return [trackingArea_ mouseInsideTrackingAreaForView:contentView_];
}

- (void)mouseEntered:(NSEvent*)event {
  // Empty implementation. Required for CrTrackingArea.
}

- (void)mouseExited:(NSEvent*)event {
  DCHECK_EQ([event trackingArea], trackingArea_.get());

  animationController_->AnimateToolbarOutIfPossible();

  [owner_ updateToolbarLayout];
  [self removeTrackingArea];
}

@end
