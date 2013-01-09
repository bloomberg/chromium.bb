// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_LINUX_H_

#include "ui/views/widget/desktop_aura/desktop_root_window_host_linux.h"
#include "chrome/browser/ui/views/frame/browser_desktop_root_window_host.h"

class BrowserFrame;
class BrowserView;

namespace views {
class DesktopNativeWidgetAura;
}

class BrowserDesktopRootWindowHostLinux
    : public BrowserDesktopRootWindowHost,
      public views::DesktopRootWindowHostLinux {
 public:
  BrowserDesktopRootWindowHostLinux(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      const gfx::Rect& initial_bounds);
  virtual ~BrowserDesktopRootWindowHostLinux();

 private:
  // Overridden from BrowserDesktopRootWindowHost:
  virtual DesktopRootWindowHost* AsDesktopRootWindowHost() OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual bool UsesNativeSystemMenu() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopRootWindowHostLinux);
};


#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_LINUX_H_
