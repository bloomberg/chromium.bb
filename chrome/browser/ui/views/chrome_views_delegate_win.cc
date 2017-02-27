// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "chrome/browser/ui/views/native_widget_factory.h"
#include "ui/base/win/shell.h"

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // Check the force_software_compositing flag only on Windows. If this flag is
  // on, it means that the widget being created wants to use the software
  // compositor which requires a top level window. We cannot have a mixture of
  // compositors active in one view hierarchy.
  NativeWidgetType native_widget_type =
      (params->parent && params->child && !params->force_software_compositing &&
       params->type != views::Widget::InitParams::TYPE_TOOLTIP)
          ? NativeWidgetType::NATIVE_WIDGET_AURA
          : NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA;

  if (!ui::win::IsAeroGlassEnabled()) {
    // If we don't have composition (either because Glass is not enabled or
    // because it was disabled at the command line), anything that requires
    // transparency will be broken with a toplevel window, so force the use of
    // a non toplevel window.
    if (params->opacity == views::Widget::InitParams::TRANSLUCENT_WINDOW &&
        !params->force_software_compositing)
      native_widget_type = NativeWidgetType::NATIVE_WIDGET_AURA;
  } else {
    // If we're on Vista+ with composition enabled, then we can use toplevel
    // windows for most things (they get blended via WS_EX_COMPOSITED, which
    // allows for animation effects, but also exceeding the bounds of the parent
    // window).
    if (params->parent &&
        params->type != views::Widget::InitParams::TYPE_CONTROL &&
        params->type != views::Widget::InitParams::TYPE_WINDOW) {
      native_widget_type = NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA;
    }
  }
  return ::CreateNativeWidget(native_widget_type, params, delegate);
}
