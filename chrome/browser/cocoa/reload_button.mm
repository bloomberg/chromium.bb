// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/reload_button.h"

#include "base/nsimage_cache_mac.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/cocoa/gradient_button_cell.h"
#import "chrome/browser/cocoa/view_id_util.h"

namespace {

NSString* const kReloadImageName = @"reload_Template.pdf";
NSString* const kStopImageName = @"stop_Template.pdf";

}  // namespace

@implementation ReloadButton

- (void)dealloc {
  if (trackingArea_) {
    [self removeTrackingArea:trackingArea_];
    trackingArea_.reset();
  }
  [super dealloc];
}

- (void)updateTrackingAreas {
  // If the mouse is hovering when the tracking area is updated, the
  // control could end up locked into inappropriate behavior for
  // awhile, so unwind state.
  if (isMouseInside_)
    [self mouseExited:nil];

  if (trackingArea_) {
    [self removeTrackingArea:trackingArea_];
    trackingArea_.reset();
  }
  trackingArea_.reset([[NSTrackingArea alloc]
                        initWithRect:[self bounds]
                             options:(NSTrackingMouseEnteredAndExited |
                                      NSTrackingActiveInActiveApp)
                               owner:self
                            userInfo:nil]);
  [self addTrackingArea:trackingArea_];
}

- (void)awakeFromNib {
  [self updateTrackingAreas];

  // Don't allow multi-clicks, because the user probably wouldn't ever
  // want to stop+reload or reload+stop.
  [self setIgnoresMultiClick:YES];
}

- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force {
  pendingReloadMode_ = NO;

  // Can always transition to stop mode.  Only transition to reload
  // mode if forced or if the mouse isn't hovering.  Otherwise, note
  // that reload mode is desired and disable the button.
  if (isLoading) {
    [self setImage:nsimage_cache::ImageNamed(kStopImageName)];
    [self setTag:IDC_STOP];
    [self setEnabled:YES];
  } else if (force || ![self isMouseInside]) {
    [self setImage:nsimage_cache::ImageNamed(kReloadImageName)];
    [self setTag:IDC_RELOAD];

    // This button's cell may not have received a mouseExited event, and
    // therefore it could still think that the mouse is inside the button.  Make
    // sure the cell's sense of mouse-inside matches the local sense, to prevent
    // drawing artifacts.
    id cell = [self cell];
    if ([cell respondsToSelector:@selector(setMouseInside:animate:)])
      [cell setMouseInside:[self isMouseInside] animate:NO];
    [self setEnabled:YES];
  } else if ([self tag] == IDC_STOP) {
    pendingReloadMode_ = YES;
    [self setEnabled:NO];
  }
}

- (BOOL)sendAction:(SEL)theAction to:(id)theTarget {
  if ([self tag] == IDC_STOP) {
    // The stop command won't be valid after the attempt to change
    // back to reload.  But it "worked", so short-circuit it.
    const BOOL ret =
        pendingReloadMode_ ? YES : [super sendAction:theAction to:theTarget];

    // When the stop is processed, immediately change to reload mode,
    // even though the IPC still has to bounce off the renderer and
    // back before the regular |-setIsLoaded:force:| will be called.
    // [This is how views and gtk do it.]
    if (ret)
      [self setIsLoading:NO force:YES];

    return ret;
  }

  return [super sendAction:theAction to:theTarget];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  isMouseInside_ = YES;
}

- (void)mouseExited:(NSEvent*)theEvent {
  isMouseInside_ = NO;

  // Reload mode was requested during the hover.
  if (pendingReloadMode_)
    [self setIsLoading:NO force:YES];
}

- (BOOL)isMouseInside {
  return trackingArea_ && isMouseInside_;
}

- (ViewID)viewID {
  return VIEW_ID_RELOAD_BUTTON;
}

@end  // ReloadButton

@implementation ReloadButton (Testing)

- (NSTrackingArea*)trackingArea {
  return trackingArea_;
}

@end
