// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

#include "base/logging.h"

const CGFloat LocationBarDecoration::kOmittedWidth = 0.0;
const CGFloat LocationBarDecoration::kTextYInset = 4.0;

bool LocationBarDecoration::IsVisible() const {
  return visible_;
}

void LocationBarDecoration::SetVisible(bool visible) {
  visible_ = visible;
}


CGFloat LocationBarDecoration::GetWidthForSpace(CGFloat width,
                                                CGFloat text_width) {
  NOTREACHED();
  return kOmittedWidth;
}

void LocationBarDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  NOTREACHED();
}

void LocationBarDecoration::DrawWithBackgroundInFrame(NSRect background_frame,
                                                      NSRect frame,
                                                      NSView* control_view) {
  // Default to no background.
  DrawInFrame(frame, control_view);
}

NSString* LocationBarDecoration::GetToolTip() {
  return nil;
}

bool LocationBarDecoration::AcceptsMousePress() {
  return false;
}

bool LocationBarDecoration::IsDraggable() {
  return false;
}

NSImage* LocationBarDecoration::GetDragImage() {
  return nil;
}

NSRect LocationBarDecoration::GetDragImageFrame(NSRect frame) {
  return NSZeroRect;
}

NSPasteboard* LocationBarDecoration::GetDragPasteboard() {
  return nil;
}

bool LocationBarDecoration::OnMousePressed(NSRect frame) {
  return false;
}

NSMenu* LocationBarDecoration::GetMenu() {
  return nil;
}

ButtonDecoration* LocationBarDecoration::AsButtonDecoration() {
  return NULL;
}

bool LocationBarDecoration::IsSeparator() const {
  return false;
}
