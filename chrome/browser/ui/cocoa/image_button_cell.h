// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_IMAGE_BUTTON_CELL_H_
#define CHROME_BROWSER_UI_COCOA_IMAGE_BUTTON_CELL_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"

namespace image_button_cell {

// Possible states
enum ButtonState {
  kDefaultState = 0,
  kHoverState,
  kPressedState,
  kDisabledState,
  // The same as above, but for non-main, non-key windows.
  kDefaultStateBackground,
  kHoverStateBackground,
  kButtonStateCount
};

} // namespace ImageButtonCell

@protocol ImageButton
@optional
// Sent from an ImageButtonCell to its view when the mouse enters or exits the
// cell.
- (void)mouseInsideStateDidChange:(BOOL)isInside;
@end

// A button cell that can disable a different image for each possible button
// state. Images are specified by image IDs.
@interface ImageButtonCell : NSButtonCell {
 @private
  struct {
    // At most one of these two fields will be non-null.
    int imageId;
    base::scoped_nsobject<NSImage> image;
  } image_[image_button_cell::kButtonStateCount];
  BOOL isMouseInside_;
}

@property(assign, nonatomic) BOOL isMouseInside;

// Gets the image for the given button state. Will load from a resource pak if
// the image was originally set using an image ID.
- (NSImage*)imageForState:(image_button_cell::ButtonState)state
                     view:(NSView*)controlView;

// Sets the image for the given button state using an image ID.
// The image will be lazy loaded from a resource pak -- important because
// this is in the hot path for startup.
- (void)setImageID:(NSInteger)imageID
    forButtonState:(image_button_cell::ButtonState)state;

// Sets the image for the given button state using an image.
- (void)setImage:(NSImage*)image
  forButtonState:(image_button_cell::ButtonState)state;

// Gets the alpha to use to draw the button for the current window focus state.
- (CGFloat)imageAlphaForWindowState:(NSWindow*)window;

// Draws the cell's image within |cellFrame|.
- (void)drawImageWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;

// If |controlView| is a first responder then draws a blue focus ring.
- (void)drawFocusRingWithFrame:(NSRect)cellFrame inView:(NSView*)controlView;

@end

#endif // CHROME_BROWSER_UI_COCOA_IMAGE_BUTTON_CELL_H_
