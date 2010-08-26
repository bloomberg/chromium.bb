// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

#if defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/gtk_util.h"
#endif

BackgroundModeManager::BackgroundModeManager(Profile* profile)
    : profile_(profile),
      background_app_count_(0),
      status_tray_(NULL),
      status_icon_(NULL) {
  // If background mode is disabled for unittests, just exit - don't listen for
  // any notifications.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBackgroundMode))
    return;

  // If the -keep-alive-for-test flag is passed, then always keep chrome running
  // in the background until the user explicitly terminates it, by acting as if
  // we loaded a background app.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKeepAliveForTest))
    OnBackgroundAppLoaded();

  // When an extension is installed, make sure launch on startup is properly
  // set if appropriate. Likewise, turn off launch on startup when the last
  // background app is uninstalled.
  registrar_.Add(this, NotificationType::EXTENSION_INSTALLED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::EXTENSION_UNINSTALLED,
                 Source<Profile>(profile));
  // Listen for when extensions are loaded/unloaded so we can track the
  // number of background apps.
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile));

  // Check for the presence of background apps after all extensions have been
  // loaded, to handle the case where an extension has been manually removed
  // while Chrome was not running.
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 Source<Profile>(profile));

  // Listen for the application shutting down so we can decrement our KeepAlive
  // count.
  registrar_.Add(this, NotificationType::APP_TERMINATING,
                 NotificationService::AllSources());


}

BackgroundModeManager::~BackgroundModeManager() {
  // If we're going away, remove our status tray icon so we don't get any events
  // from it.
  RemoveStatusTrayIcon();
}

bool BackgroundModeManager::IsBackgroundModeEnabled() {
  return profile_->GetPrefs()->GetBoolean(prefs::kBackgroundModeEnabled);
}

void BackgroundModeManager::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
      EnableLaunchOnStartup(IsBackgroundModeEnabled() &&
                            background_app_count_ > 0);
      break;
    case NotificationType::EXTENSION_LOADED:
      if (IsBackgroundApp(Details<Extension>(details).ptr()))
        OnBackgroundAppLoaded();
      break;
    case NotificationType::EXTENSION_UNLOADED:
      if (IsBackgroundApp(Details<Extension>(details).ptr()))
        OnBackgroundAppUnloaded();
      break;
    case NotificationType::EXTENSION_INSTALLED:
      if (IsBackgroundApp(Details<Extension>(details).ptr()))
        OnBackgroundAppInstalled();
      break;
    case NotificationType::EXTENSION_UNINSTALLED:
      if (IsBackgroundApp(Details<Extension>(details).ptr()))
        OnBackgroundAppUninstalled();
      break;
    case NotificationType::APP_TERMINATING:
      // Performing an explicit shutdown, so exit background mode if we were in
      // background mode.
      if (background_app_count_ > 0 && IsBackgroundModeEnabled())
        EndBackgroundMode();
      // Shutting down, so don't listen for any more notifications so we don't
      // try to re-enter/exit background mode again.
      registrar_.RemoveAll();
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool BackgroundModeManager::IsBackgroundApp(Extension* extension) {
  return extension->HasApiPermission(Extension::kBackgroundPermission);
}

void BackgroundModeManager::OnBackgroundAppLoaded() {
  // When a background app loads, increment our count and also enable
  // KeepAlive mode if the preference is set.
  background_app_count_++;
  if (background_app_count_ == 1 && IsBackgroundModeEnabled())
    StartBackgroundMode();
}

void BackgroundModeManager::StartBackgroundMode() {
  // Put ourselves in KeepAlive mode and create a status tray icon.
  BrowserList::StartKeepAlive();

  // Display a status icon to exit Chrome.
  CreateStatusTrayIcon();
}

void BackgroundModeManager::OnBackgroundAppUnloaded() {
  // When a background app unloads, decrement our count and also end
  // KeepAlive mode if appropriate.
  background_app_count_--;
  DCHECK(background_app_count_ == 0);
  if (background_app_count_ == 0 && IsBackgroundModeEnabled())
    EndBackgroundMode();
}

void BackgroundModeManager::EndBackgroundMode() {
  // End KeepAlive mode and blow away our status tray icon.
  BrowserList::EndKeepAlive();
  RemoveStatusTrayIcon();
}

void BackgroundModeManager::OnBackgroundAppInstalled() {
  // We're installing a background app. If this is the first background app
  // being installed, make sure we are set to launch on startup.
  if (IsBackgroundModeEnabled() && background_app_count_ == 0)
    EnableLaunchOnStartup(true);
}

void BackgroundModeManager::OnBackgroundAppUninstalled() {
  // When uninstalling a background app, disable launch on startup if it's the
  // last one.
  if (IsBackgroundModeEnabled() && background_app_count_ == 1)
    EnableLaunchOnStartup(false);
}

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // TODO(atwilson): Add platform-specific code to enable/disable launch on
  // startup.
}

void BackgroundModeManager::CreateStatusTrayIcon() {
  // Only need status icons on windows/linux. ChromeOS doesn't allow exiting
  // Chrome and Mac can use the dock icon instead.
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  if (!status_tray_)
    status_tray_ = profile_->GetStatusTray();
#endif

  // If the platform doesn't support status icons, or we've already created
  // our status icon, just return.
  if (!status_tray_ || status_icon_)
    return;
  status_icon_ = status_tray_->CreateStatusIcon();
  if (!status_icon_)
    return;

  // Set the image and add ourselves as a click observer on it
  SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON);
  status_icon_->SetImage(*bitmap);
  status_icon_->SetToolTip(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  // Create a context menu item for Chrome.
  menus::SimpleMenuModel* menu = new menus::SimpleMenuModel(this);
  menu->AddItem(IDC_ABOUT, l10n_util::GetStringFUTF16(IDS_ABOUT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));

#if defined(TOOLKIT_GTK)
  string16 preferences = gtk_util::GetStockPreferencesMenuLabel();
  if (preferences.empty())
    menu->AddItemWithStringId(IDC_OPTIONS, IDS_OPTIONS);
  else
    menu->AddItem(IDC_OPTIONS, preferences);
#else
  menu->AddItemWithStringId(IDC_OPTIONS, IDS_OPTIONS);
#endif
  menu->AddSeparator();
  menu->AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  status_icon_->SetContextMenu(menu);
}

bool BackgroundModeManager::IsCommandIdChecked(int command_id) const {
  return false;
}

bool BackgroundModeManager::IsCommandIdEnabled(int command_id) const {
  // For now, we do not support disabled items.
  return true;
}

bool BackgroundModeManager::GetAcceleratorForCommandId(
    int command_id,
    menus::Accelerator* accelerator) {
  // No accelerators for status icon context menus.
  return false;
}

void BackgroundModeManager::RemoveStatusTrayIcon() {
  if (status_icon_)
    status_tray_->RemoveStatusIcon(status_icon_);
  status_icon_ = NULL;
}


void BackgroundModeManager::ExecuteCommand(int item) {
  switch (item) {
    case IDC_EXIT:
      UserMetrics::RecordAction(UserMetricsAction("Exit"), profile_);
      BrowserList::CloseAllBrowsersAndExit();
      break;
    case IDC_ABOUT:
      GetBrowserWindow()->OpenAboutChromeDialog();
      break;
    case IDC_OPTIONS:
      GetBrowserWindow()->OpenOptionsDialog();
      break;
    default:
      NOTREACHED();
      break;
  }
}

Browser* BackgroundModeManager::GetBrowserWindow() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser) {
    Browser::OpenEmptyWindow(profile_);
    browser = BrowserList::GetLastActive();
  }
  return browser;
}

// static
void BackgroundModeManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kBackgroundModeEnabled, true);
}
