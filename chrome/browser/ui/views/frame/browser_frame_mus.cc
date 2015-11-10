// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_mus.h"

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "ui/views/mus/window_manager_connection.h"

BrowserFrameMus::BrowserFrameMus(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetMus(
          browser_frame,
          views::WindowManagerConnection::Get()->app()->shell(),
          views::WindowManagerConnection::Get()->NewWindow(
              std::map<std::string, std::vector<uint8_t>>()),
          mus::mojom::SURFACE_TYPE_DEFAULT),
      browser_view_(browser_view) {
}

BrowserFrameMus::~BrowserFrameMus() {}

views::Widget::InitParams BrowserFrameMus::GetWidgetParams() {
  views::Widget::InitParams params;
  params.native_widget = this;
  params.bounds = gfx::Rect(10, 10, 640, 480);
  return params;
}

bool BrowserFrameMus::UseCustomFrame() const {
  return true;
}

bool BrowserFrameMus::UsesNativeSystemMenu() const {
  return false;
}

bool BrowserFrameMus::ShouldSaveWindowPlacement() const {
  return false;
}

void BrowserFrameMus::GetWindowPlacement(
    gfx::Rect* bounds, ui::WindowShowState* show_state) const {
  *bounds = gfx::Rect(10, 10, 800, 600);
  *show_state = ui::SHOW_STATE_NORMAL;
}

int BrowserFrameMus::GetMinimizeButtonOffset() const {
  return 0;
}
