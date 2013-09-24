// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUBBLE_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUBBLE_DECORATION_H_

#import <Cocoa/Cocoa.h>

#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"
#import "ui/base/cocoa/appkit_utils.h"

// Draws an outlined rounded rect, with an optional image to the left
// and an optional text label to the right.

class BubbleDecoration : public LocationBarDecoration {
 public:
  BubbleDecoration();
  virtual ~BubbleDecoration();

  // Setup the drawing parameters.
  NSImage* GetImage();
  void SetImage(NSImage* image);
  void SetLabel(NSString* label);
  void SetTextColor(NSColor* text_color);
  virtual ui::NinePartImageIds GetBubbleImageIds() = 0;

  // Implement |LocationBarDecoration|.
  virtual void DrawInFrame(NSRect frame, NSView* control_view) OVERRIDE;
  virtual void DrawWithBackgroundInFrame(NSRect background_frame,
                                         NSRect frame,
                                         NSView* control_view) OVERRIDE;
  virtual CGFloat GetWidthForSpace(CGFloat width) OVERRIDE;

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

  // Image drawn in the left side of the bubble.
  base::scoped_nsobject<NSImage> image_;

  // Label to draw to right of image.  Can be |nil|.
  base::scoped_nsobject<NSString> label_;

  // Contains attribute for drawing |label_|.
  base::scoped_nsobject<NSMutableDictionary> attributes_;

  DISALLOW_COPY_AND_ASSIGN(BubbleDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUBBLE_DECORATION_H_
