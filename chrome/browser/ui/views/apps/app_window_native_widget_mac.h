// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_NATIVE_WIDGET_MAC_H
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_NATIVE_WIDGET_MAC_H

#include "ui/views/widget/native_widget_mac.h"

// This implements features specific to app windows, e.g. frameless windows that
// behave like normal windows.
class AppWindowNativeWidgetMac : public views::NativeWidgetMac {
 public:
  explicit AppWindowNativeWidgetMac(views::Widget* widget);
  ~AppWindowNativeWidgetMac() override;

 protected:
  // NativeWidgetMac:
  gfx::NativeWindow CreateNSWindow(
      const views::Widget::InitParams& params) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppWindowNativeWidgetMac);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_NATIVE_WIDGET_MAC_H
