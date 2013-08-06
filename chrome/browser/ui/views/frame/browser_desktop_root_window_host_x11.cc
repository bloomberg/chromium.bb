// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_root_window_host_x11.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostX11, public:

BrowserDesktopRootWindowHostX11::BrowserDesktopRootWindowHostX11(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    const gfx::Rect& initial_bounds,
    BrowserView* browser_view)
    : DesktopRootWindowHostX11(native_widget_delegate,
                               desktop_native_widget_aura,
                               initial_bounds),
      browser_view_(browser_view) {
}

BrowserDesktopRootWindowHostX11::~BrowserDesktopRootWindowHostX11() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostX11,
//     BrowserDesktopRootWindowHost implementation:

views::DesktopRootWindowHost*
    BrowserDesktopRootWindowHostX11::AsDesktopRootWindowHost() {
  return this;
}

int BrowserDesktopRootWindowHostX11::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopRootWindowHostX11::UsesNativeSystemMenu() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostX11,
//     views::DesktopRootWindowHostX11 implementation:

aura::RootWindow* BrowserDesktopRootWindowHostX11::Init(
    aura::Window* content_window,
    const views::Widget::InitParams& params) {
  aura::RootWindow* root_window = views::DesktopRootWindowHostX11::Init(
      content_window, params);

  // We have now created our backing X11 window. We now need to (possibly)
  // alert Unity that there's a menu bar attached to it.
  global_menu_bar_x11_.reset(new GlobalMenuBarX11(browser_view_, this));

  return root_window;
}

void BrowserDesktopRootWindowHostX11::CloseNow() {
  global_menu_bar_x11_.reset();
  DesktopRootWindowHostX11::CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHost, public:

// static
BrowserDesktopRootWindowHost*
    BrowserDesktopRootWindowHost::CreateBrowserDesktopRootWindowHost(
        views::internal::NativeWidgetDelegate* native_widget_delegate,
        views::DesktopNativeWidgetAura* desktop_native_widget_aura,
        const gfx::Rect& initial_bounds,
        BrowserView* browser_view,
        BrowserFrame* browser_frame) {
  return new BrowserDesktopRootWindowHostX11(native_widget_delegate,
                                             desktop_native_widget_aura,
                                             initial_bounds,
                                             browser_view);
}
