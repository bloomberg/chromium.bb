// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_views.h"

#include "views/window/window_win.h"

namespace {
bool IsNonClientHitTestCode(UINT hittest) {
  return hittest != HTCLIENT && hittest != HTNOWHERE && hittest != HTCLOSE;
}
}

class NativeConstrainedWindowWin : public NativeConstrainedWindow,
                                   public views::WindowWin {
 public:
  NativeConstrainedWindowWin(NativeConstrainedWindowDelegate* delegate,
                             views::WindowDelegate* window_delegate)
      : WindowWin(window_delegate),
        delegate_(delegate) {
    views::Widget::CreateParams params(
        views::Widget::CreateParams::TYPE_WINDOW);
    params.child = true;
    SetCreateParams(params);
  }

  virtual ~NativeConstrainedWindowWin() {
  }

 private:
  // Overridden from NativeConstrainedWindow:
  virtual void InitNativeConstrainedWindow(gfx::NativeView parent) OVERRIDE {
    WindowWin::Init(parent, gfx::Rect());
  }
  virtual views::NativeWindow* AsNativeWindow() OVERRIDE {
    return this;
  }

  // Overridden from views::WindowWin:
  virtual void OnFinalMessage(HWND window) OVERRIDE {
    delegate_->OnNativeConstrainedWindowDestroyed();
    WindowWin::OnFinalMessage(window);
  }
  virtual LRESULT OnMouseActivate(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param) OVERRIDE {
    if (IsNonClientHitTestCode(static_cast<UINT>(LOWORD(l_param))))
      delegate_->OnNativeConstrainedWindowMouseActivate();
    return WindowWin::OnMouseActivate(message, w_param, l_param);
  }

  // Overridden from views::Window:
  virtual views::NonClientFrameView* CreateFrameViewForWindow() OVERRIDE {
    return delegate_->CreateFrameViewForWindow();
  }

  NativeConstrainedWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeConstrainedWindowWin);
};

////////////////////////////////////////////////////////////////////////////////
// NativeConstrainedWindow, public:

// static
NativeConstrainedWindow* NativeConstrainedWindow::CreateNativeConstrainedWindow(
    NativeConstrainedWindowDelegate* delegate,
    views::WindowDelegate* window_delegate) {
  return new NativeConstrainedWindowWin(delegate, window_delegate);
}
