// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_

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
  explicit EVBubbleDecoration(LocationIconDecoration* location_icon);
  virtual ~EVBubbleDecoration();

  // |GetWidthForSpace()| will set |full_label| as the label, if it
  // fits, else it will set an elided version.
  void SetFullLabel(NSString* full_label);


  // Implement |LocationBarDecoration|.
  virtual CGFloat GetWidthForSpace(CGFloat width) OVERRIDE;
  virtual bool IsDraggable() OVERRIDE;
  virtual NSPasteboard* GetDragPasteboard() OVERRIDE;
  virtual NSImage* GetDragImage() OVERRIDE;
  virtual NSRect GetDragImageFrame(NSRect frame) OVERRIDE;
  virtual bool OnMousePressed(NSRect frame, NSPoint location) OVERRIDE;
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual NSPoint GetBubblePointInFrame(NSRect frame) OVERRIDE;

  // Implement |BubbleDecoration|.
  virtual ui::NinePartImageIds GetBubbleImageIds() OVERRIDE;

 private:
  // The real label.  BubbleDecoration's label may be elided.
  base::scoped_nsobject<NSString> full_label_;

  LocationIconDecoration* location_icon_;  // weak, owned by location bar.

  DISALLOW_COPY_AND_ASSIGN(EVBubbleDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_EV_BUBBLE_DECORATION_H_
