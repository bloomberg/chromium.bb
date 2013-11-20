// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/input_method/mode_indicator_widget.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "chrome/browser/chromeos/input_method/mode_indicator_view.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_border.h"

namespace chromeos {
namespace input_method {

ModeIndicatorWidget::ModeIndicatorWidget()
    : mode_view_(new input_method::ModeIndicatorView) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);

  // This class is owned by controller impl as well as other components like
  // info_list.
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

  // Show the window always on top
  params.parent = ash::Shell::GetContainer(
      ash::Shell::GetTargetRootWindow(),
      ash::internal::kShellWindowId_InputMethodContainer);

  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  Init(params);
  views::corewm::SetWindowVisibilityAnimationType(
      GetNativeView(),
      views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);

  // Pass the ownership.
  SetContentsView(mode_view_);
}

ModeIndicatorWidget::~ModeIndicatorWidget() {
}

void ModeIndicatorWidget::SetCursorLocation(const gfx::Rect& cursor_location) {
  cursor_location_ = cursor_location;
  gfx::Rect bound(GetClientAreaBoundsInScreen());
  bound.set_x(cursor_location.x() - bound.width() / 2);
  bound.set_y(cursor_location.bottom());
  SetBounds(bound);
}

void ModeIndicatorWidget::SetLabelTextUtf8(const std::string& text_utf8) {
  DCHECK(mode_view_);

  mode_view_->SetLabelTextUtf8(text_utf8);
  SetSize(mode_view_->size());
  SetCursorLocation(cursor_location_);
}

}  // namespace input_method
}  // namespace chromeos
