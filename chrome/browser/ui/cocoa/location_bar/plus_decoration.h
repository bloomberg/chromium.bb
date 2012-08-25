// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PLUS_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PLUS_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class Browser;
class CommandUpdater;
class LocationBarViewMac;

// Note: this file is under development (see crbug.com/138118).

// Plus icon on the right side of the location bar.
class PlusDecoration : public ImageDecoration {
 public:
  PlusDecoration(LocationBarViewMac* owner,
      CommandUpdater* command_updater,
      Browser* browser);
  virtual ~PlusDecoration();

  // Helper to get where the action box menu and bubble point should be
  // anchored. Similar to |PageActionDecoration| or |StarDecoration|.
  NSPoint GetActionBoxAnchorPoint();

  // Implement |LocationBarDecoration|.
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;

 private:
  // Owner of the decoration, used to obtain the menu.
  LocationBarViewMac* owner_;

  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  Browser* browser_;

  // The string to show for a tooltip.
  scoped_nsobject<NSString> tooltip_;

  DISALLOW_COPY_AND_ASSIGN(PlusDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PLUS_DECORATION_H_
