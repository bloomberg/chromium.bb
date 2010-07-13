// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/ev_bubble_decoration.h"

#import "base/logging.h"
#import "chrome/browser/cocoa/image_utils.h"
#import "chrome/browser/cocoa/location_bar/location_icon_decoration.h"

namespace {

// TODO(shess): This is ugly, find a better way.  Using it right now
// so that I can crib from gtk and still be able to see that I'm using
// the same values easily.
const NSColor* ColorWithRGBBytes(int rr, int gg, int bb) {
  DCHECK_LE(rr, 255);
  DCHECK_LE(bb, 255);
  DCHECK_LE(gg, 255);
  return [NSColor colorWithCalibratedRed:static_cast<float>(rr)/255.0
                                   green:static_cast<float>(gg)/255.0
                                    blue:static_cast<float>(bb)/255.0
                                   alpha:1.0];
}

}  // namespace

EVBubbleDecoration::EVBubbleDecoration(
    LocationIconDecoration* location_icon,
    NSFont* font)
    : BubbleDecoration(font),
      location_icon_(location_icon) {
  // Color tuples stolen from location_bar_view_gtk.cc.
  NSColor* border_color = ColorWithRGBBytes(0x90, 0xc3, 0x90);
  NSColor* background_color = ColorWithRGBBytes(0xef, 0xfc, 0xef);
  NSColor* text_color = ColorWithRGBBytes(0x07, 0x95, 0x00);
  SetColors(border_color, background_color, text_color);
}

// Pass mouse operations through to location icon.
bool EVBubbleDecoration::IsDraggable() {
  return location_icon_->IsDraggable();
}

NSPasteboard* EVBubbleDecoration::GetDragPasteboard() {
  return location_icon_->GetDragPasteboard();
}

NSImage* EVBubbleDecoration::GetDragImage() {
  return location_icon_->GetDragImage();
}

bool EVBubbleDecoration::OnMousePressed(NSRect frame) {
  return location_icon_->OnMousePressed(frame);
}
