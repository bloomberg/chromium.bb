// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/status_area/status_area_view.h"

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
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

StatusAreaView::StatusAreaView()
    : focus_cycler_for_testing_(NULL) {
}

StatusAreaView::~StatusAreaView() {
}

void StatusAreaView::SetFocusCyclerForTesting(const FocusCycler* focus_cycler) {
  focus_cycler_for_testing_ = focus_cycler;
}

bool StatusAreaView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    RemovePaneFocus();
    GetFocusManager()->ClearFocus();
    return true;
  }
  return false;
}

views::Widget* StatusAreaView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* StatusAreaView::GetWidget() const {
  return View::GetWidget();
}

bool StatusAreaView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  const FocusCycler* focus_cycler = focus_cycler_for_testing_ ?
      focus_cycler_for_testing_ : Shell::GetInstance()->focus_cycler();
  return focus_cycler->widget_activating() == GetWidget();
}

void StatusAreaView::DeleteDelegate() {
  // If this is used as the content-view of the widget, then do nothing, since
  // deleting the widget will end up deleting this. But if this is used only as
  // the widget-delegate, then delete this now.
  if (!GetWidget())
    delete this;
}

ASH_EXPORT views::Widget* CreateStatusArea(views::View* contents) {
  StatusAreaView* status_area_view = new StatusAreaView;
  if (!contents)
    contents = status_area_view;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  gfx::Size ps = contents ? contents->GetPreferredSize() : gfx::Size(200, 29);
  params.bounds = gfx::Rect(0, 0, ps.width(), ps.height());
  params.delegate = status_area_view;
  params.parent = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_StatusContainer);
  params.transparent = true;
  widget->Init(params);
  // TODO(sky): We need the contents to be an AccessiblePaneView for
  // FocusCycler. SystemTray isn't an AccessiblePaneView, so we wrap it in
  // one. This is a bit of a hack, but at this point this code path is only used
  // for tests. Once the migration to SystemTray is done this method should no
  // longer be needed and we can nuke this.
  views::AccessiblePaneView* accessible_pane =
      new views::AccessiblePaneView;
  accessible_pane->SetLayoutManager(new views::FillLayout);
  accessible_pane->AddChildView(contents);
  widget->set_focus_on_creation(false);
  widget->SetContentsView(accessible_pane);
  widget->Show();
  widget->GetNativeView()->SetName("StatusAreaView");
  return widget;
}

}  // namespace internal
}  // namespace ash
