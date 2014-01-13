// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/native_app_window_views_win.h"

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "ash/shell.h"
#include "chrome/browser/apps/per_app_settings_service.h"
#include "chrome/browser/apps/per_app_settings_service_factory.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

NativeAppWindowViewsWin::NativeAppWindowViewsWin() {
}

void NativeAppWindowViewsWin::ActivateParentDesktopIfNecessary() {
  if (!ash::Shell::HasInstance())
    return;

  views::Widget* widget =
      implicit_cast<views::WidgetDelegate*>(this)->GetWidget();
  chrome::HostDesktopType host_desktop_type =
      chrome::GetHostDesktopTypeForNativeWindow(widget->GetNativeWindow());
  // Only switching into Ash from Native is supported. Tearing the user out of
  // Metro mode can only be done by launching a process from Metro mode itself.
  // This is done for launching apps, but not regular activations.
  if (host_desktop_type == chrome::HOST_DESKTOP_TYPE_ASH &&
      chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_NATIVE) {
    chrome::ActivateMetroChrome();
  }
}

void NativeAppWindowViewsWin::OnBeforeWidgetInit(
    views::Widget::InitParams* init_params, views::Widget* widget) {
  // If an app has any existing windows, ensure new ones are created on the
  // same desktop.
  apps::ShellWindow* any_existing_window =
      apps::ShellWindowRegistry::Get(profile())->
          GetCurrentShellWindowForApp(extension()->id());
  chrome::HostDesktopType desktop_type;
  if (any_existing_window) {
    desktop_type = chrome::GetHostDesktopTypeForNativeWindow(
        any_existing_window->GetNativeWindow());
  } else {
    PerAppSettingsService* settings =
        PerAppSettingsServiceFactory::GetForBrowserContext(profile());
    if (settings->HasDesktopLastLaunchedFrom(extension()->id())) {
      desktop_type = settings->GetDesktopLastLaunchedFrom(extension()->id());
    } else {
      // We don't know what desktop this app was last launched from, so take our
      // best guess as to what desktop the user is on.
      desktop_type = chrome::GetActiveDesktop();
    }
  }
  if (desktop_type == chrome::HOST_DESKTOP_TYPE_ASH)
    init_params->context = ash::Shell::GetPrimaryRootWindow();
  else
    init_params->native_widget = new views::DesktopNativeWidgetAura(widget);
}

void NativeAppWindowViewsWin::Show() {
  ActivateParentDesktopIfNecessary();
  NativeAppWindowViews::Show();
}

void NativeAppWindowViewsWin::Activate() {
  ActivateParentDesktopIfNecessary();
  NativeAppWindowViews::Activate();
}
