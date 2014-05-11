// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_TRANSLATE_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_TRANSLATE_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/location_bar/image_decoration.h"

class CommandUpdater;
@class TranslateBubbleController;

// Translate icon on the right side of the field. This appears when
// translation is available on the current page. This icon is lit when
// translation is already done, otherwise not lit.
class TranslateDecoration : public ImageDecoration {
 public:
  explicit TranslateDecoration(CommandUpdater* command_updater);
  virtual ~TranslateDecoration();

  // Toggles the icon on or off.
  void SetLit(bool on);

  // Implement |LocationBarDecoration|
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame, NSPoint location) OVERRIDE;
  virtual NSString* GetToolTip() OVERRIDE;
  virtual NSPoint GetBubblePointInFrame(NSRect frame) OVERRIDE;

 private:
  // For showing the translate bubble up.
  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  DISALLOW_COPY_AND_ASSIGN(TranslateDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_TRANSLATE_DECORATION_H_
