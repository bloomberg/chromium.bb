// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/background_view.h"

#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/status/clock_menu_button.h"
#include "chrome/browser/chromeos/status/network_menu_button.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/common/x11_util.h"
#include "views/screen.h"
#include "views/widget/widget_gtk.h"

// X Windows headers have "#define Status int". That interferes with
// NetworkLibrary header which defines enum "Status".
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>

namespace chromeos {

BackgroundView::BackgroundView() : status_area_(NULL) {
  views::Painter* painter = chromeos::CreateWizardPainter(
      &chromeos::BorderDefinition::kWizardBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));
  InitStatusArea();
}

void BackgroundView::Init() {
  InitStatusArea();
  Layout();
  status_area_->SchedulePaint();
}

void BackgroundView::Teardown() {
  RemoveAllChildViews(true);
  status_area_ = NULL;
}

static void ResetXCursor() {
  // TODO(sky): nuke this once new window manager is in place.
  // This gets rid of the ugly X default cursor.
  Display* display = x11_util::GetXDisplay();
  Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
  XID root_window = x11_util::GetX11RootWindow();
  XSetWindowAttributes attr;
  attr.cursor = cursor;
  XChangeWindowAttributes(display, root_window, CWCursor, &attr);
}

// static
views::Widget* BackgroundView::CreateWindowContainingView(
    const gfx::Rect& bounds,
    BackgroundView** view) {
  ResetXCursor();

  views::WidgetGtk* window =
      new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  window->Init(NULL, bounds);
  chromeos::WmIpc::instance()->SetWindowType(
      window->GetNativeView(),
      chromeos::WmIpc::WINDOW_TYPE_LOGIN_BACKGROUND,
      NULL);
  *view = new BackgroundView();
  window->SetContentsView(*view);

  // This keeps the window from flashing at startup.
  GdkWindow* gdk_window = window->GetNativeView()->window;
  gdk_window_set_back_pixmap(gdk_window, NULL, false);

  return window;
}

void BackgroundView::Layout() {
  int right_top_padding =
      chromeos::BorderDefinition::kWizardBorder.padding +
      chromeos::BorderDefinition::kWizardBorder.corner_radius / 2;
  gfx::Size status_area_size = status_area_->GetPreferredSize();
  status_area_->SetBounds(
      width() - status_area_size.width() - right_top_padding,
      right_top_padding,
      status_area_size.width(),
      status_area_size.height());
}

gfx::NativeWindow BackgroundView::GetNativeWindow() const {
  return
      GTK_WINDOW(static_cast<views::WidgetGtk*>(GetWidget())->GetNativeView());
}

bool BackgroundView::ShouldOpenButtonOptions(
    const views::View* button_view) const {
  if (button_view == status_area_->clock_view() ||
      button_view == status_area_->network_view()) {
    return false;
  }
  return true;
}

void BackgroundView::OpenButtonOptions(const views::View* button_view) const {
  // TODO(avayvod): Add some dialog for options or remove them completely.
}

bool BackgroundView::IsButtonVisible(const views::View* button_view) const {
  return true;
}

void BackgroundView::InitStatusArea() {
  DCHECK(status_area_ == NULL);
  status_area_ = new StatusAreaView(this);
  status_area_->Init();
  AddChildView(status_area_);
}

}  // namespace chromeos
