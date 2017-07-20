// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_window_views.h"

#include "ui/views/widget/widget.h"

// static
std::unique_ptr<OverlayWindow> OverlayWindow::Create() {
  return base::WrapUnique(new OverlayWindowViews());
}

OverlayWindowViews::OverlayWindowViews() {
  widget_.reset(new views::Widget());
}

OverlayWindowViews::~OverlayWindowViews() = default;

void OverlayWindowViews::Init() {
  // TODO(apacible): Finalize the type of widget. http://crbug/726621
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

  // TODO(apacible): Update preferred sizing and positioning.
  // http://crbug/726621
  params.bounds = gfx::Rect(200, 200, 700, 500);
  params.keep_on_top = true;
  params.visible_on_all_workspaces = true;

  widget_->Init(params);
  widget_->Show();

  // TODO(apacible): Set the WidgetDelegate for more control over behavior.
  // http://crbug/726621
}

bool OverlayWindowViews::IsActive() const {
  return widget_->IsActive();
}

void OverlayWindowViews::Show() {
  widget_->Show();
}

void OverlayWindowViews::Hide() {
  widget_->Hide();
}

void OverlayWindowViews::Close() {
  widget_->Close();
}

void OverlayWindowViews::Activate() {
  widget_->Activate();
}

bool OverlayWindowViews::IsAlwaysOnTop() const {
  return true;
}

ui::Layer* OverlayWindowViews::GetLayer() {
  return widget_->GetLayer();
}

gfx::NativeWindow OverlayWindowViews::GetNativeWindow() const {
  return widget_->GetNativeWindow();
}

gfx::Rect OverlayWindowViews::GetBounds() const {
  return widget_->GetRestoredBounds();
}
