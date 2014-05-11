// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_MIC_SEARCH_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_MIC_SEARCH_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class CommandUpdater;

// Draws a microphone icon on the right side of the omnibox. This is used for
// voice search.
class MicSearchDecoration : public ImageDecoration {
 public:
  explicit MicSearchDecoration(CommandUpdater* command_updater);
  virtual ~MicSearchDecoration();

  // Implement |LocationBarDecoration|.
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame, NSPoint location) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;

 private:
  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  DISALLOW_COPY_AND_ASSIGN(MicSearchDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_MIC_SEARCH_DECORATION_H_
