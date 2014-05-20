// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_desktop_window_tree_host_x11.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/theme_image_mapper.h"

namespace {

// DesktopThemeProvider maps resource ids using MapThemeImage(). This is
// necessary for BrowserDesktopWindowTreeHostWin so that it uses the windows
// theme images rather than the ash theme images.
//
// This differs from the version in browser_desktop_window_tree_host_win.cc
// because we need to also look up whether we're using the native theme.
class DesktopThemeProvider : public ui::ThemeProvider {
 public:
  explicit DesktopThemeProvider(ThemeService* delegate)
      : delegate_(delegate) {
  }

  virtual bool UsingSystemTheme() const OVERRIDE {
    return delegate_->UsingSystemTheme();
  }
  virtual gfx::ImageSkia* GetImageSkiaNamed(int id) const OVERRIDE {
    if (delegate_->UsingSystemTheme())
      return delegate_->GetImageSkiaNamed(id);

    return delegate_->GetImageSkiaNamed(
        chrome::MapThemeImage(chrome::HOST_DESKTOP_TYPE_NATIVE, id));
  }
  virtual SkColor GetColor(int id) const OVERRIDE {
    return delegate_->GetColor(id);
  }
  virtual int GetDisplayProperty(int id) const OVERRIDE {
    return delegate_->GetDisplayProperty(id);
  }
  virtual bool ShouldUseNativeFrame() const OVERRIDE {
    return delegate_->ShouldUseNativeFrame();
  }
  virtual bool HasCustomImage(int id) const OVERRIDE {
    return delegate_->HasCustomImage(
        chrome::MapThemeImage(chrome::HOST_DESKTOP_TYPE_NATIVE, id));
  }
  virtual base::RefCountedMemory* GetRawData(
      int id,
      ui::ScaleFactor scale_factor) const OVERRIDE {
    return delegate_->GetRawData(id, scale_factor);
  }

 private:
  ThemeService* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DesktopThemeProvider);
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostX11, public:

BrowserDesktopWindowTreeHostX11::BrowserDesktopWindowTreeHostX11(
    views::internal::NativeWidgetDelegate* native_widget_delegate,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura,
    BrowserView* browser_view,
    BrowserFrame* browser_frame)
    : DesktopWindowTreeHostX11(native_widget_delegate,
                               desktop_native_widget_aura),
      browser_view_(browser_view) {
  scoped_ptr<ui::ThemeProvider> theme_provider(
      new DesktopThemeProvider(ThemeServiceFactory::GetForProfile(
                                   browser_view->browser()->profile())));
  browser_frame->SetThemeProvider(theme_provider.Pass());
}

BrowserDesktopWindowTreeHostX11::~BrowserDesktopWindowTreeHostX11() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostX11,
//     BrowserDesktopWindowTreeHost implementation:

views::DesktopWindowTreeHost*
    BrowserDesktopWindowTreeHostX11::AsDesktopWindowTreeHost() {
  return this;
}

int BrowserDesktopWindowTreeHostX11::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserDesktopWindowTreeHostX11::UsesNativeSystemMenu() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHostX11,
//     views::DesktopWindowTreeHostX11 implementation:

void BrowserDesktopWindowTreeHostX11::Init(
    aura::Window* content_window,
    const views::Widget::InitParams& params) {
  views::DesktopWindowTreeHostX11::Init(content_window, params);

  // We have now created our backing X11 window. We now need to (possibly)
  // alert Unity that there's a menu bar attached to it.
  global_menu_bar_x11_.reset(new GlobalMenuBarX11(browser_view_, this));
}

void BrowserDesktopWindowTreeHostX11::CloseNow() {
  global_menu_bar_x11_.reset();
  DesktopWindowTreeHostX11::CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserDesktopWindowTreeHost, public:

// static
BrowserDesktopWindowTreeHost*
    BrowserDesktopWindowTreeHost::CreateBrowserDesktopWindowTreeHost(
        views::internal::NativeWidgetDelegate* native_widget_delegate,
        views::DesktopNativeWidgetAura* desktop_native_widget_aura,
        BrowserView* browser_view,
        BrowserFrame* browser_frame) {
  return new BrowserDesktopWindowTreeHostX11(native_widget_delegate,
                                             desktop_native_widget_aura,
                                             browser_view,
                                             browser_frame);
}
