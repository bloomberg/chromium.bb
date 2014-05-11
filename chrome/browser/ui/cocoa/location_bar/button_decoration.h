// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUTTON_DECORATION_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUTTON_DECORATION_H_

#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_decoration.h"
#import "ui/base/cocoa/appkit_utils.h"

// |LocationBarDecoration| which looks and acts like a button. It has a
// nine-part image as the background, and an icon that is rendered in the center
// of the nine-part image.

class ButtonDecoration : public LocationBarDecoration {
 public:
  enum ButtonState {
    kButtonStateNormal,
    kButtonStateHover,
    kButtonStatePressed
  };

  // Constructs the ButtonDecoration with the specified background 9-part images
  // and icons for the three button states. Also takes the maximum number of
  // pixels of inner horizontal padding to be used between the left/right 9-part
  // images and the icon.
  ButtonDecoration(ui::NinePartImageIds normal_image_ids,
                   int normal_icon_id,
                   ui::NinePartImageIds hover_image_ids,
                   int hover_icon_id,
                   ui::NinePartImageIds pressed_image_ids,
                   int pressed_icon_id,
                   CGFloat max_inner_padding);

  virtual ~ButtonDecoration();

  void SetButtonState(ButtonState state);
  ButtonState GetButtonState() const;

  // Whether a click on this decoration should prevent focusing of the omnibox
  // or not.
  virtual bool PreventFocus(NSPoint location) const;

  // Changes the icon for the specified button state only.
  void SetIcon(ButtonState state, int icon_id);

  // Changes the icon for all button states.
  void SetIcon(int icon_id);

  // Changes the background image for all button states.
  void SetBackgroundImageIds(ui::NinePartImageIds normal_image_ids,
                             ui::NinePartImageIds hover_image_ids,
                             ui::NinePartImageIds pressed_image_ids);

  ui::NinePartImageIds GetBackgroundImageIds() const;
  NSImage* GetIconImage() const;

  // Implement |LocationBarDecoration|.
  virtual CGFloat GetWidthForSpace(CGFloat width) OVERRIDE;
  virtual void DrawInFrame(NSRect frame, NSView* control_view) OVERRIDE;
  virtual bool AcceptsMousePress() OVERRIDE;
  virtual bool IsDraggable() OVERRIDE;
  virtual bool OnMousePressed(NSRect frame, NSPoint location) OVERRIDE;
  virtual ButtonDecoration* AsButtonDecoration() OVERRIDE;

 private:
  ui::NinePartImageIds normal_image_ids_;
  ui::NinePartImageIds hover_image_ids_;
  ui::NinePartImageIds pressed_image_ids_;
  int normal_icon_id_;
  int hover_icon_id_;
  int pressed_icon_id_;
  ButtonState state_;
  CGFloat max_inner_padding_;

  DISALLOW_COPY_AND_ASSIGN(ButtonDecoration);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_BUTTON_DECORATION_H_
