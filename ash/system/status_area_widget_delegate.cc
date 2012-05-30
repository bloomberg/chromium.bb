// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget_delegate.h"

#include "ash/ash_export.h"
#include "ash/focus_cycler.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget.h"

namespace {

int kTraySpacing = 10;

}  // namespace

namespace ash {
namespace internal {

StatusAreaWidgetDelegate::StatusAreaWidgetDelegate()
    : focus_cycler_for_testing_(NULL) {
  SetLayout(views::BoxLayout::kHorizontal);
}

StatusAreaWidgetDelegate::~StatusAreaWidgetDelegate() {
}

void StatusAreaWidgetDelegate::SetFocusCyclerForTesting(
    const FocusCycler* focus_cycler) {
  focus_cycler_for_testing_ = focus_cycler;
}

views::View* StatusAreaWidgetDelegate::GetDefaultFocusableChild() {
  return child_at(0);
}

bool StatusAreaWidgetDelegate::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    RemovePaneFocus();
    GetFocusManager()->ClearFocus();
    return true;
  }
  return false;
}

views::Widget* StatusAreaWidgetDelegate::GetWidget() {
  return View::GetWidget();
}

const views::Widget* StatusAreaWidgetDelegate::GetWidget() const {
  return View::GetWidget();
}

bool StatusAreaWidgetDelegate::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  const FocusCycler* focus_cycler = focus_cycler_for_testing_ ?
      focus_cycler_for_testing_ : Shell::GetInstance()->focus_cycler();
  return focus_cycler->widget_activating() == GetWidget();
}

void StatusAreaWidgetDelegate::DeleteDelegate() {
}

void StatusAreaWidgetDelegate::SetLayout(
    views::BoxLayout::Orientation orientation) {
  SetLayoutManager(new views::BoxLayout(orientation, 0, 0, kTraySpacing));
  Layout();
}

}  // namespace internal
}  // namespace ash
