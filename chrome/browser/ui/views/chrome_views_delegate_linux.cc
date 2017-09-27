// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/linux_ui/linux_ui.h"

namespace {

bool IsDesktopEnvironmentUnity() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop_env =
      base::nix::GetDesktopEnvironment(env.get());
  return desktop_env == base::nix::DESKTOP_ENVIRONMENT_UNITY;
}

}  // namespace

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  NativeWidgetType native_widget_type =
      (params->parent && params->type != views::Widget::InitParams::TYPE_MENU &&
       params->type != views::Widget::InitParams::TYPE_TOOLTIP)
          ? NativeWidgetType::NATIVE_WIDGET_AURA
          : NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA;
  return ::CreateNativeWidget(native_widget_type, params, delegate);
}

gfx::ImageSkia* ChromeViewsDelegate::GetDefaultWindowIcon() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_64);
}

bool ChromeViewsDelegate::WindowManagerProvidesTitleBar(bool maximized) {
  // On Ubuntu Unity, the system always provides a title bar for
  // maximized windows.
  //
  // TODO(thomasanderson): Consider using the _UNITY_SHELL wm hint
  // when support for Ubuntu Trusty is dropped.
  if (!maximized)
    return false;
  static bool is_desktop_environment_unity = IsDesktopEnvironmentUnity();
  return is_desktop_environment_unity;
}
