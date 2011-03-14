// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/panel_browser_view.h"

#include "chrome/browser/chromeos/frame/panel_controller.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace {

const int kPanelMinWidthPixels = 100;
const int kPanelMinHeightPixels = 100;
const int kPanelDefaultWidthPixels = 250;
const int kPanelDefaultHeightPixels = 300;
const float kPanelMaxWidthFactor = 0.80;
const float kPanelMaxHeightFactor = 0.80;

}

namespace chromeos {

PanelBrowserView::PanelBrowserView(Browser* browser)
    : BrowserView(browser),
      creator_xid_(0) {
}

////////////////////////////////////////////////////////////////////////////////
// PanelBrowserView functions

void PanelBrowserView::LimitBounds(gfx::Rect* bounds) const {
  GdkScreen* screen = gtk_widget_get_screen(GetWidget()->GetNativeView());
  int max_width = gdk_screen_get_width(screen) * kPanelMaxWidthFactor;
  int max_height = gdk_screen_get_height(screen) * kPanelMaxHeightFactor;

  if (bounds->width() == 0 && bounds->height() == 0) {
    bounds->set_width(kPanelDefaultWidthPixels);
    bounds->set_height(kPanelDefaultHeightPixels);
  }

  if (bounds->width() < kPanelMinWidthPixels)
    bounds->set_width(kPanelMinWidthPixels);
  else if (bounds->width() > max_width)
    bounds->set_width(max_width);

  if (bounds->height() < kPanelMinHeightPixels)
    bounds->set_height(kPanelMinHeightPixels);
  else if (bounds->height() > max_height)
    bounds->set_height(max_height);
}


////////////////////////////////////////////////////////////////////////////////
// BrowserView overrides.

void PanelBrowserView::Show() {
  if (panel_controller_.get() == NULL) {
    panel_controller_.reset(new PanelController(this, GetNativeHandle()));
    panel_controller_->Init(
        true /* focus when opened */, bounds(), creator_xid_,
        WM_IPC_PANEL_USER_RESIZE_HORIZONTALLY_AND_VERTICALLY);
  }
  ::BrowserView::Show();
}

void PanelBrowserView::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect limit_bounds = bounds;
  LimitBounds(&limit_bounds);
  ::BrowserView::SetBounds(limit_bounds);
}

void PanelBrowserView::Close() {
  ::BrowserView::Close();
  if (panel_controller_.get())
    panel_controller_->Close();
}

void PanelBrowserView::UpdateTitleBar() {
  ::BrowserView::UpdateTitleBar();
  if (panel_controller_.get())
    panel_controller_->UpdateTitleBar();
}

void PanelBrowserView::SetCreatorView(PanelBrowserView* creator) {
  DCHECK(creator);
  GtkWindow* window = creator->GetNativeHandle();
  creator_xid_ = ui::GetX11WindowFromGtkWidget(GTK_WIDGET(window));
}

bool PanelBrowserView::GetSavedWindowBounds(gfx::Rect* bounds) const {
  bool res = ::BrowserView::GetSavedWindowBounds(bounds);
  if (res)
    LimitBounds(bounds);
  return res;
}

void PanelBrowserView::OnWindowActivate(bool active) {
  ::BrowserView::OnWindowActivate(active);
  if (panel_controller_.get()) {
    if (active)
      panel_controller_->OnFocusIn();
    else
      panel_controller_->OnFocusOut();
  }
}

////////////////////////////////////////////////////////////////////////////////
// PanelController::Delegate overrides.

string16 PanelBrowserView::GetPanelTitle() {
  return browser()->GetWindowTitleForCurrentTab();
}

SkBitmap PanelBrowserView::GetPanelIcon() {
  return browser()->GetCurrentPageIcon();
}

bool PanelBrowserView::CanClosePanel() {
  return ::BrowserView::CanClose();
}

void PanelBrowserView::ClosePanel() {
  Close();
}

}  // namespace chromeos
