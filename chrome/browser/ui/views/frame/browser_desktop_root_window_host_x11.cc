// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_root_window_host_x11.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostX11, public:

BrowserDesktopRootWindowHostX11::BrowserDesktopRootWindowHostX11(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    const gfx::Rect& initial_bounds)
    : DesktopRootWindowHostX11(native_widget_delegate,
                                 desktop_native_widget_aura,
                                 initial_bounds) {
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
                                               initial_bounds);
}
