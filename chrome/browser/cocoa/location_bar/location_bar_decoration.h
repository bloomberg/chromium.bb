// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_
#define CHROME_BROWSER_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_

#import <Cocoa/Cocoa.h>

#import "base/basictypes.h"

// Base class for decorations at the left and right of the location
// bar.  For instance, the location icon.

// |LocationBarDecoration| and subclasses should approximately
// parallel the classes provided under views/location_bar/.  The term
// "decoration" is used because "view" has strong connotations in
// Cocoa, and while these are view-like, they aren't views at all.
// Decorations are more like Cocoa cells, except implemented in C++ to
// allow more similarity to the other platform implementations.

class LocationBarDecoration {
 public:
  LocationBarDecoration()
      : visible_(false) {
  }
  virtual ~LocationBarDecoration() {}

  // Determines whether the decoration is visible.
  virtual bool IsVisible() const {
    return visible_;
  }
  virtual void SetVisible(bool visible) {
    visible_ = visible;
  }

  // Decorations can change their size to fit the available space.
  // Returns the width the decoration will use in the space allotted,
  // or |kOmittedWidth| if it should be omitted.
  virtual CGFloat GetWidthForSpace(CGFloat width);

  // Draw the decoration in the frame provided.  The frame will be
  // generated from an earlier call to |GetWidthForSpace()|.
  virtual void DrawInFrame(NSRect frame, NSView* control_view);

  // Returns the tooltip for this decoration, return |nil| for no tooltip.
  virtual NSString* GetToolTip() { return nil; }

  // Determine if the item can act as a drag source.
  virtual bool IsDraggable() { return false; }

  // The image to drag.
  virtual NSImage* GetDragImage() { return nil; }

  // The pasteboard to drag.
  virtual NSPasteboard* GetDragPasteboard() { return nil; }

  // Called on mouse down.  Return |false| to indicate that the press
  // was not processed and should be handled by the cell.
  virtual bool OnMousePressed(NSRect frame) { return false; }

  // Called to get the right-click menu, return |nil| for no menu.
  virtual NSMenu* GetMenu() { return nil; }

  // Width returned by |GetWidthForSpace()| when the item should be
  // omitted for this width;
  static const CGFloat kOmittedWidth;

 private:
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarDecoration);
};

#endif  // CHROME_BROWSER_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_
