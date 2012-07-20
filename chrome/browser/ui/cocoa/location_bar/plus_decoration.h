// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PLUS_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PLUS_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class CommandUpdater;

// Note: this file is under development (see crbug.com/138118).

// Plus icon on the right side of the location bar.
class PlusDecoration : public ImageDecoration {
 public:
  explicit PlusDecoration(CommandUpdater* command_updater);
  virtual ~PlusDecoration();

  // Implement |LocationBarDecoration|.
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;

 private:
  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  // The string to show for a tooltip.
  scoped_nsobject<NSString> tooltip_;

  DISALLOW_COPY_AND_ASSIGN(PlusDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_PLUS_DECORATION_H_
