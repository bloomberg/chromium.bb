// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/location_bar/bubble_decoration.h"

// Draws the "Extended Validation SSL" bubble.  This will be a lock
// icon plus a label from the certification, and will replace the
// location icon for URLs which have an EV cert.  The |location_icon|
// is used to fulfill drag-related calls.

// TODO(shess): Refactor to pull the |location_icon| functionality out
// into a distinct class like views |ClickHandler|.
// http://crbug.com/48866

class LocationIconDecoration;

class EVBubbleDecoration : public BubbleDecoration {
 public:
  EVBubbleDecoration(LocationIconDecoration* location_icon, NSFont* font);
  virtual ~EVBubbleDecoration();

  // |GetWidthForSpace()| will set |full_label| as the label, if it
  // fits, else it will set an elided version.
  void SetFullLabel(NSString* full_label);

  // Get the point where the page info bubble should point within the
  // decoration's frame, in the cell's coordinates.
  NSPoint GetBubblePointInFrame(NSRect frame);

  // Implement |LocationBarDecoration|.
  virtual CGFloat GetWidthForSpace(CGFloat width);
  virtual bool IsDraggable();
  virtual NSPasteboard* GetDragPasteboard();
  virtual NSImage* GetDragImage();
  virtual NSRect GetDragImageFrame(NSRect frame);
  virtual bool OnMousePressed(NSRect frame);
  virtual bool AcceptsMousePress();

 private:
  // Keeps a reference to the font for use when eliding.
  scoped_nsobject<NSFont> font_;

  // The real label.  BubbleDecoration's label may be elided.
  scoped_nsobject<NSString> full_label_;

  LocationIconDecoration* location_icon_;  // weak, owned by location bar.

  DISALLOW_COPY_AND_ASSIGN(EVBubbleDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_
