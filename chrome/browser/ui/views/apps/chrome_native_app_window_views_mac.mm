// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/apps/chrome_native_app_window_views_mac.h"

#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/browser/ui/views/apps/app_window_native_widget_mac.h"
#include "chrome/browser/ui/views/apps/native_app_window_frame_view_mac.h"

ChromeNativeAppWindowViewsMac::ChromeNativeAppWindowViewsMac()
    : is_hidden_with_app_(false) {
}

ChromeNativeAppWindowViewsMac::~ChromeNativeAppWindowViewsMac() {
}

void ChromeNativeAppWindowViewsMac::OnBeforeWidgetInit(
    const extensions::AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  DCHECK(!init_params->native_widget);
  init_params->remove_standard_frame = IsFrameless();
  init_params->native_widget = new AppWindowNativeWidgetMac(widget, this);
  ChromeNativeAppWindowViews::OnBeforeWidgetInit(create_params, init_params,
                                                 widget);
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsMac::CreateStandardDesktopAppFrame() {
  return new NativeAppWindowFrameViewMac(widget());
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsMac::CreateNonStandardAppFrame() {
  return new NativeAppWindowFrameViewMac(widget());
}

void ChromeNativeAppWindowViewsMac::Show() {
  if (is_hidden_with_app_) {
    // If there is a shim to gently request attention, return here. Otherwise
    // show the window as usual.
    if (apps::ExtensionAppShimHandler::ActivateAndRequestUserAttentionForWindow(
            app_window())) {
      return;
    }
  }

  ChromeNativeAppWindowViews::Show();
}

void ChromeNativeAppWindowViewsMac::ShowInactive() {
  if (is_hidden_with_app_)
    return;

  ChromeNativeAppWindowViews::ShowInactive();
}

void ChromeNativeAppWindowViewsMac::FlashFrame(bool flash) {
  apps::ExtensionAppShimHandler::RequestUserAttentionForWindow(
      app_window(), flash ? apps::APP_SHIM_ATTENTION_CRITICAL
                          : apps::APP_SHIM_ATTENTION_CANCEL);
}

void ChromeNativeAppWindowViewsMac::ShowWithApp() {
  is_hidden_with_app_ = false;
  if (!app_window()->is_hidden())
    ShowInactive();
}

void ChromeNativeAppWindowViewsMac::HideWithApp() {
  is_hidden_with_app_ = true;
  ChromeNativeAppWindowViews::Hide();
}
