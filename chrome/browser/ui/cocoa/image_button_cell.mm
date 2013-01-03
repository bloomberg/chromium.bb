// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/image_button_cell.h"

#include "base/logging.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

// Adjust the overlay position relative to the top right of the button image.
const CGFloat kOverlayOffsetX = -3;
const CGFloat kOverlayOffsetY = 5;

// When the window doesn't have focus then we want to draw the button with a
// slightly lighter color. We do this by just reducing the alpha.
const CGFloat kImageNoFocusAlpha = 0.65;

} // namespace

@interface ImageButtonCell (Private)
- (void)sharedInit;
- (image_button_cell::ButtonState)currentButtonState;
- (NSImage*)imageForID:(NSInteger)imageID
           controlView:(NSView*)controlView;
@end

@implementation ImageButtonCell

@synthesize overlayImageID = overlayImageID_;
@synthesize isMouseInside = isMouseInside_;

// For nib instantiations
- (id)initWithCoder:(NSCoder*)decoder {
  if ((self = [super initWithCoder:decoder])) {
    [self sharedInit];
  }
  return self;
}

// For programmatic instantiations
- (id)initTextCell:(NSString*)string {
  if ((self = [super initTextCell:string])) {
    [self sharedInit];
  }
  return self;
}

- (void)sharedInit {
  [self setHighlightsBy:NSNoCellMask];

  // We need to set this so that we can override |-mouseEntered:| and
  // |-mouseExited:| to change the button image on hover states.
  [self setShowsBorderOnlyWhileMouseInside:YES];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  BOOL windowHasFocus = [[controlView window] isMainWindow] ||
                        [[controlView window] isKeyWindow];
  CGFloat alpha = windowHasFocus ? 1.0 : kImageNoFocusAlpha;
  NSImage* image = image_[[self currentButtonState]];

  if (!windowHasFocus) {
    if ([self currentButtonState] == image_button_cell::kDefaultState &&
        image_[image_button_cell::kDefaultStateBackground]) {
      image = image_[image_button_cell::kDefaultStateBackground];
      alpha = 1.0;
    } else if ([self currentButtonState] == image_button_cell::kHoverState &&
        image_[image_button_cell::kHoverStateBackground]) {
      image = image_[image_button_cell::kHoverStateBackground];
      alpha = 1.0;
    }
  }

  NSRect imageRect;
  imageRect.size = [image size];
  imageRect.origin.x = cellFrame.origin.x +
    roundf((NSWidth(cellFrame) - NSWidth(imageRect)) / 2.0);
  imageRect.origin.y = cellFrame.origin.y +
    roundf((NSHeight(cellFrame) - NSHeight(imageRect)) / 2.0);

  [image drawInRect:imageRect
           fromRect:NSZeroRect
          operation:NSCompositeSourceOver
           fraction:alpha
     respectFlipped:YES
              hints:nil];

  if (overlayImageID_) {
    NSImage* overlayImage = [self imageForID:overlayImageID_
                                 controlView:controlView];
    NSRect overlayRect;
    overlayRect.size = [overlayImage size];
    overlayRect.origin.x = NSMaxX(imageRect) - NSWidth(overlayRect) +
                           kOverlayOffsetX;
    overlayRect.origin.y = NSMinY(imageRect) + kOverlayOffsetY;

    [overlayImage drawInRect:overlayRect
                    fromRect:NSZeroRect
                   operation:NSCompositeSourceOver
                    fraction:1.0
              respectFlipped:YES
                       hints:nil];
  }
}

- (void)setImageID:(NSInteger)imageID
    forButtonState:(image_button_cell::ButtonState)state {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* image = imageID ? rb.GetNativeImageNamed(imageID).ToNSImage() : nil;
  [self setImage:image forButtonState:state];
}

// Sets the image for the given button state using an image.
- (void)setImage:(NSImage*)image
  forButtonState:(image_button_cell::ButtonState)state {
  DCHECK_GE(state, 0);
  DCHECK_LT(state, image_button_cell::kButtonStateCount);
  image_[state].reset([image retain]);
  [[self controlView] setNeedsDisplay:YES];
}

- (void)setOverlayImageID:(NSInteger)imageID {
  if (overlayImageID_ != imageID) {
    overlayImageID_ = imageID;
    [[self controlView] setNeedsDisplay:YES];
  }
}

- (image_button_cell::ButtonState)currentButtonState {
  if (![self isEnabled] && image_[image_button_cell::kDisabledState])
    return image_button_cell::kDisabledState;
  else if ([self isHighlighted] && image_[image_button_cell::kPressedState])
    return image_button_cell::kPressedState;
  else if ([self isMouseInside] && image_[image_button_cell::kHoverState])
    return image_button_cell::kHoverState;
  else
    return image_button_cell::kDefaultState;
}

- (NSImage*)imageForID:(NSInteger)imageID
           controlView:(NSView*)controlView {
  if (!imageID)
    return nil;

  ui::ThemeProvider* themeProvider = [[controlView window] themeProvider];
  if (!themeProvider)
    return nil;

  return themeProvider->GetNSImageNamed(imageID, true);
}

- (void)setIsMouseInside:(BOOL)isMouseInside {
  if (isMouseInside_ != isMouseInside) {
    isMouseInside_ = isMouseInside;
    NSView<ImageButton>* control =
        static_cast<NSView<ImageButton>*>([self controlView]);
    if ([control respondsToSelector:@selector(mouseInsideStateDidChange:)]) {
      [control mouseInsideStateDidChange:isMouseInside];
    }
    [control setNeedsDisplay:YES];
  }
}

- (void)setShowsBorderOnlyWhileMouseInside:(BOOL)show {
  VLOG_IF(1, !show) << "setShowsBorderOnlyWhileMouseInside:NO ignored";
}

- (BOOL)showsBorderOnlyWhileMouseInside {
  // Always returns YES so that buttons always get mouse tracking even when
  // disabled. The reload button (and possibly others) depend on this.
  return YES;
}

- (void)mouseEntered:(NSEvent*)theEvent {
  [self setIsMouseInside:YES];
}

- (void)mouseExited:(NSEvent*)theEvent {
  [self setIsMouseInside:NO];
}

@end
