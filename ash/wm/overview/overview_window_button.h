// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_WINDOW_BUTTON_H_
#define ASH_WM_OVERVIEW_OVERVIEW_WINDOW_BUTTON_H_

#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ui/views/controls/button/button.h"

namespace aura {
class ScopedWindowTargeter;
}

namespace ash {

class OverviewWindowTargeter;

// A class that encapsulates the LabelButton and targeting logic for overview
// mode.
class OverviewWindowButton : public views::ButtonListener {
 public:
  // |target| is the window that should be activated when the button is pressed.
  explicit OverviewWindowButton(aura::Window* target);

  ~OverviewWindowButton() override;

  // Sets the label and targetable bounds.
  void SetBounds(const gfx::Rect& bounds,
                 const OverviewAnimationType& animation_type);

  // Sends an a11y focus alert so that, if chromevox is enabled, the window
  // label is read.
  void SendFocusAlert() const;

  // Sets the label text.
  void SetLabelText(const base::string16& title);

  // Sets the label opacity.
  void SetOpacity(float opacity);

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  friend class WindowSelectorTest;

  // LabelButton shown under each of the windows.
  class OverviewButtonView;

  // Window that the button will activate on click.
  aura::Window* target_;

  // Label under the window displaying its active tab name.
  scoped_ptr<views::Widget> window_label_;

  // View for the label button under the window.
  OverviewButtonView* window_label_button_view_;

  // Reference to the targeter implemented by the scoped window targeter.
  OverviewWindowTargeter* overview_window_targeter_;

  // Stores and restores on exit the actual window targeter.
  scoped_ptr<aura::ScopedWindowTargeter> scoped_window_targeter_;

  DISALLOW_COPY_AND_ASSIGN(OverviewWindowButton);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_WINDOW_BUTTON_H_
