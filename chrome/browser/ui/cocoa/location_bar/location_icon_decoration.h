// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_ICON_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_ICON_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class LocationBarViewMac;

// LocationIconDecoration is used to display an icon to the left of
// the address.

class LocationIconDecoration : public ImageDecoration {
 public:
  explicit LocationIconDecoration(LocationBarViewMac* owner);
  ~LocationIconDecoration() override;

  // Allow dragging the current URL.
  bool IsDraggable() override;
  NSPasteboard* GetDragPasteboard() override;
  NSImage* GetDragImage() override;
  NSRect GetDragImageFrame(NSRect frame) override;

  // Show the page info panel on click.
  bool OnMousePressed(NSRect frame, NSPoint location) override;
  bool AcceptsMousePress() override;
  bool HasHoverAndPressEffect() override;
  NSString* GetToolTip() override;
  NSPoint GetBubblePointInFrame(NSRect frame) override;
  NSRect GetBackgroundFrame(NSRect frame) override;

 private:
  NSRect drag_frame_;
  // The location bar view that owns us.
  LocationBarViewMac* owner_;

  DISALLOW_COPY_AND_ASSIGN(LocationIconDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_ICON_DECORATION_H_
