// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/panel_browser_view.h"

#include "chrome/browser/chromeos/panel_controller.h"
#include "views/window/window.h"

namespace chromeos {

PanelBrowserView::PanelBrowserView(Browser* browser)
    : BrowserView(browser) {
}

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
  panel_controller_.reset(new chromeos::PanelController(this));
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

}  // namespace chromeos
