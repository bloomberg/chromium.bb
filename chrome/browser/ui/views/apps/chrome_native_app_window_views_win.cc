// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"

#include "apps/ui/views/app_window_frame_view.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/apps/per_app_settings_service.h"
#include "chrome/browser/apps/per_app_settings_service_factory.h"
#include "chrome/browser/jumplist_updater_win.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/views/apps/app_window_desktop_native_widget_aura_win.h"
#include "chrome/browser/ui/views/apps/glass_app_window_frame_view_win.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_win.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/shell.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/win/hwnd_util.h"

ChromeNativeAppWindowViewsWin::ChromeNativeAppWindowViewsWin()
    : glass_frame_view_(NULL), weak_ptr_factory_(this) {
}

void ChromeNativeAppWindowViewsWin::ActivateParentDesktopIfNecessary() {
  // Only switching into Ash from Native is supported. Tearing the user out of
  // Metro mode can only be done by launching a process from Metro mode itself.
  // This is done for launching apps, but not regular activations.
  if (IsRunningInAsh() &&
      chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_NATIVE) {
    chrome::ActivateMetroChrome();
  }
}

HWND ChromeNativeAppWindowViewsWin::GetNativeAppWindowHWND() const {
  return views::HWNDForWidget(widget()->GetTopLevelWidget());
}

bool ChromeNativeAppWindowViewsWin::IsRunningInAsh() {
  if (!ash::Shell::HasInstance())
    return false;

  views::Widget* widget =
      implicit_cast<views::WidgetDelegate*>(this)->GetWidget();
  chrome::HostDesktopType host_desktop_type =
      chrome::GetHostDesktopTypeForNativeWindow(widget->GetNativeWindow());
  return host_desktop_type == chrome::HOST_DESKTOP_TYPE_ASH;
}

void ChromeNativeAppWindowViewsWin::EnsureCaptionStyleSet() {
  // Windows seems to have issues maximizing windows without WS_CAPTION.
  // The default views / Aura implementation will remove this if we are using
  // frameless or colored windows, so we put it back here.
  HWND hwnd = GetNativeAppWindowHWND();
  int current_style = ::GetWindowLong(hwnd, GWL_STYLE);
  ::SetWindowLong(hwnd, GWL_STYLE, current_style | WS_CAPTION);
}

void ChromeNativeAppWindowViewsWin::OnBeforeWidgetInit(
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  content::BrowserContext* browser_context = app_window()->browser_context();
  std::string extension_id = app_window()->extension_id();
  // If an app has any existing windows, ensure new ones are created on the
  // same desktop.
  extensions::AppWindow* any_existing_window =
      extensions::AppWindowRegistry::Get(browser_context)
          ->GetCurrentAppWindowForApp(extension_id);
  chrome::HostDesktopType desktop_type;
  if (any_existing_window) {
    desktop_type = chrome::GetHostDesktopTypeForNativeWindow(
        any_existing_window->GetNativeWindow());
  } else {
    PerAppSettingsService* settings =
        PerAppSettingsServiceFactory::GetForBrowserContext(browser_context);
    if (settings->HasDesktopLastLaunchedFrom(extension_id)) {
      desktop_type = settings->GetDesktopLastLaunchedFrom(extension_id);
    } else {
      // We don't know what desktop this app was last launched from, so take our
      // best guess as to what desktop the user is on.
      desktop_type = chrome::GetActiveDesktop();
    }
  }
  if (desktop_type == chrome::HOST_DESKTOP_TYPE_ASH)
    init_params->context = ash::Shell::GetPrimaryRootWindow();
  else
    init_params->native_widget = new AppWindowDesktopNativeWidgetAuraWin(this);
}

void ChromeNativeAppWindowViewsWin::InitializeDefaultWindow(
    const extensions::AppWindow::CreateParams& create_params) {
  ChromeNativeAppWindowViews::InitializeDefaultWindow(create_params);

  // Remaining initialization is for Windows shell integration, which doesn't
  // apply to app windows in Ash.
  if (IsRunningInAsh())
    return;

  const extensions::Extension* extension = app_window()->GetExtension();
  if (!extension)
    return;

  std::string app_name =
      web_app::GenerateApplicationNameFromExtensionId(extension->id());
  base::string16 app_name_wide = base::UTF8ToWide(app_name);
  HWND hwnd = GetNativeAppWindowHWND();
  Profile* profile =
      Profile::FromBrowserContext(app_window()->browser_context());
  app_model_id_ =
      ShellIntegration::GetAppModelIdForProfile(app_name_wide,
                                                profile->GetPath());
  ui::win::SetAppIdForWindow(app_model_id_, hwnd);
  web_app::UpdateRelaunchDetailsForApp(profile, extension, hwnd);

  if (!create_params.alpha_enabled)
    EnsureCaptionStyleSet();
  UpdateShelfMenu();
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsWin::CreateStandardDesktopAppFrame() {
  glass_frame_view_ = NULL;
  if (ui::win::IsAeroGlassEnabled()) {
    glass_frame_view_ = new GlassAppWindowFrameViewWin(this, widget());
    return glass_frame_view_;
  }
  return ChromeNativeAppWindowViews::CreateStandardDesktopAppFrame();
}

void ChromeNativeAppWindowViewsWin::Show() {
  ActivateParentDesktopIfNecessary();
  ChromeNativeAppWindowViews::Show();
}

void ChromeNativeAppWindowViewsWin::Activate() {
  ActivateParentDesktopIfNecessary();
  ChromeNativeAppWindowViews::Activate();
}

void ChromeNativeAppWindowViewsWin::UpdateShelfMenu() {
  if (!JumpListUpdater::IsEnabled() || IsRunningInAsh())
    return;

  // Currently the only option is related to ephemeral apps, so avoid updating
  // the app's jump list when the feature is not enabled.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableEphemeralApps)) {
    return;
  }

  const extensions::Extension* extension = app_window()->GetExtension();
  if (!extension)
    return;

  // For the icon resources.
  base::FilePath chrome_path;
  if (!PathService::Get(base::FILE_EXE, &chrome_path))
    return;

  DCHECK(!app_model_id_.empty());

  JumpListUpdater jumplist_updater(app_model_id_);
  if (!jumplist_updater.BeginUpdate())
    return;

  // Add item to install ephemeral apps.
  if (extensions::util::IsEphemeralApp(extension->id(),
                                       app_window()->browser_context())) {
    scoped_refptr<ShellLinkItem> link(new ShellLinkItem());
    link->set_title(l10n_util::GetStringUTF16(IDS_APP_INSTALL_TITLE));
    link->set_icon(chrome_path.value(),
                   icon_resources::kInstallPackagedAppIndex);
    ShellIntegration::AppendProfileArgs(
        app_window()->browser_context()->GetPath(), link->GetCommandLine());
    link->GetCommandLine()->AppendSwitchASCII(switches::kInstallFromWebstore,
                                              extension->id());

    ShellLinkItemList items;
    items.push_back(link);
    jumplist_updater.AddTasks(items);
  }

  // Note that an empty jumplist must still be committed to clear all items.
  jumplist_updater.CommitUpdate();
}
