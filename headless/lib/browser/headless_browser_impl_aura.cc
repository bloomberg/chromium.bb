// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_focus_client.h"
#include "headless/lib/browser/headless_screen.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

void HeadlessBrowserImpl::PlatformInitialize() {
  HeadlessScreen* screen = HeadlessScreen::Create(options()->window_size);
  display::Screen::SetScreenInstance(screen);
}

void HeadlessBrowserImpl::PlatformCreateWindow() {
  DCHECK(aura::Env::GetInstance());
  ui::DeviceDataManager::CreateInstance();

  window_tree_host_.reset(
      new HeadlessWindowTreeHost(gfx::Rect(options()->window_size)));
  window_tree_host_->InitHost();
  window_tree_host_->window()->Show();
  window_tree_host_->SetParentWindow(window_tree_host_->window());

  focus_client_.reset(new HeadlessFocusClient());
  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());
}

void HeadlessBrowserImpl::PlatformInitializeWebContents(
    const gfx::Size& initial_size,
    content::WebContents* web_contents) {
  gfx::NativeView contents = web_contents->GetNativeView();
  gfx::NativeWindow parent_window = window_tree_host_->window();
  DCHECK(!parent_window->Contains(contents));
  parent_window->AddChild(contents);
  contents->Show();
  contents->SetBounds(gfx::Rect(initial_size));

  content::RenderWidgetHostView* host_view =
      web_contents->GetRenderWidgetHostView();
  if (host_view)
    host_view->SetSize(initial_size);
}

}  // namespace headless
