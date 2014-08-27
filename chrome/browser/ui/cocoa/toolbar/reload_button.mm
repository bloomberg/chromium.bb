// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/toolbar/reload_button.h"

#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Constant matches Windows.
NSTimeInterval kPendingReloadTimeout = 1.35;

}  // namespace

@interface ReloadButton ()
- (void)invalidatePendingReloadTimer;
- (void)forceReloadState:(NSTimer *)timer;
@end

@implementation ReloadButton

+ (Class)cellClass {
  return [ImageButtonCell class];
}

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    // Since this is not a custom view, -awakeFromNib won't be called twice.
    [self awakeFromNib];
  }
  return self;
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow {
  // If this view is moved to a new window, reset its state.
  [self setIsLoading:NO force:YES];
  [super viewWillMoveToWindow:newWindow];
}

- (void)awakeFromNib {
  // Don't allow multi-clicks, because the user probably wouldn't ever
  // want to stop+reload or reload+stop.
  [self setIgnoresMultiClick:YES];
}

- (void)invalidatePendingReloadTimer {
  [pendingReloadTimer_ invalidate];
  pendingReloadTimer_ = nil;
}

- (void)updateTag:(NSInteger)anInt {
  if ([self tag] == anInt)
    return;

  // Forcibly remove any stale tooltip which is being displayed.
  [self removeAllToolTips];
  id cell = [self cell];
  [self setTag:anInt];
  if (anInt == IDC_RELOAD) {
    [cell setImageID:IDR_RELOAD
      forButtonState:image_button_cell::kDefaultState];
    [cell setImageID:IDR_RELOAD_H
      forButtonState:image_button_cell::kHoverState];
    [cell setImageID:IDR_RELOAD_P
      forButtonState:image_button_cell::kPressedState];
    // The stop button has a disabled image but the reload button doesn't. To
    // unset it we have to explicilty change the image ID to 0.
    [cell setImageID:0
      forButtonState:image_button_cell::kDisabledState];
    [self setToolTip:l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_RELOAD)];
  } else if (anInt == IDC_STOP) {
    [cell setImageID:IDR_STOP
      forButtonState:image_button_cell::kDefaultState];
    [cell setImageID:IDR_STOP_H
      forButtonState:image_button_cell::kHoverState];
    [cell setImageID:IDR_STOP_P
      forButtonState:image_button_cell::kPressedState];
    [cell setImageID:IDR_STOP_D
      forButtonState:image_button_cell::kDisabledState];
    [self setToolTip:l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_STOP)];
  } else {
    NOTREACHED();
  }
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
  if ([attribute isEqualToString:NSAccessibilityEnabledAttribute] &&
      pendingReloadTimer_) {
    return [NSNumber numberWithBool:NO];
  } else {
    return [super accessibilityAttributeValue:attribute];
  }
}

- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force {
  // Can always transition to stop mode.  Only transition to reload
  // mode if forced or if the mouse isn't hovering.  Otherwise, note
  // that reload mode is desired and disable the button.
  if (isLoading) {
    [self invalidatePendingReloadTimer];
    [self updateTag:IDC_STOP];
  } else if (force) {
    [self invalidatePendingReloadTimer];
    [self updateTag:IDC_RELOAD];
  } else if ([self tag] == IDC_STOP &&
             !pendingReloadTimer_ &&
             [[self cell] isMouseInside]) {
    id cell = [self cell];
    [cell setImageID:IDR_STOP_D
      forButtonState:image_button_cell::kDefaultState];
    [cell setImageID:IDR_STOP_D
      forButtonState:image_button_cell::kDisabledState];
    [cell setImageID:IDR_STOP_D
      forButtonState:image_button_cell::kHoverState];
    [cell setImageID:IDR_STOP_D
      forButtonState:image_button_cell::kPressedState];
    pendingReloadTimer_ =
        [NSTimer timerWithTimeInterval:kPendingReloadTimeout
                                target:self
                              selector:@selector(forceReloadState:)
                              userInfo:nil
                               repeats:NO];
    // Must add the timer to |NSRunLoopCommonModes| because
    // it should run in |NSEventTrackingRunLoopMode| as well as
    // |NSDefaultRunLoopMode|.
    [[NSRunLoop currentRunLoop] addTimer:pendingReloadTimer_
                                 forMode:NSRunLoopCommonModes];
  } else {
    [self invalidatePendingReloadTimer];
    [self updateTag:IDC_RELOAD];
  }
  [self setEnabled:pendingReloadTimer_ == nil];
}

- (void)forceReloadState:(NSTimer *)timer {
  DCHECK_EQ(timer, pendingReloadTimer_);
  [self setIsLoading:NO force:YES];
  // Verify that |pendingReloadTimer_| is nil so it is not left dangling.
  DCHECK(!pendingReloadTimer_);
}

- (BOOL)sendAction:(SEL)theAction to:(id)theTarget {
  if ([self tag] == IDC_STOP) {
    if (pendingReloadTimer_) {
      // If |pendingReloadTimer_| then the control is currently being
      // drawn in a disabled state, so just return. The control is NOT actually
      // disabled, otherwise mousetracking (courtesy of the NSButtonCell)
      // would not work.
      return YES;
    } else {
      // When the stop is processed, immediately change to reload mode,
      // even though the IPC still has to bounce off the renderer and
      // back before the regular |-setIsLoaded:force:| will be called.
      // [This is how views and gtk do it.]
      BOOL ret = [super sendAction:theAction to:theTarget];
      if (ret)
        [self forceReloadState:pendingReloadTimer_];
      return ret;
    }
  }

  return [super sendAction:theAction to:theTarget];
}

- (ViewID)viewID {
  return VIEW_ID_RELOAD_BUTTON;
}

- (void)mouseInsideStateDidChange:(BOOL)isInside {
  [pendingReloadTimer_ fire];
}

@end  // ReloadButton

@implementation ReloadButton (Testing)

+ (void)setPendingReloadTimeout:(NSTimeInterval)seconds {
  kPendingReloadTimeout = seconds;
}

@end
