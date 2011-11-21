// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_views.h"

#include "ui/views/widget/native_widget_aura.h"

class NativeConstrainedWindowAura : public NativeConstrainedWindow,
                                   public views::NativeWidgetAura {
 public:
  explicit NativeConstrainedWindowAura(
      NativeConstrainedWindowDelegate* delegate)
      : views::NativeWidgetAura(delegate->AsNativeWidgetDelegate()),
        delegate_(delegate) {
  }

  virtual ~NativeConstrainedWindowAura() {
  }

 private:
  // Overridden from NativeConstrainedWindow:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE {
    return this;
  }

  // Overridden from views::NativeWidgetAura:

  NativeConstrainedWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeConstrainedWindowAura);
};

////////////////////////////////////////////////////////////////////////////////
// NativeConstrainedWindow, public:

// static
NativeConstrainedWindow* NativeConstrainedWindow::CreateNativeConstrainedWindow(
    NativeConstrainedWindowDelegate* delegate) {
  return new NativeConstrainedWindowAura(delegate);
}
