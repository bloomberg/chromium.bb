// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_CHROME_TO_MOBILE_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_CHROME_TO_MOBILE_DECORATION_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class CommandUpdater;
class Profile;

// Chrome To Mobile icon on the right side of the field.
class ChromeToMobileDecoration : public ImageDecoration {
 public:
  ChromeToMobileDecoration(Profile* profile, CommandUpdater* command_updater);
  virtual ~ChromeToMobileDecoration();

  // Get the point where the bubble should point within the decoration's frame.
  NSPoint GetBubblePointInFrame(NSRect frame);

  // Update the icon to reflect the lit or unlit state.
  void SetLit(bool lit);

  // Implement |LocationBarDecoration|.
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;

 private:
  Profile* profile_;

  // Used for showing the Chrome To Mobile bubble.
  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  // The string to show for a tooltip.
  scoped_nsobject<NSString> tooltip_;

  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_CHROME_TO_MOBILE_DECORATION_H_
