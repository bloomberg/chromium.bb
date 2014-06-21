// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_BUTTON_FROM_VIEW_H_
#define ASH_SYSTEM_USER_BUTTON_FROM_VIEW_H_

#include "base/macros.h"
#include "ui/gfx/insets.h"
#include "ui/views/controls/button/custom_button.h"

namespace ash {
namespace tray {

// This view is used to wrap it's content and transform it into button.
class ButtonFromView : public views::CustomButton {
 public:
  // The |content| is the content which is shown within the button. The
  // |button_listener| will be informed - if provided - when a button was
  // pressed. If |highlight_on_hover| is set to true, the button will be
  // highlighted upon hover and show the accessibility caret.
  // The |tab_frame_inset| will be used to inset the blue tab frame inside the
  // button.
  ButtonFromView(views::View* content,
                 views::ButtonListener* listener,
                 bool highlight_on_hover,
                 const gfx::Insets& tab_frame_inset);
  virtual ~ButtonFromView();

  // Called when the border should remain even in the non highlighted state.
  void ForceBorderVisible(bool show);

  // Overridden from views::View
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // Check if the item is hovered.
  bool is_hovered_for_test() { return button_hovered_; }

 private:
  // Change the hover/active state of the "button" when the status changes.
  void ShowActive();

  // Content of button.
  views::View* content_;

  // Whether button should be highligthed on hover.
  bool highlight_on_hover_;

  // True if button is hovered.
  bool button_hovered_;

  // True if the border should be always visible.
  bool show_border_;

  // The insets which get used for the drawn accessibility (tab) frame.
  gfx::Insets tab_frame_inset_;

  DISALLOW_COPY_AND_ASSIGN(ButtonFromView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_USER_BUTTON_FROM_VIEW_H_
