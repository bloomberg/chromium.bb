// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_SEPARATOR_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_SEPARATOR_DECORATION_H_

#include "base/compiler_specific.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

// This class is used to draw a separator between decorations in the omnibox.
class SeparatorDecoration : public LocationBarDecoration {
 public:
  SeparatorDecoration();
  virtual ~SeparatorDecoration();

  // Implement LocationBarDecoration:
  virtual void DrawInFrame(NSRect frame, NSView* control_view) OVERRIDE;
  virtual CGFloat GetWidthForSpace(CGFloat width, CGFloat text_width) OVERRIDE;
  virtual bool IsSeparator() const OVERRIDE;

 private:
  NSColor* SeparatorColor(NSView* view) const;

  DISALLOW_COPY_AND_ASSIGN(SeparatorDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_SEPARATOR_DECORATION_H_
