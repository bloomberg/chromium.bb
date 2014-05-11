// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ZOOM_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ZOOM_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#import "chrome/browser/ui/cocoa/browser/zoom_bubble_controller.h"
#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class LocationBarViewMac;
@class ZoomBubbleController;
class ZoomController;
class ZoomDecorationTest;

// Zoom icon at the end of the omnibox (close to page actions) when at a
// non-standard zoom level.
class ZoomDecoration : public ImageDecoration,
                       public ZoomBubbleControllerDelegate {
 public:
  explicit ZoomDecoration(LocationBarViewMac* owner);
  virtual ~ZoomDecoration();

  // Called when this decoration should show or hide itself in its most current
  // state.
  void Update(ZoomController* zoom_controller);

  // Shows the zoom bubble for this decoration. If |auto_close| is YES, then
  // the bubble will automatically close after a fixed period of time.
  void ShowBubble(BOOL auto_close);

  // Closes the zoom bubble.
  void CloseBubble();

 private:
  friend ZoomDecorationTest;

  bool IsAtDefaultZoom() const;
  bool ShouldShowDecoration() const;

  // LocationBarDecoration implementation.
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame, NSPoint location) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;
  virtual NSPoint GetBubblePointInFrame(NSRect frame) OVERRIDE;

  // ZoomBubbleControllerDelegate implementation.
  virtual content::WebContents* GetWebContents() OVERRIDE;
  virtual void OnClose() OVERRIDE;

  // The control that owns this. Weak.
  LocationBarViewMac* owner_;

  // The bubble that this decoration shows. Weak, owns self.
  ZoomBubbleController* bubble_;

  // The string to show for a tooltip.
  base::scoped_nsobject<NSString> tooltip_;

  DISALLOW_COPY_AND_ASSIGN(ZoomDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ZOOM_DECORATION_H_
