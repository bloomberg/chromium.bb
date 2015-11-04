// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BAR_CONTROL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_BAR_CONTROL_BUTTON_H_

#include "base/callback.h"
#include "base/macros.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
enum class VectorIconId;
}

namespace views {
class InkDropAnimationController;
}

// A class for buttons that control bars (find bar, download shelf, etc.). The
// button has an image and no text.
class BarControlButton : public views::ImageButton, public views::InkDropHost {
 public:
  explicit BarControlButton(views::ButtonListener* listener);
  ~BarControlButton() override;

  // Sets the icon to display and provides a callback which should return the
  // text color from which to derive this icon's color.
  void SetIcon(gfx::VectorIconId id,
               const base::Callback<SkColor(void)>& get_text_color_callback);

  // views::ImageButton:
  void OnThemeChanged() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void NotifyClick(const ui::Event& event) override;

 private:
  // views::InkDropHost:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;

  gfx::VectorIconId id_;
  base::Callback<SkColor(void)> get_text_color_callback_;

  // Animation controller for the ink drop ripple effect.
  scoped_ptr<views::InkDropAnimationController> ink_drop_animation_controller_;

  DISALLOW_COPY_AND_ASSIGN(BarControlButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BAR_CONTROL_BUTTON_H_
