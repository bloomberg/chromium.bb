// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_X11_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_X11_H_

#include "ui/views/widget/desktop_aura/desktop_root_window_host_x11.h"
#include "chrome/browser/ui/views/frame/browser_desktop_root_window_host.h"

class BrowserFrame;
class BrowserView;

namespace views {
class DesktopNativeWidgetAura;
}

class BrowserDesktopRootWindowHostX11
    : public BrowserDesktopRootWindowHost,
      public views::DesktopRootWindowHostX11 {
 public:
  BrowserDesktopRootWindowHostX11(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      const gfx::Rect& initial_bounds);
  virtual ~BrowserDesktopRootWindowHostX11();

 private:
  // Overridden from BrowserDesktopRootWindowHost:
  virtual DesktopRootWindowHost* AsDesktopRootWindowHost() OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual bool UsesNativeSystemMenu() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopRootWindowHostX11);
};


#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_X11_H_
