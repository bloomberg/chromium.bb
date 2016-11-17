// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_USER_BUTTON_FROM_VIEW_H_
#define ASH_COMMON_SYSTEM_USER_BUTTON_FROM_VIEW_H_

#include <memory>

#include "ash/common/system/tray/tray_popup_ink_drop_style.h"
#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {
class InkDropContainerView;
}  // namespace views

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
  // An accessible label gets computed based upon descendant views of this view.
  ButtonFromView(views::View* content,
                 views::ButtonListener* listener,
                 TrayPopupInkDropStyle ink_drop_style,
                 bool highlight_on_hover,
                 const gfx::Insets& tab_frame_inset);
  ~ButtonFromView() override;

  // Called when the border should remain even in the non highlighted state.
  void ForceBorderVisible(bool show);

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void Layout() override;

  // Check if the item is hovered.
  bool is_hovered_for_test() { return button_hovered_; }

 protected:
  // views::CustomButton:
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

 private:
  // Change the hover/active state of the "button" when the status changes.
  void ShowActive();

  // Content of button.
  views::View* content_;

  // Defines the flavor of ink drop ripple/highlight that should be constructed.
  TrayPopupInkDropStyle ink_drop_style_;

  // Whether button should be highligthed on hover.
  bool highlight_on_hover_;

  // True if button is hovered.
  bool button_hovered_;

  // True if the border should be always visible.
  bool show_border_;

  // The insets which get used for the drawn accessibility (tab) frame.
  gfx::Insets tab_frame_inset_;

  // A separate view is necessary to hold the ink drop layer so that |this| can
  // host labels with subpixel anti-aliasing enabled. Only used for material
  // design.
  views::InkDropContainerView* ink_drop_container_;

  std::unique_ptr<views::InkDropMask> ink_drop_mask_;

  DISALLOW_COPY_AND_ASSIGN(ButtonFromView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_USER_BUTTON_FROM_VIEW_H_
