// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_WEB_INTENTS_BUTTON_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_WEB_INTENTS_BUTTON_DECORATION_H_

#include <string>

#import <Cocoa/Cocoa.h>

#include "base/string16.h"
#include "chrome/browser/ui/cocoa/location_bar/bubble_decoration.h"

class LocationBarViewMac;
class TabContents;
@class WebIntentsButtonAnimationState;

class WebIntentsButtonDecoration : public BubbleDecoration {
 public:
  WebIntentsButtonDecoration(LocationBarViewMac* owner, NSFont* font);
  virtual ~WebIntentsButtonDecoration();

  // LocationBarDecoration
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame) OVERRIDE;
  virtual CGFloat GetWidthForSpace(CGFloat width) OVERRIDE;
  virtual void DrawInFrame(NSRect frame, NSView* control_view) OVERRIDE;

  // Optionally display the web intents button.
  void Update(TabContents* tab_contents);

  // Called from internal animator.
  virtual void AnimationTimerFired();

 private:
  friend class WebIntentsButtonDecorationTest;

  // Returns an attributed string with the animated text. Caller is responsible
  // for releasing.
  NSAttributedString* CreateAnimatedText();

  // Measure the width of the animated text.
  CGFloat MeasureTextWidth();

  LocationBarViewMac* owner_;  // weak

  // Used when the decoration has animated text.
  scoped_nsobject<WebIntentsButtonAnimationState> animation_;
  CGFloat textWidth_;
  scoped_nsobject<NSAttributedString> animatedText_;
  scoped_nsobject<NSGradient> gradient_;
  bool ranAnimation_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsButtonDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_WEB_INTENTS_BUTTON_DECORATION_H_
