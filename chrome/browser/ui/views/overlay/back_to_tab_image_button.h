// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_IMAGE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/views/controls/button/image_button.h"

namespace views {

// An image button representing a back-to-tab button. When user hovers/focuses
// the button, a grey circular background appears as an indicator.
class BackToTabImageButton : public views::ImageButton {
 public:
  explicit BackToTabImageButton(ButtonListener*);
  ~BackToTabImageButton() override = default;

  // views::Button:
  void StateChanged(ButtonState old_state) override;

  // views::View:
  void OnFocus() override;
  void OnBlur() override;

  // Sets the position of itself with an offset from the given window size.
  void SetPosition(const gfx::Size& size,
                   OverlayWindowViews::WindowQuadrant quadrant);

 private:
  const gfx::ImageSkia back_to_tab_background_;

  DISALLOW_COPY_AND_ASSIGN(BackToTabImageButton);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_IMAGE_BUTTON_H_
