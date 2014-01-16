// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/image_button_cell.h"

#include "base/logging.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#import "chrome/browser/ui/cocoa/rect_path_utils.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

// When the window doesn't have focus then we want to draw the button with a
// slightly lighter color. We do this by just reducing the alpha.
const CGFloat kImageNoFocusAlpha = 0.65;

@interface ImageButtonCell (Private)
- (void)sharedInit;
- (image_button_cell::ButtonState)currentButtonState;
- (NSImage*)imageForID:(NSInteger)imageID
           controlView:(NSView*)controlView;
@end

@implementation ImageButtonCell

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

- (NSImage*)imageForState:(image_button_cell::ButtonState)state
                     view:(NSView*)controlView{
  if (image_[state].imageId)
    return [self imageForID:image_[state].imageId controlView:controlView];
  return image_[state].image;
}

- (void)drawImageWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  image_button_cell::ButtonState state = [self currentButtonState];
  BOOL windowHasFocus = [[controlView window] isMainWindow] ||
                        [[controlView window] isKeyWindow];
  CGFloat alpha = [self imageAlphaForWindowState:[controlView window]];
  NSImage* image = [self imageForState:state view:controlView];

  if (!windowHasFocus) {
    NSImage* defaultImage = [self
      imageForState:image_button_cell::kDefaultStateBackground
               view:controlView];
    NSImage* hoverImage = [self
      imageForState:image_button_cell::kHoverStateBackground
               view:controlView];
    if ([self currentButtonState] == image_button_cell::kDefaultState &&
        defaultImage) {
      image = defaultImage;
      alpha = 1.0;
    } else if ([self currentButtonState] == image_button_cell::kHoverState &&
        hoverImage) {
      image = hoverImage;
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
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  [self drawImageWithFrame:cellFrame inView:controlView];
  // Only draw custom focus ring if the 10.7 focus ring APIs are not available.
  // TODO(groby): Remove once we build against the 10.7 SDK.
  if (![self respondsToSelector:@selector(drawFocusRingMaskWithFrame:inView:)])
    [self drawFocusRingWithFrame:cellFrame inView:controlView];
}

- (void)setImageID:(NSInteger)imageID
    forButtonState:(image_button_cell::ButtonState)state {
  DCHECK_GE(state, 0);
  DCHECK_LT(state, image_button_cell::kButtonStateCount);

  image_[state].image.reset();
  image_[state].imageId = imageID;
  [[self controlView] setNeedsDisplay:YES];
}

// Sets the image for the given button state using an image.
- (void)setImage:(NSImage*)image
  forButtonState:(image_button_cell::ButtonState)state {
  DCHECK_GE(state, 0);
  DCHECK_LT(state, image_button_cell::kButtonStateCount);

  image_[state].image.reset([image retain]);
  image_[state].imageId = 0;
  [[self controlView] setNeedsDisplay:YES];
}

- (CGFloat)imageAlphaForWindowState:(NSWindow*)window {
  BOOL windowHasFocus = [window isMainWindow] || [window isKeyWindow];
  return windowHasFocus ? 1.0 : kImageNoFocusAlpha;
}

- (void)drawFocusRingWithFrame:(NSRect)cellFrame inView:(NSView*)controlView {
  if (![self showsFirstResponder])
    return;
  gfx::ScopedNSGraphicsContextSaveGState scoped_state;
  const CGFloat lineWidth = [controlView cr_lineWidth];
  rect_path_utils::FrameRectWithInset(rect_path_utils::RoundedCornerAll,
                                      NSInsetRect(cellFrame, 0, lineWidth),
                                      0.0,            // insetX
                                      0.0,            // insetY
                                      3.0,            // outerRadius
                                      lineWidth * 2,  // lineWidth
                                      [controlView
                                          cr_keyboardFocusIndicatorColor]);
}

- (image_button_cell::ButtonState)currentButtonState {
  bool (^has)(image_button_cell::ButtonState) =
      ^(image_button_cell::ButtonState state) {
          return image_[state].image || image_[state].imageId;
      };
  if (![self isEnabled] && has(image_button_cell::kDisabledState))
    return image_button_cell::kDisabledState;
  if ([self isHighlighted] && has(image_button_cell::kPressedState))
    return image_button_cell::kPressedState;
  if ([self isMouseInside] && has(image_button_cell::kHoverState))
    return image_button_cell::kHoverState;
  return image_button_cell::kDefaultState;
}

- (NSImage*)imageForID:(NSInteger)imageID
           controlView:(NSView*)controlView {
  if (!imageID)
    return nil;

  ui::ThemeProvider* themeProvider = [[controlView window] themeProvider];
  if (!themeProvider)
    return nil;

  return themeProvider->GetNSImageNamed(imageID);
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
