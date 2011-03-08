// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background_application_list_model.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

void BackgroundModeManager::OnApplicationDataChanged(
    const Extension* extension) {
  UpdateContextMenuEntryIcon(extension);
}

void BackgroundModeManager::OnApplicationListChanged() {
  UpdateStatusTrayIconContextMenu();
}

BackgroundModeManager::BackgroundModeManager(Profile* profile,
                                             CommandLine* command_line)
    : profile_(profile),
      applications_(profile),
      background_app_count_(0),
      context_menu_(NULL),
      context_menu_application_offset_(0),
      in_background_mode_(false),
      keep_alive_for_startup_(false),
      status_tray_(NULL),
      status_icon_(NULL) {
  // If background mode is disabled, just exit - don't listen for any
  // notifications.
  if (!IsBackgroundModeEnabled(command_line))
    return;

  // Keep the browser alive until extensions are done loading - this is needed
  // by the --no-startup-window flag. We want to stay alive until we load
  // extensions, at which point we should either run in background mode (if
  // there are background apps) or exit if there are none.
  if (command_line->HasSwitch(switches::kNoStartupWindow)) {
    keep_alive_for_startup_ = true;
    BrowserList::StartKeepAlive();
  }

  // If the -keep-alive-for-test flag is passed, then always keep chrome running
  // in the background until the user explicitly terminates it, by acting as if
  // we loaded a background app.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKeepAliveForTest))
    OnBackgroundAppLoaded();

  // Listen for when extensions are loaded/unloaded so we can track the
  // number of background apps and modify our keep-alive and launch-on-startup
  // state appropriately.
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

  applications_.AddObserver(this);
}

BackgroundModeManager::~BackgroundModeManager() {
  applications_.RemoveObserver(this);

  // We're going away, so exit background mode (does nothing if we aren't in
  // background mode currently). This is primarily needed for unit tests,
  // because in an actual running system we'd get an APP_TERMINATING
  // notification before being destroyed.
  EndBackgroundMode();
}

void BackgroundModeManager::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
      // Extensions are loaded, so we don't need to manually keep the browser
      // process alive any more when running in no-startup-window mode.
      EndKeepAliveForStartup();

      // On a Mac, we use 'login items' mechanism which has user-facing UI so we
      // don't want to stomp on user choice every time we start and load
      // registered extensions. This means that if a background app is removed
      // or added while Chrome is not running, we could leave Chrome in the
      // wrong state, but this is better than constantly forcing Chrome to
      // launch on startup even after the user removes the LoginItem manually.
#if !defined(OS_MACOSX)
      EnableLaunchOnStartup(background_app_count_ > 0);
#endif
      break;
    case NotificationType::EXTENSION_LOADED: {
        Extension* extension = Details<Extension>(details).ptr();
        if (BackgroundApplicationListModel::IsBackgroundApp(*extension)) {
          // Extensions loaded after the ExtensionsService is ready should be
          // treated as new installs.
          if (profile_->GetExtensionService()->is_ready())
            OnBackgroundAppInstalled(extension);
          OnBackgroundAppLoaded();
        }
      }
      break;
    case NotificationType::EXTENSION_UNLOADED:
      if (BackgroundApplicationListModel::IsBackgroundApp(
              *Details<UnloadedExtensionInfo>(details)->extension)) {
        Details<UnloadedExtensionInfo> info =
            Details<UnloadedExtensionInfo>(details);
        // If we already got an unload notification when it was disabled, ignore
        // this one.
        // TODO(atwilson): Change BackgroundModeManager to use
        // BackgroundApplicationListModel instead of tracking the count here.
        if (info->already_disabled)
          return;
        OnBackgroundAppUnloaded();
        OnBackgroundAppUninstalled();
      }
      break;
    case NotificationType::APP_TERMINATING:
      // Make sure we aren't still keeping the app alive (only happens if we
      // don't receive an EXTENSIONS_READY notification for some reason).
      EndKeepAliveForStartup();
      // Performing an explicit shutdown, so exit background mode (does nothing
      // if we aren't in background mode currently).
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

void BackgroundModeManager::EndKeepAliveForStartup() {
  if (keep_alive_for_startup_) {
    keep_alive_for_startup_ = false;
    // We call this via the message queue to make sure we don't try to end
    // keep-alive (which can shutdown Chrome) before the message loop has
    // started.
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableFunction(BrowserList::EndKeepAlive));
  }
}

void BackgroundModeManager::OnBackgroundAppLoaded() {
  // When a background app loads, increment our count and also enable
  // KeepAlive mode if the preference is set.
  background_app_count_++;
  if (background_app_count_ == 1)
    StartBackgroundMode();
}

void BackgroundModeManager::StartBackgroundMode() {
  // Don't bother putting ourselves in background mode if we're already there.
  if (in_background_mode_)
    return;

  // Mark ourselves as running in background mode.
  in_background_mode_ = true;

  // Put ourselves in KeepAlive mode and create a status tray icon.
  BrowserList::StartKeepAlive();

  // Display a status icon to exit Chrome.
  CreateStatusTrayIcon();
}

void BackgroundModeManager::OnBackgroundAppUnloaded() {
  // When a background app unloads, decrement our count and also end
  // KeepAlive mode if appropriate.
  background_app_count_--;
  DCHECK(background_app_count_ >= 0);
  if (background_app_count_ == 0)
    EndBackgroundMode();
}

void BackgroundModeManager::EndBackgroundMode() {
  if (!in_background_mode_)
    return;
  in_background_mode_ = false;

  // End KeepAlive mode and blow away our status tray icon.
  BrowserList::EndKeepAlive();
  RemoveStatusTrayIcon();
}

void BackgroundModeManager::OnBackgroundAppInstalled(
    const Extension* extension) {
  // We're installing a background app. If this is the first background app
  // being installed, make sure we are set to launch on startup.
  if (background_app_count_ == 0)
    EnableLaunchOnStartup(true);

  // Notify the user that a background app has been installed.
  if (extension)  // NULL when called by unit tests.
    DisplayAppInstalledNotification(extension);
}

void BackgroundModeManager::OnBackgroundAppUninstalled() {
  // When uninstalling a background app, disable launch on startup if
  // we have no more background apps.
  if (background_app_count_ == 0)
    EnableLaunchOnStartup(false);
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
  UpdateStatusTrayIconContextMenu();
}

void BackgroundModeManager::UpdateContextMenuEntryIcon(
    const Extension* extension) {
  if (!context_menu_)
    return;
  context_menu_->SetIcon(
      context_menu_application_offset_ + applications_.GetPosition(extension),
      *(applications_.GetIcon(extension)));
  status_icon_->SetContextMenu(context_menu_);  // for Update effect
}

void BackgroundModeManager::UpdateStatusTrayIconContextMenu() {
  if (!status_icon_)
    return;

  // Create a context menu item for Chrome.
  ui::SimpleMenuModel* menu = new ui::SimpleMenuModel(this);
  // Add About item
  menu->AddItem(IDC_ABOUT, l10n_util::GetStringFUTF16(IDS_ABOUT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  menu->AddItem(IDC_OPTIONS, GetPreferencesMenuLabel());
  menu->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  menu->AddSeparator();
  int position = 0;
  context_menu_application_offset_ = menu->GetItemCount();
  for (ExtensionList::const_iterator cursor = applications_.begin();
       cursor != applications_.end();
       ++cursor, ++position) {
    const SkBitmap* icon = applications_.GetIcon(*cursor);
    DCHECK(position == applications_.GetPosition(*cursor));
    const std::string& name = (*cursor)->name();
    menu->AddItem(position, UTF8ToUTF16(name));
    if (icon)
      menu->SetIcon(menu->GetItemCount() - 1, *icon);
  }
  if (applications_.size() > 0)
    menu->AddSeparator();
  menu->AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  context_menu_ = menu;
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
    ui::Accelerator* accelerator) {
  // No accelerators for status icon context menus.
  return false;
}

void BackgroundModeManager::RemoveStatusTrayIcon() {
  if (status_icon_)
    status_tray_->RemoveStatusIcon(status_icon_);
  status_icon_ = NULL;
  context_menu_ = NULL;  // Do not delete, points within status_icon_
}

void BackgroundModeManager::ExecuteApplication(int item) {
  DCHECK(item >= 0 && item < static_cast<int>(applications_.size()));
  Browser* browser = BrowserList::GetLastActive();
  if (!browser) {
    Browser::OpenEmptyWindow(profile_);
    browser = BrowserList::GetLastActive();
  }
  const Extension* extension = applications_.GetExtension(item);
  browser->OpenApplicationTab(profile_, extension, NULL);
}

void BackgroundModeManager::ExecuteCommand(int item) {
  switch (item) {
    case IDC_ABOUT:
      GetBrowserWindow()->OpenAboutChromeDialog();
      break;
    case IDC_EXIT:
      UserMetrics::RecordAction(UserMetricsAction("Exit"), profile_);
      BrowserList::CloseAllBrowsersAndExit();
      break;
    case IDC_OPTIONS:
      GetBrowserWindow()->OpenOptionsDialog();
      break;
    case IDC_TASK_MANAGER:
      GetBrowserWindow()->OpenTaskManager(true);
      break;
    default:
      ExecuteApplication(item);
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
bool BackgroundModeManager::IsBackgroundModeEnabled(
    const CommandLine* command_line) {

  // Background mode is disabled if the appropriate flag is passed, or if
  // extensions are disabled. It's always disabled on chromeos since chrome
  // is always running on that platform, making it superfluous.
#if defined(OS_CHROMEOS)
  return false;
#else
  bool background_mode_enabled =
      !command_line->HasSwitch(switches::kDisableBackgroundMode) &&
      !command_line->HasSwitch(switches::kDisableExtensions);
  return background_mode_enabled;
#endif
}
