// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/apps/chrome_native_app_window_views_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#import "chrome/browser/ui/views/apps/app_window_native_widget_mac.h"
#import "chrome/browser/ui/views/apps/native_app_window_frame_view_mac.h"

@interface NSView (WebContentsView)
- (void)setMouseDownCanMoveWindow:(BOOL)can_move;
@end

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
  return new NativeAppWindowFrameViewMac(widget(), this);
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsMac::CreateNonStandardAppFrame() {
  return new NativeAppWindowFrameViewMac(widget(), this);
}

void ChromeNativeAppWindowViewsMac::Show() {
  UnhideWithoutActivation();
  ChromeNativeAppWindowViews::Show();
}

void ChromeNativeAppWindowViewsMac::ShowInactive() {
  if (is_hidden_with_app_)
    return;

  ChromeNativeAppWindowViews::ShowInactive();
}

void ChromeNativeAppWindowViewsMac::Activate() {
  UnhideWithoutActivation();
  ChromeNativeAppWindowViews::Activate();
}

void ChromeNativeAppWindowViewsMac::FlashFrame(bool flash) {
  apps::ExtensionAppShimHandler::RequestUserAttentionForWindow(
      app_window(), flash ? apps::APP_SHIM_ATTENTION_CRITICAL
                          : apps::APP_SHIM_ATTENTION_CANCEL);
}

void ChromeNativeAppWindowViewsMac::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  ChromeNativeAppWindowViews::UpdateDraggableRegions(regions);

  NSView* web_contents_view = app_window()->web_contents()->GetNativeView();
  [web_contents_view setMouseDownCanMoveWindow:YES];
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

void ChromeNativeAppWindowViewsMac::UnhideWithoutActivation() {
  if (is_hidden_with_app_) {
    apps::ExtensionAppShimHandler::UnhideWithoutActivationForWindow(
        app_window());
    is_hidden_with_app_ = false;
  }
}
