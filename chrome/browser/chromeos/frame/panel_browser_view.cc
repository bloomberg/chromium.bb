// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/panel_browser_view.h"

#include "chrome/browser/chromeos/frame/panel_controller.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "views/window/window.h"

namespace chromeos {

PanelBrowserView::PanelBrowserView(Browser* browser)
    : BrowserView(browser),
      creator_xid_(0) {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserView overrides.

void PanelBrowserView::Init() {
  BrowserView::Init();
  // The visibility of toolbar is controlled in
  // the BrowserView::IsToolbarVisible method.

  views::Window* window = frame()->GetWindow();
  gfx::NativeWindow native_window = window->GetNativeWindow();
  // The window manager needs the min size for popups.
  gfx::Rect bounds = window->GetBounds();
  gtk_widget_set_size_request(
      GTK_WIDGET(native_window), bounds.width(), bounds.height());
  // If we don't explicitly resize here there is a race condition between
  // the X Server and the window manager. Windows will appear with a default
  // size of 200x200 if this happens.
  gtk_window_resize(native_window, bounds.width(), bounds.height());
}

void PanelBrowserView::Show() {
  panel_controller_.reset(new PanelController(this, GetNativeHandle()));
  panel_controller_->Init(
      true /* focus when opened */, bounds(), creator_xid_,
      WM_IPC_PANEL_USER_RESIZE_HORIZONTALLY_AND_VERTICALLY);
  BrowserView::Show();
}

void PanelBrowserView::Close() {
  BrowserView::Close();
  if (panel_controller_.get())
    panel_controller_->Close();
}

void PanelBrowserView::UpdateTitleBar() {
  BrowserView::UpdateTitleBar();
  if (panel_controller_.get())
    panel_controller_->UpdateTitleBar();
}

void PanelBrowserView::ActivationChanged(bool activated) {
  BrowserView::ActivationChanged(activated);
  if (panel_controller_.get()) {
    if (activated)
      panel_controller_->OnFocusIn();
    else
      panel_controller_->OnFocusOut();
  }
}

void PanelBrowserView::SetCreatorView(PanelBrowserView* creator) {
  DCHECK(creator);
  GtkWindow* window = creator->GetNativeHandle();
  creator_xid_ = x11_util::GetX11WindowFromGtkWidget(GTK_WIDGET(window));
}

////////////////////////////////////////////////////////////////////////////////
// PanelController::Delegate overrides.

string16 PanelBrowserView::GetPanelTitle() {
  return browser()->GetWindowTitleForCurrentTab();
}

SkBitmap PanelBrowserView::GetPanelIcon() {
  return browser()->GetCurrentPageIcon();
}

void PanelBrowserView::ClosePanel() {
  Close();
}

}  // namespace chromeos
