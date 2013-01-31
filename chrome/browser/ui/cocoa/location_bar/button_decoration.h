// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUTTON_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUTTON_DECORATION_H_

#import "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"

// |LocationBarDecoration| which looks and acts like a button.

class ButtonDecoration : public LocationBarDecoration {
 public:
  enum ButtonState {
    kButtonStateNormal,
    kButtonStateHover,
    kButtonStatePressed
  };

  ButtonDecoration();
  virtual ~ButtonDecoration();

  void SetButtonState(ButtonState state);
  ButtonState GetButtonState() const;

  // To be called when a mouse click occurs within the decoration, which will
  // set and reset the button's state as necessary before calling into
  // |OnMousePressed| below.
  bool OnMousePressedWithView(NSRect frame, NSView* control_view);

  // Implement |LocationBarDecoration|.
  virtual CGFloat GetWidthForSpace(CGFloat width, CGFloat text_width) OVERRIDE;
  virtual void DrawInFrame(NSRect frame, NSView* control_view) OVERRIDE;
  virtual bool OnMousePressed(NSRect frame) OVERRIDE;
  virtual ButtonDecoration* AsButtonDecoration() OVERRIDE;

 protected:
  // Setters for the images for different states.
  void SetNormalImage(NSImage* normal_image);
  void SetHoverImage(NSImage* hover_image);
  void SetPressedImage(NSImage* pressed_image);

 private:
  scoped_nsobject<NSImage> normal_image_;
  scoped_nsobject<NSImage> hover_image_;
  scoped_nsobject<NSImage> pressed_image_;
  ButtonState state_;

  NSImage* GetImage();

  DISALLOW_COPY_AND_ASSIGN(ButtonDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUTTON_DECORATION_H_
