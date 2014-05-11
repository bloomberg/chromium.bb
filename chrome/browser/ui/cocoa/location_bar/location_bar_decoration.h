// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_

#import <Cocoa/Cocoa.h>

#import "base/basictypes.h"

class ButtonDecoration;

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
  virtual bool IsVisible() const;
  virtual void SetVisible(bool visible);

  // Decorations can change their size to fit the available space.
  // Returns the width the decoration will use in the space allotted,
  // or |kOmittedWidth| if it should be omitted.
  virtual CGFloat GetWidthForSpace(CGFloat width);

  // Draw the decoration in the frame provided.  The frame will be
  // generated from an earlier call to |GetWidthForSpace()|.
  virtual void DrawInFrame(NSRect frame, NSView* control_view);

  // Draw the decoration in the frame provided, possibly including a
  // background that fills |background_frame|.  The frame will be
  // generated from an earlier call to |GetWidthForSpace()|, and the
  // |background_frame| will include the column of pixels exactly
  // between two decorations.
  virtual void DrawWithBackgroundInFrame(NSRect background_frame,
                                         NSRect frame,
                                         NSView* control_view);

  // Returns the tooltip for this decoration, return |nil| for no tooltip.
  virtual NSString* GetToolTip();

  // Decorations which do not accept mouse events are treated like the
  // field's background for purposes of selecting text.  When such
  // decorations are adjacent to the text area, they will show the
  // I-beam cursor.  Decorations which do accept mouse events will get
  // an arrow cursor when the mouse is over them.
  virtual bool AcceptsMousePress();

  // Determine if the item can act as a drag source.
  virtual bool IsDraggable();

  // The image to drag.
  virtual NSImage* GetDragImage();

  // Return the place within the decoration's frame where the
  // |GetDragImage()| comes from.  This is used to make sure the image
  // appears correctly under the mouse while dragging.  |frame|
  // matches the frame passed to |DrawInFrame()|.
  virtual NSRect GetDragImageFrame(NSRect frame);

  // The pasteboard to drag.
  virtual NSPasteboard* GetDragPasteboard();

  // Called on mouse down.  Return |false| to indicate that the press
  // was not processed and should be handled by the cell.
  virtual bool OnMousePressed(NSRect frame, NSPoint location);

  // Called to get the right-click menu, return |nil| for no menu.
  virtual NSMenu* GetMenu();

  // Gets the font used to draw text in the decoration.
  virtual NSFont* GetFont() const;

  // Helper to get where the bubble point should land. |frame| specifies the
  // decorations' image rectangle. Defaults to |frame.origin| if not overriden.
  // The return value is in the same coordinate system as |frame|.
  virtual NSPoint GetBubblePointInFrame(NSRect frame);

  static void DrawLabel(NSString* label,
                        NSDictionary* attributes,
                        const NSRect& frame);
  static void DrawAttributedString(NSAttributedString* str,
                                   const NSRect& frame);
  static NSSize GetLabelSize(NSString* label,
                             NSDictionary* attributes);

  // Returns the current |LocationBarDecoration| as a |ButtonDecoration|, if it
  // inherits from that class (i.e. if it needs to act as a button).
  virtual ButtonDecoration* AsButtonDecoration();

  // Width returned by |GetWidthForSpace()| when the item should be
  // omitted for this width;
  static const CGFloat kOmittedWidth;

 private:
  bool visible_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_
