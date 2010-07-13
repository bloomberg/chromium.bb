// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_LOCATION_BAR_LOCATION_ICON_DECORATION_H_
#define CHROME_BROWSER_COCOA_LOCATION_BAR_LOCATION_ICON_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/location_bar/location_bar_decoration.h"

class LocationBarViewMac;

// LocationIconDecoration is used to display an icon to the left of
// the address.

class LocationIconDecoration : public LocationBarDecoration {
 public:
  explicit LocationIconDecoration(LocationBarViewMac* owner);
  virtual ~LocationIconDecoration();

  // The image to display for the location.
  NSImage* GetImage();
  void SetImage(NSImage* image);

  // Implement |LocationBarDecoration|.
  virtual CGFloat GetWidthForSpace(CGFloat width);
  virtual void DrawInFrame(NSRect frame, NSView* control_view);

  // Allow dragging the current URL.
  virtual bool IsDraggable();
  virtual NSPasteboard* GetDragPasteboard();
  virtual NSImage* GetDragImage() { return GetImage(); }

  // Show the page info panel on click.
  virtual bool OnMousePressed(NSRect frame);

 private:
  // The location bar view that owns us.
  LocationBarViewMac* owner_;

  scoped_nsobject<NSImage> image_;

  DISALLOW_COPY_AND_ASSIGN(LocationIconDecoration);
};

#endif  // CHROME_BROWSER_COCOA_LOCATION_BAR_LOCATION_ICON_DECORATION_H_
