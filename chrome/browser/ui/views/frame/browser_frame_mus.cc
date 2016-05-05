// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_mus.h"

#include <stdint.h>

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/mus/window_manager_frame_values.h"

namespace {

views::Widget::InitParams GetWidgetParamsImpl(BrowserView* browser_view) {
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(10, 10, 640, 480);
  params.delegate = browser_view;
  return params;
}

mus::Window* CreateMusWindow(BrowserView* browser_view) {
  std::map<std::string, std::vector<uint8_t>> properties;
  views::NativeWidgetMus::ConfigurePropertiesForNewWindow(
      GetWidgetParamsImpl(browser_view), &properties);
  const std::string chrome_app_id(extension_misc::kChromeAppId);
  properties[mus::mojom::WindowManager::kAppID_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(chrome_app_id);
  return views::WindowManagerConnection::Get()->NewWindow(properties);
}

}  // namespace

BrowserFrameMus::BrowserFrameMus(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetMus(
          browser_frame,
          views::WindowManagerConnection::Get()->connector(),
          CreateMusWindow(browser_view),
          mus::mojom::SurfaceType::DEFAULT),
      browser_view_(browser_view) {}

BrowserFrameMus::~BrowserFrameMus() {}

views::Widget::InitParams BrowserFrameMus::GetWidgetParams() {
  views::Widget::InitParams params(GetWidgetParamsImpl(browser_view_));
  params.native_widget = this;
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

void BrowserFrameMus::UpdateClientArea() {
  // BrowserNonClientFrameViewMus::OnBoundsChanged() takes care of updating
  // the insets.
}
