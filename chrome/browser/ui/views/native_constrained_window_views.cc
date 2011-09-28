// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_views.h"

#include "views/widget/native_widget_views.h"

class NativeConstrainedWindowViews : public NativeConstrainedWindow,
                                     public views::NativeWidgetViews {
 public:
  explicit NativeConstrainedWindowViews(
      NativeConstrainedWindowDelegate* delegate)
      : views::NativeWidgetViews(delegate->AsNativeWidgetDelegate()),
        delegate_(delegate) {
  }

  virtual ~NativeConstrainedWindowViews() {
  }

 private:
  // Overridden from NativeConstrainedWindow:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE {
    return this;
  }

  NativeConstrainedWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeConstrainedWindowViews);
};

////////////////////////////////////////////////////////////////////////////////
// NativeConstrainedWindow, public:

// static
NativeConstrainedWindow* NativeConstrainedWindow::CreateNativeConstrainedWindow(
    NativeConstrainedWindowDelegate* delegate) {
  return new NativeConstrainedWindowViews(delegate);
}
