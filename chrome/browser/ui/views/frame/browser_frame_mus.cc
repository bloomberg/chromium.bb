// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_mus.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/window_manager_frame_values.h"

BrowserFrameMus::BrowserFrameMus(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::DesktopNativeWidgetAura(browser_frame),
      browser_frame_(browser_frame),
      browser_view_(browser_view) {}

BrowserFrameMus::~BrowserFrameMus() {}

views::Widget::InitParams BrowserFrameMus::GetWidgetParams() {
  views::Widget::InitParams params;
  params.native_widget = this;
  params.bounds = gfx::Rect(10, 10, 640, 480);
  params.delegate = browser_view_;
  std::map<std::string, std::vector<uint8_t>> properties =
      views::MusClient::ConfigurePropertiesFromParams(params);
  const std::string chrome_app_id(extension_misc::kChromeAppId);
  // Indicates mash shouldn't handle immersive, rather we will.
  properties[ui::mojom::WindowManager::kDisableImmersive_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(true);
  properties[ui::mojom::WindowManager::kAppID_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(chrome_app_id);
  std::unique_ptr<views::DesktopWindowTreeHostMus> desktop_window_tree_host =
      base::MakeUnique<views::DesktopWindowTreeHostMus>(browser_frame_, this,
                                                        &properties);
  // BrowserNonClientFrameViewMus::OnBoundsChanged() takes care of updating
  // the insets.
  desktop_window_tree_host->set_auto_update_client_area(false);
  SetDesktopWindowTreeHost(std::move(desktop_window_tree_host));
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

bool BrowserFrameMus::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

bool BrowserFrameMus::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

int BrowserFrameMus::GetMinimizeButtonOffset() const {
  return 0;
}
