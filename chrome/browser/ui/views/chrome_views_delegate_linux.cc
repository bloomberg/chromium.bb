// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/linux_ui/linux_ui.h"

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  NativeWidgetType native_widget_type =
      (params->parent && params->type != views::Widget::InitParams::TYPE_MENU &&
       params->type != views::Widget::InitParams::TYPE_MENU)
          ? NativeWidgetType::NATIVE_WIDGET_AURA
          : NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA;
  return ::CreateNativeWidget(native_widget_type, params, delegate);
}

gfx::ImageSkia* ChromeViewsDelegate::GetDefaultWindowIcon() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_64);
}

bool ChromeViewsDelegate::WindowManagerProvidesTitleBar(bool maximized) {
  // On Ubuntu Unity, the system always provides a title bar for maximized
  // windows.
  views::LinuxUI* ui = views::LinuxUI::instance();
  return maximized && ui && ui->UnityIsRunning();
}
