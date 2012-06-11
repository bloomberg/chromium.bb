// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

StatusAreaWidget::StatusAreaWidget() {
  widget_delegate_ = new internal::StatusAreaWidgetDelegate;

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = widget_delegate_;
  params.parent = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      ash::internal::kShellWindowId_StatusContainer);
  params.transparent = true;
  Init(params);
  set_focus_on_creation(false);
  SetContentsView(widget_delegate_);
  GetNativeView()->SetName("StatusAreaWidget");
}

StatusAreaWidget::~StatusAreaWidget() {
}

void StatusAreaWidget::AddTray(views::View* tray) {
  widget_delegate_->AddChildView(tray);
  widget_delegate_->Layout();
}

void StatusAreaWidget::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_BOTTOM)
    widget_delegate_->SetLayout(views::BoxLayout::kHorizontal);
  else
    widget_delegate_->SetLayout(views::BoxLayout::kVertical);
}

}  // namespace internal
}  // namespace ash
