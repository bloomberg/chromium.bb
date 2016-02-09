// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BAR_CONTROL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_BAR_CONTROL_BUTTON_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
enum class VectorIconId;
}

namespace views {
class InkDropDelegate;
}

// A class for buttons that control bars (find bar, download shelf, etc.). The
// button has an image and no text.
class BarControlButton : public views::ImageButton {
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

 private:
  gfx::VectorIconId id_;
  base::Callback<SkColor(void)> get_text_color_callback_;

  // Controls the visual feedback for the button state.
  scoped_ptr<views::InkDropDelegate> ink_drop_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BarControlButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BAR_CONTROL_BUTTON_H_
