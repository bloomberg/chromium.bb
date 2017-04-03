// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_clipboard.h"
#include "headless/lib/browser/headless_screen.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

void HeadlessBrowserImpl::PlatformInitialize() {
  HeadlessScreen* screen = HeadlessScreen::Create(options()->window_size);
  display::Screen::SetScreenInstance(screen);

  // TODO(eseckler): We shouldn't share clipboard contents across WebContents
  // (or at least BrowserContexts).
  ui::Clipboard::SetClipboardForCurrentThread(
      base::MakeUnique<HeadlessClipboard>());
}

void HeadlessBrowserImpl::PlatformCreateWindow() {
  DCHECK(aura::Env::GetInstance());
  ui::DeviceDataManager::CreateInstance();
}

void HeadlessBrowserImpl::PlatformInitializeWebContents(
    const gfx::Size& initial_size,
    HeadlessWebContentsImpl* web_contents) {
  gfx::Rect initial_rect(initial_size);

  auto window_tree_host =
      base::MakeUnique<HeadlessWindowTreeHost>(initial_rect);
  window_tree_host->InitHost();
  gfx::NativeWindow parent_window = window_tree_host->window();
  parent_window->Show();
  window_tree_host->SetParentWindow(parent_window);
  web_contents->set_window_tree_host(std::move(window_tree_host));

  gfx::NativeView contents = web_contents->web_contents()->GetNativeView();
  DCHECK(!parent_window->Contains(contents));
  parent_window->AddChild(contents);
  contents->Show();
  contents->SetBounds(initial_rect);

  content::RenderWidgetHostView* host_view =
      web_contents->web_contents()->GetRenderWidgetHostView();
  if (host_view)
    host_view->SetSize(initial_size);
}

}  // namespace headless
