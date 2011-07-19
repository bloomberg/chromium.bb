// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_STAR_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_STAR_DECORATION_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class CommandUpdater;

// Star icon on the right side of the field.

class StarDecoration : public ImageDecoration {
 public:
  explicit StarDecoration(CommandUpdater* command_updater);
  virtual ~StarDecoration();

  // Sets the image and tooltip based on |starred|.
  void SetStarred(bool starred);

  // Get the point where the bookmark bubble should point within the
  // decoration's frame.
  NSPoint GetBubblePointInFrame(NSRect frame);

  // Implement |LocationBarDecoration|.
  virtual bool AcceptsMousePress();
  virtual bool OnMousePressed(NSRect frame);
  virtual NSString* GetToolTip();

 private:
  // For bringing up bookmark bar.
  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  // The string to show for a tooltip.
  scoped_nsobject<NSString> tooltip_;

  DISALLOW_COPY_AND_ASSIGN(StarDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_STAR_DECORATION_H_
