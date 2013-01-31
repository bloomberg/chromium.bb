// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/button_decoration.h"

#include "base/logging.h"

ButtonDecoration::ButtonDecoration()
    : state_(kButtonStateNormal) {
}

ButtonDecoration::~ButtonDecoration() {
}

void ButtonDecoration::SetButtonState(ButtonDecoration::ButtonState state) {
  state_ = state;
}

ButtonDecoration::ButtonState ButtonDecoration::GetButtonState() const {
  return state_;
}

bool ButtonDecoration::OnMousePressedWithView(
    NSRect frame, NSView* control_view) {
  ButtonState old_state = GetButtonState();
  SetButtonState(ButtonDecoration::kButtonStatePressed);
  [control_view setNeedsDisplay:YES];

  bool handled = OnMousePressed(frame);

  SetButtonState(old_state);
  return handled;
}

CGFloat ButtonDecoration::GetWidthForSpace(CGFloat width, CGFloat text_width) {
  NSImage* image = GetImage();
  if (image) {
    const CGFloat image_width = [image size].width;
    if (image_width <= width)
      return image_width;
  }
  return kOmittedWidth;
}

void ButtonDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  NSImage *image = GetImage();
  const CGFloat x_inset =
      std::floor((NSWidth(frame) - [image size].width) / 2.0);
  const CGFloat y_inset =
      std::floor((NSHeight(frame) - [image size].height) / 2.0);

  [image drawInRect:NSInsetRect(frame, x_inset, y_inset)
           fromRect:NSZeroRect  // Entire image
          operation:NSCompositeSourceOver
           fraction:1.0
     respectFlipped:YES
              hints:nil];
}

bool ButtonDecoration::OnMousePressed(NSRect frame) {
  return false;
}

ButtonDecoration* ButtonDecoration::AsButtonDecoration() {
  return this;
}

void ButtonDecoration::SetNormalImage(NSImage* normal_image) {
  normal_image_.reset([normal_image retain]);
}

void ButtonDecoration::SetHoverImage(NSImage* hover_image) {
  hover_image_.reset([hover_image retain]);
}

void ButtonDecoration::SetPressedImage(NSImage* pressed_image) {
  pressed_image_.reset([pressed_image retain]);
}

NSImage* ButtonDecoration::GetImage() {
  switch(state_) {
    case kButtonStateNormal:
      DCHECK(normal_image_.get());
      return normal_image_.get();
    case kButtonStateHover:
      DCHECK(hover_image_.get());
      return hover_image_.get();
    case kButtonStatePressed:
      DCHECK(pressed_image_.get());
      return pressed_image_.get();
    default:
      NOTREACHED();
      return nil;
  }
}
