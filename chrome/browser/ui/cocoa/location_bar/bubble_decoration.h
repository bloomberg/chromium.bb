// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUBBLE_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUBBLE_DECORATION_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/gtest_prod_util.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

// Draws an outlined rounded rect, with an optional image to the left
// and an optional text label to the right.

class BubbleDecoration : public LocationBarDecoration {
 public:
  // |font| will be used when drawing the label, and cannot be |nil|.
  BubbleDecoration(NSFont* font);
  virtual ~BubbleDecoration();

  // Setup the drawing parameters.
  NSImage* GetImage();
  void SetImage(NSImage* image);
  void SetLabel(NSString* label);
  void SetColors(NSColor* border_color,
                 NSColor* background_color,
                 NSColor* text_color);

  // Implement |LocationBarDecoration|.
  virtual void DrawInFrame(NSRect frame, NSView* control_view);
  virtual CGFloat GetWidthForSpace(CGFloat width);

 protected:
  // Helper returning bubble width for the given |image| and |label|
  // assuming |font_| (for sizing text).  Arguments can be nil.
  CGFloat GetWidthForImageAndLabel(NSImage* image, NSString* label);

  // Helper to return where the image is drawn, for subclasses to drag
  // from.  |frame| is the decoration's frame in the containing cell.
  NSRect GetImageRectInFrame(NSRect frame);

 private:
  friend class SelectedKeywordDecorationTest;
  FRIEND_TEST_ALL_PREFIXES(SelectedKeywordDecorationTest,
                           UsesPartialKeywordIfNarrow);

  // Contains font and color attribute for drawing |label_|.
  scoped_nsobject<NSDictionary> attributes_;

  // Image drawn in the left side of the bubble.
  scoped_nsobject<NSImage> image_;

  // Label to draw to right of image.  Can be |nil|.
  scoped_nsobject<NSString> label_;

  // Colors used to draw the bubble, should be set by the subclass
  // constructor.
  scoped_nsobject<NSColor> background_color_;
  scoped_nsobject<NSColor> border_color_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUBBLE_DECORATION_H_
