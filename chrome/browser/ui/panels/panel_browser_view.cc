// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_view.h"

#include "base/logging.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/window/window.h"

BrowserWindow* Panel::CreateNativePanel(Browser* browser, Panel* panel) {
  BrowserView* view = new PanelBrowserView(browser, panel);
  (new BrowserFrame(view))->InitBrowserFrame();
  view->GetWidget()->SetAlwaysOnTop(true);
  view->GetWindow()->non_client_view()->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  return view;
}

PanelBrowserView::PanelBrowserView(Browser* browser, Panel* panel)
  : BrowserView(browser),
    panel_(panel),
    mouse_pressed_(false),
    mouse_dragging_(false) {
}

PanelBrowserView::~PanelBrowserView() {
}

void PanelBrowserView::Close() {
  if (!panel_)
    return;

  // Check if the panel is in the closing process, i.e. Panel::Close() is
  // called.
#ifndef NDEBUG
  DCHECK(panel_->closing());
#endif

  ::BrowserView::Close();
  panel_ = NULL;
}

void PanelBrowserView::UpdateTitleBar() {
  ::BrowserView::UpdateTitleBar();
  GetFrameView()->UpdateTitleBar();
}

bool PanelBrowserView::GetSavedWindowBounds(gfx::Rect* bounds) const {
  *bounds = panel_->GetRestoredBounds();
  return true;
}

void PanelBrowserView::OnWindowActivationChanged(bool active) {
  ::BrowserView::OnWindowActivationChanged(active);
  GetFrameView()->OnActivationChanged(active);
}

PanelBrowserFrameView* PanelBrowserView::GetFrameView() const {
  return static_cast<PanelBrowserFrameView*>(frame()->GetFrameView());
}

bool PanelBrowserView::OnTitleBarMousePressed(const views::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;
  mouse_pressed_ = true;
  mouse_pressed_point_ = event.location();
  mouse_dragging_ = false;
  panel_->manager()->StartDragging(panel_);
  return true;
}

bool PanelBrowserView::OnTitleBarMouseDragged(const views::MouseEvent& event) {
  if (!mouse_pressed_)
    return false;

  // We do not allow dragging vertically.
  int delta_x = event.location().x() - mouse_pressed_point_.x();
  if (!mouse_dragging_ && ExceededDragThreshold(delta_x, 0))
    mouse_dragging_ = true;
  if (mouse_dragging_)
    panel_->manager()->Drag(delta_x);
  return true;
}

bool PanelBrowserView::OnTitleBarMouseReleased(const views::MouseEvent& event) {
  // Only handle clicks that started in our window.
  if (!mouse_pressed_)
    return false;
  mouse_pressed_ = false;

  if (mouse_dragging_) {
    mouse_dragging_ = false;
    panel_->manager()->EndDragging(false);
  } else {
    panel_->manager()->EndDragging(true);
  }
  return true;
}
