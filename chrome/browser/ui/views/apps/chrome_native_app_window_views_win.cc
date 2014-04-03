// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/apps/per_app_settings_service.h"
#include "chrome/browser/apps/per_app_settings_service_factory.h"
#include "chrome/browser/jumplist_updater_win.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_win.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/win/shell.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/win/hwnd_util.h"

namespace {

void CreateIconAndSetRelaunchDetails(
    const base::FilePath& web_app_path,
    const base::FilePath& icon_file,
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const HWND hwnd) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  // Set the relaunch data so "Pin this program to taskbar" has the app's
  // information.
  CommandLine command_line = ShellIntegration::CommandLineArgsForLauncher(
      shortcut_info.url,
      shortcut_info.extension_id,
      shortcut_info.profile_path);

  base::FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return;
  }
  command_line.SetProgram(chrome_exe);
  ui::win::SetRelaunchDetailsForWindow(command_line.GetCommandLineString(),
      shortcut_info.title, hwnd);

  if (!base::PathExists(web_app_path) &&
      !base::CreateDirectory(web_app_path)) {
    return;
  }

  ui::win::SetAppIconForWindow(icon_file.value(), hwnd);
  web_app::internals::CheckAndSaveIcon(icon_file, shortcut_info.favicon);
}

}  // namespace

ChromeNativeAppWindowViewsWin::ChromeNativeAppWindowViewsWin()
    : weak_ptr_factory_(this) {}

void ChromeNativeAppWindowViewsWin::ActivateParentDesktopIfNecessary() {
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

void ChromeNativeAppWindowViewsWin::OnShortcutInfoLoaded(
    const ShellIntegration::ShortcutInfo& shortcut_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HWND hwnd = GetNativeAppWindowHWND();

  // Set window's icon to the one we're about to create/update in the web app
  // path. The icon cache will refresh on icon creation.
  base::FilePath web_app_path = web_app::GetWebAppDataDirectory(
      shortcut_info.profile_path, shortcut_info.extension_id,
      shortcut_info.url);
  base::FilePath icon_file = web_app_path
      .Append(web_app::internals::GetSanitizedFileName(shortcut_info.title))
      .ReplaceExtension(FILE_PATH_LITERAL(".ico"));

  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&CreateIconAndSetRelaunchDetails,
                 web_app_path, icon_file, shortcut_info, hwnd));
}

HWND ChromeNativeAppWindowViewsWin::GetNativeAppWindowHWND() const {
  return views::HWNDForWidget(widget()->GetTopLevelWidget());
}

void ChromeNativeAppWindowViewsWin::OnBeforeWidgetInit(
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  content::BrowserContext* browser_context = app_window()->browser_context();
  const extensions::Extension* extension = app_window()->extension();
  // If an app has any existing windows, ensure new ones are created on the
  // same desktop.
  apps::AppWindow* any_existing_window =
      apps::AppWindowRegistry::Get(browser_context)
          ->GetCurrentAppWindowForApp(extension->id());
  chrome::HostDesktopType desktop_type;
  if (any_existing_window) {
    desktop_type = chrome::GetHostDesktopTypeForNativeWindow(
        any_existing_window->GetNativeWindow());
  } else {
    PerAppSettingsService* settings =
        PerAppSettingsServiceFactory::GetForBrowserContext(browser_context);
    if (settings->HasDesktopLastLaunchedFrom(extension->id())) {
      desktop_type = settings->GetDesktopLastLaunchedFrom(extension->id());
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

void ChromeNativeAppWindowViewsWin::InitializeDefaultWindow(
    const apps::AppWindow::CreateParams& create_params) {
  ChromeNativeAppWindowViews::InitializeDefaultWindow(create_params);

  const extensions::Extension* extension = app_window()->extension();
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

  web_app::UpdateShortcutInfoAndIconForApp(
      extension,
      profile,
      base::Bind(&ChromeNativeAppWindowViewsWin::OnShortcutInfoLoaded,
                 weak_ptr_factory_.GetWeakPtr()));

  UpdateShelfMenu();
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
  if (!JumpListUpdater::IsEnabled())
    return;

  // Currently the only option is related to ephemeral apps, so avoid updating
  // the app's jump list when the feature is not enabled.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableEphemeralApps)) {
    return;
  }

  // For the icon resources.
  base::FilePath chrome_path;
  if (!PathService::Get(base::FILE_EXE, &chrome_path))
    return;

  JumpListUpdater jumplist_updater(app_model_id_);
  if (!jumplist_updater.BeginUpdate())
    return;

  // Add item to install ephemeral apps.
  const extensions::Extension* extension = app_window()->extension();
  DCHECK(extension);
  if (extension->is_ephemeral()) {
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

  jumplist_updater.CommitUpdate();
}
