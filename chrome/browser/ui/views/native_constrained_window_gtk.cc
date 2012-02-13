// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_views.h"

#include "ui/views/widget/native_widget_gtk.h"

class NativeConstrainedWindowGtk : public NativeConstrainedWindow,
                                   public views::NativeWidgetGtk {
 public:
  explicit NativeConstrainedWindowGtk(
      NativeConstrainedWindowDelegate* delegate)
      : views::NativeWidgetGtk(delegate->AsNativeWidgetDelegate()),
        delegate_(delegate) {
  }

  virtual ~NativeConstrainedWindowGtk() {
  }

 private:
  // Overridden from NativeConstrainedWindow:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE {
    return this;
  }

  NativeConstrainedWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeConstrainedWindowGtk);
};

////////////////////////////////////////////////////////////////////////////////
// NativeConstrainedWindow, public:

// static
NativeConstrainedWindow* NativeConstrainedWindow::CreateNativeConstrainedWindow(
    NativeConstrainedWindowDelegate* delegate) {
  return new NativeConstrainedWindowGtk(delegate);
}

