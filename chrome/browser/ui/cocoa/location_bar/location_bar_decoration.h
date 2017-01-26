// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"

@class DecorationMouseTrackingDelegate;
@class CrTrackingArea;

namespace gfx {
struct VectorIcon;
}

// Base class for decorations at the left and right of the location
// bar.  For instance, the location icon.

// |LocationBarDecoration| and subclasses should approximately
// parallel the classes provided under views/location_bar/.  The term
// "decoration" is used because "view" has strong connotations in
// Cocoa, and while these are view-like, they aren't views at all.
// Decorations are more like Cocoa cells, except implemented in C++ to
// allow more similarity to the other platform implementations.

// This enum class represents the state of the decoration's interactions
// with the mouse.
enum class DecorationMouseState { NONE, HOVER, PRESSED };

class LocationBarDecoration {
 public:
  LocationBarDecoration();
  virtual ~LocationBarDecoration();

  // Determines whether the decoration is visible.
  virtual bool IsVisible() const;
  virtual void SetVisible(bool visible);

  // Decorations can change their size to fit the available space.
  // Returns the width the decoration will use in the space allotted,
  // or |kOmittedWidth| if it should be omitted.
  virtual CGFloat GetWidthForSpace(CGFloat width);

  // Returns a NSRect derived from |frame| for the background to fill.
  virtual NSRect GetBackgroundFrame(NSRect frame);

  // Draw the decoration in the frame provided.  The frame will be
  // generated from an earlier call to |GetWidthForSpace()|.
  virtual void DrawInFrame(NSRect frame, NSView* control_view);

  // Draw the decoration in the frame provided, possibly including a
  // background that fills |background_frame|.  The frame will be
  // generated from an earlier call to |GetWidthForSpace()|, and the
  // |background_frame| will include the column of pixels exactly
  // between two decorations.
  void DrawWithBackgroundInFrame(NSRect frame, NSView* control_view);

  // Returns the tooltip for this decoration, return |nil| for no tooltip.
  virtual NSString* GetToolTip();

  // Methods to set up and remove the tracking area from the |control_view|.
  CrTrackingArea* SetupTrackingArea(NSRect frame, NSView* control_view);
  void RemoveTrackingArea();

  // Decorations which do not accept mouse events are treated like the
  // field's background for purposes of selecting text.  When such
  // decorations are adjacent to the text area, they will show the
  // I-beam cursor.  Decorations which do accept mouse events will get
  // an arrow cursor when the mouse is over them.
  virtual bool AcceptsMousePress();

  // Returns true if the decoration should display a background if it's
  // hovered or pressed. The default value is equivalent to the value returned
  // from AcceptsMousePress().
  virtual bool HasHoverAndPressEffect();

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

  // Called on mouse down, when the decoration isn't being dragged. Return
  // |false| to indicate that the press was not processed and should be
  // handled by the cell.
  virtual bool OnMousePressed(NSRect frame, NSPoint location);

  // Mouse events called on mouse down/up.
  void OnMouseDown();
  void OnMouseUp();

  // Called by |tracking_delegate_| when the mouse enters/exits the decoration.
  void OnMouseEntered();
  void OnMouseExited();

  // Sets the active state of the decoration. If the state has changed, call
  // UpdateDecorationState().
  void SetActive(bool active);

  // Called to get the right-click menu, return |nil| for no menu.
  virtual NSMenu* GetMenu();

  // Gets the font used to draw text in the decoration.
  virtual NSFont* GetFont() const;

  // Gets the accessibility NSView for this decoration. This NSView is
  // a transparent button that is positioned over this decoration to allow it to
  // accept keyboard focus and keyboard activations.
  virtual NSView* GetAccessibilityView();

  // Helper to get where the bubble point should land. |frame| specifies the
  // decorations' image rectangle. Defaults to |frame.origin| if not overriden.
  // The return value is in the same coordinate system as |frame|.
  virtual NSPoint GetBubblePointInFrame(NSRect frame);

  // Gets the Material Design vector-based icon.
  NSImage* GetMaterialIcon(bool location_bar_is_dark) const;

  static void DrawLabel(NSString* label,
                        NSDictionary* attributes,
                        const NSRect& frame);
  static void DrawAttributedString(NSAttributedString* str,
                                   const NSRect& frame);
  static NSSize GetLabelSize(NSString* label,
                             NSDictionary* attributes);

  // Called when the accessibility view receives an action via a keyboard button
  // press or VoiceOver activation. This method is public so it can be exposed
  // to the private DecorationAccessibilityView helper class.
  void OnAccessibilityViewAction();

  DecorationMouseState state() const { return state_; }

  bool active() const { return active_; }

  // Width returned by |GetWidthForSpace()| when the item should be
  // omitted for this width;
  static const CGFloat kOmittedWidth;

  // Material text color if the location bar is dark.
  static const SkColor kMaterialDarkModeTextColor;

 protected:
  // Gets the color used to draw the Material Design icon. The default
  // implementation satisfies most cases - few subclasses should need to
  // override.
  virtual SkColor GetMaterialIconColor(bool location_bar_is_dark) const;

  // Gets the decoration's Material Design vector icon. Subclasses should
  // override to return the correct icon. Not an abstract method because some
  // decorations are assigned their icon (vs. creating it themselves).
  virtual const gfx::VectorIcon* GetMaterialVectorIcon() const;

  // Gets the color used for the divider. Only used in Material design.
  NSColor* GetDividerColor(bool location_bar_is_dark) const;

 private:
  // Called when the state of the decoration is updated.
  void UpdateDecorationState();

  bool visible_ = false;

  // True if the decoration is active.
  bool active_ = false;

  base::scoped_nsobject<NSView> accessibility_view_;

  // The decoration's tracking area. Only set if the decoration accepts a mouse
  // press.
  base::scoped_nsobject<CrTrackingArea> tracking_area_;

  // The view that |tracking_area_| is added to.
  NSView* tracking_area_owner_ = nullptr;

  // Delegate object that handles mouseEntered: and mouseExited: events from
  // the tracking area.
  base::scoped_nsobject<DecorationMouseTrackingDelegate> tracking_delegate_;

  // The state of the decoration.
  DecorationMouseState state_ = DecorationMouseState::NONE;

  DISALLOW_COPY_AND_ASSIGN(LocationBarDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_DECORATION_H_
