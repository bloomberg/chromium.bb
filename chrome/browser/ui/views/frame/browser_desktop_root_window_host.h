// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_H_

class BrowserFrame;
class BrowserView;
namespace gfx {
class Rect;
}
namespace views {
class DesktopNativeWidgetAura;
class DesktopRootWindowHost;
namespace internal {
class NativeWidgetDelegate;
}
}

// Interface to a platform specific browser frame implementation. The object
// implementing this interface will also implement views::DesktopRootWindowHost.
class BrowserDesktopRootWindowHost {
 public:
  // BDRWH is owned by the RootWindow.
  static BrowserDesktopRootWindowHost* CreateBrowserDesktopRootWindowHost(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura,
      const gfx::Rect& initial_bounds,
      BrowserView* browser_view,
      BrowserFrame* browser_frame);

  virtual views::DesktopRootWindowHost* AsDesktopRootWindowHost() = 0;

  virtual int GetMinimizeButtonOffset() const = 0;

  // Returns true if the OS takes care of showing the system menu. Returning
  // false means BrowserFrame handles showing the system menu.
  virtual bool UsesNativeSystemMenu() const = 0;
};


#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_H_
