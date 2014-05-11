// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "ui/gfx/font.h"

const CGFloat LocationBarDecoration::kOmittedWidth = 0.0;

bool LocationBarDecoration::IsVisible() const {
  return visible_;
}

void LocationBarDecoration::SetVisible(bool visible) {
  visible_ = visible;
}


CGFloat LocationBarDecoration::GetWidthForSpace(CGFloat width) {
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

bool LocationBarDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  return false;
}

NSMenu* LocationBarDecoration::GetMenu() {
  return nil;
}

NSFont* LocationBarDecoration::GetFont() const {
  return OmniboxViewMac::GetFieldFont(gfx::Font::NORMAL);
}

NSPoint LocationBarDecoration::GetBubblePointInFrame(NSRect frame) {
  // Clients that use a bubble should implement this. Can't be abstract
  // because too many LocationBarDecoration subclasses don't use a bubble.
  // Can't live on subclasses only because it needs to be on a shared API.
  NOTREACHED();
  return frame.origin;
}

// static
void LocationBarDecoration::DrawLabel(NSString* label,
                                      NSDictionary* attributes,
                                      const NSRect& frame) {
  base::scoped_nsobject<NSAttributedString> str(
      [[NSAttributedString alloc] initWithString:label attributes:attributes]);
  DrawAttributedString(str, frame);
}

// static
void LocationBarDecoration::DrawAttributedString(NSAttributedString* str,
                                                 const NSRect& frame) {
  NSRect text_rect = frame;
  text_rect.size.height = [str size].height;
  text_rect.origin.y = roundf(NSMidY(frame) - NSHeight(text_rect) / 2.0) - 1;
  [str drawInRect:text_rect];
}

// static
NSSize LocationBarDecoration::GetLabelSize(NSString* label,
                                           NSDictionary* attributes) {
  return [label sizeWithAttributes:attributes];
}

ButtonDecoration* LocationBarDecoration::AsButtonDecoration() {
  return NULL;
}
