// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_root_window_host_linux.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostLinux, public:

BrowserDesktopRootWindowHostLinux::BrowserDesktopRootWindowHostLinux(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    const gfx::Rect& initial_bounds)
    : DesktopRootWindowHostLinux(native_widget_delegate,
                                 desktop_native_widget_aura,
                                 initial_bounds) {
}

BrowserDesktopRootWindowHostLinux::~BrowserDesktopRootWindowHostLinux() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopRootWindowHostLinux,
//     BrowserDesktopRootWindowHost implementation:

views::DesktopRootWindowHost*
    BrowserDesktopRootWindowHostLinux::AsDesktopRootWindowHost() {
  return this;
}

int BrowserDesktopRootWindowHostLinux::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopRootWindowHostLinux::UsesNativeSystemMenu() const {
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
  return new BrowserDesktopRootWindowHostLinux(native_widget_delegate,
                                               desktop_native_widget_aura,
                                               initial_bounds);
}
