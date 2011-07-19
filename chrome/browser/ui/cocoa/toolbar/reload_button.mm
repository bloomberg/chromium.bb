// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/reload_button.h"

#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Constant matches Windows.
NSTimeInterval kPendingReloadTimeout = 1.35;

}  // namespace

@implementation ReloadButton

+ (Class)cellClass {
  return [ImageButtonCell class];
}

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

- (void)updateTag:(NSInteger)anInt {
  if ([self tag] == anInt)
    return;

  // Forcibly remove any stale tooltip which is being displayed.
  [self removeAllToolTips];

  [self setTag:anInt];
  if (anInt == IDC_RELOAD) {
    [[self cell] setImageID:IDR_RELOAD
             forButtonState:image_button_cell::kDefaultState];
    [[self cell] setImageID:IDR_RELOAD_H
             forButtonState:image_button_cell::kHoverState];
    [[self cell] setImageID:IDR_RELOAD_P
             forButtonState:image_button_cell::kPressedState];
    // The stop button has a disabled image but the reload button doesn't. To
    // unset it we have to explicilty change the image ID to 0.
    [[self cell] setImageID:0
             forButtonState:image_button_cell::kDisabledState];
    [self setToolTip:l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_RELOAD)];
  } else if (anInt == IDC_STOP) {
    [[self cell] setImageID:IDR_STOP
             forButtonState:image_button_cell::kDefaultState];
    [[self cell] setImageID:IDR_STOP_H
             forButtonState:image_button_cell::kHoverState];
    [[self cell] setImageID:IDR_STOP_P
             forButtonState:image_button_cell::kPressedState];
    [[self cell] setImageID:IDR_STOP_D
             forButtonState:image_button_cell::kDisabledState];
    [self setToolTip:l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STOP)];
  } else {
    NOTREACHED();
  }
}

- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force {
  // Can always transition to stop mode.  Only transition to reload
  // mode if forced or if the mouse isn't hovering.  Otherwise, note
  // that reload mode is desired and disable the button.
  if (isLoading) {
    pendingReloadTimer_.reset();
    [self updateTag:IDC_STOP];
    [self setEnabled:YES];
  } else if (force || ![self isMouseInside]) {
    pendingReloadTimer_.reset();
    [self updateTag:IDC_RELOAD];

    // This button's cell may not have received a mouseExited event, and
    // therefore it could still think that the mouse is inside the button.  Make
    // sure the cell's sense of mouse-inside matches the local sense, to prevent
    // drawing artifacts.
    id cell = [self cell];
    if ([cell respondsToSelector:@selector(setIsMouseInside:)])
      [cell setIsMouseInside:[self isMouseInside]];
    [self setEnabled:YES];
  } else if ([self tag] == IDC_STOP && !pendingReloadTimer_) {
    [self setEnabled:NO];
    pendingReloadTimer_.reset(
        [[NSTimer scheduledTimerWithTimeInterval:kPendingReloadTimeout
                                          target:self
                                        selector:@selector(forceReloadState)
                                        userInfo:nil
                                         repeats:NO] retain]);
  }
}

- (void)forceReloadState {
  [self setIsLoading:NO force:YES];
}

- (BOOL)sendAction:(SEL)theAction to:(id)theTarget {
  if ([self tag] == IDC_STOP) {
    // When the timer is started, the button is disabled, so this
    // should not be possible.
    DCHECK(!pendingReloadTimer_.get());

    // When the stop is processed, immediately change to reload mode,
    // even though the IPC still has to bounce off the renderer and
    // back before the regular |-setIsLoaded:force:| will be called.
    // [This is how views and gtk do it.]
    const BOOL ret = [super sendAction:theAction to:theTarget];
    if (ret)
      [self forceReloadState];
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
  if (pendingReloadTimer_)
    [self forceReloadState];
}

- (BOOL)isMouseInside {
  return isMouseInside_;
}

- (ViewID)viewID {
  return VIEW_ID_RELOAD_BUTTON;
}

@end  // ReloadButton

@implementation ReloadButton (Testing)

+ (void)setPendingReloadTimeout:(NSTimeInterval)seconds {
  kPendingReloadTimeout = seconds;
}

- (NSTrackingArea*)trackingArea {
  return trackingArea_;
}

@end
