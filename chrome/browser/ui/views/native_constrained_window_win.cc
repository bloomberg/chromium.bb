// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_views.h"

#include "views/widget/native_widget_win.h"

namespace {
bool IsNonClientHitTestCode(UINT hittest) {
  return hittest != HTCLIENT && hittest != HTNOWHERE && hittest != HTCLOSE;
}
}

class NativeConstrainedWindowWin : public NativeConstrainedWindow,
                                   public views::NativeWidgetWin {
 public:
  explicit NativeConstrainedWindowWin(NativeConstrainedWindowDelegate* delegate)
      : views::NativeWidgetWin(delegate->AsNativeWidgetDelegate()),
        delegate_(delegate) {
  }

  virtual ~NativeConstrainedWindowWin() {
  }

 private:
  // Overridden from NativeConstrainedWindow:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE {
    return this;
  }

  // Overridden from views::NativeWidgetWin:
  virtual void OnFinalMessage(HWND window) OVERRIDE {
    delegate_->OnNativeConstrainedWindowDestroyed();
    NativeWidgetWin::OnFinalMessage(window);
  }
  virtual LRESULT OnMouseActivate(UINT message,
                                  WPARAM w_param,
                                  LPARAM l_param) OVERRIDE {
    if (IsNonClientHitTestCode(static_cast<UINT>(LOWORD(l_param))))
      delegate_->OnNativeConstrainedWindowMouseActivate();
    return NativeWidgetWin::OnMouseActivate(message, w_param, l_param);
  }

  NativeConstrainedWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeConstrainedWindowWin);
};

////////////////////////////////////////////////////////////////////////////////
// NativeConstrainedWindow, public:

// static
NativeConstrainedWindow* NativeConstrainedWindow::CreateNativeConstrainedWindow(
    NativeConstrainedWindowDelegate* delegate) {
  return new NativeConstrainedWindowWin(delegate);
}
