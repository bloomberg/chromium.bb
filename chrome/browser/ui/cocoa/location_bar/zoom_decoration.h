// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ZOOM_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ZOOM_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class ToolbarModel;
class ZoomController;

// Zoom icon at the end of the omnibox (close to page actions) when at a
// non-standard zoom level.
// TODO(dbeam): add mouse event handlers to show a zoom bubble on click.
class ZoomDecoration : public ImageDecoration {
 public:
  explicit ZoomDecoration(ToolbarModel* toolbar_model);
  virtual ~ZoomDecoration();

  // Called when this decoration should show or hide itself in its most current
  // state.
  void Update(ZoomController* zoom_controller);

 private:
  // LocationBarDecoration implementation.
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;

  // A reference to the toolbar model to query whether input is currently
  // happening in the location bar. Not owned by this class.
  ToolbarModel* toolbar_model_;

  // The string to show for a tooltip.
  scoped_nsobject<NSString> tooltip_;

  DISALLOW_COPY_AND_ASSIGN(ZoomDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ZOOM_DECORATION_H_
