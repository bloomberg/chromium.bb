// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_
#define CHROME_BROWSER_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/cocoa/location_bar/bubble_decoration.h"

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

  // Implement |LocationBarDecoration|.
  virtual bool IsDraggable();
  virtual NSPasteboard* GetDragPasteboard();
  virtual NSImage* GetDragImage();
  virtual NSRect GetDragImageFrame(NSRect frame) {
    return GetImageRectInFrame(frame);
  }
  virtual bool OnMousePressed(NSRect frame);
  virtual bool AcceptsMousePress() { return true; }

 private:
  LocationIconDecoration* location_icon_;  // weak, owned by location bar.

  DISALLOW_COPY_AND_ASSIGN(EVBubbleDecoration);
};

#endif  // CHROME_BROWSER_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_
