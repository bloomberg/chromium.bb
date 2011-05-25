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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"


BackgroundModeManager::BackgroundModeData::BackgroundModeData(
    Profile* profile,
    BackgroundModeManager* background_mode_manager)
    : applications_(new BackgroundApplicationListModel(profile)),
      status_icon_(NULL),
      context_menu_(NULL),
      context_menu_application_offset_(0),
      profile_(profile),
      background_mode_manager_(background_mode_manager) {
}

BackgroundModeManager::BackgroundModeData::~BackgroundModeData() {
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager::BackgroundModeData, ui::SimpleMenuModel overrides
bool BackgroundModeManager::BackgroundModeData::IsCommandIdChecked(
    int command_id) const {
  DCHECK(command_id == IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND);
  return true;
}

bool BackgroundModeManager::BackgroundModeData::IsCommandIdEnabled(
    int command_id) const {
  // For now, we do not support disabled items.
  return true;
}

bool BackgroundModeManager::BackgroundModeData::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  // No accelerators for status icon context menus.
  return false;
}

void BackgroundModeManager::BackgroundModeData::ExecuteCommand(int item) {
  switch (item) {
    case IDC_ABOUT:
      GetBrowserWindow()->OpenAboutChromeDialog();
      break;
    case IDC_EXIT:
      UserMetrics::RecordAction(UserMetricsAction("Exit"));
      BrowserList::CloseAllBrowsersAndExit();
      break;
    case IDC_OPTIONS:
      GetBrowserWindow()->OpenOptionsDialog();
      break;
    case IDC_TASK_MANAGER:
      GetBrowserWindow()->OpenTaskManager(true);
      break;
    case IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND: {
      // Background mode must already be enabled (as otherwise this menu would
      // not be visible).
      DCHECK(background_mode_manager_->IsBackgroundModePrefEnabled());
      DCHECK(BrowserList::WillKeepAlive());

      // Set the background mode pref to "disabled" - the resulting notification
      // will result in a call to DisableBackgroundMode().
      PrefService* service = g_browser_process->local_state();
      DCHECK(service);
      service->SetBoolean(prefs::kBackgroundModeEnabled, false);
      break;
    }
    default:
      ExecuteApplication(item);
      break;
  }
}

void BackgroundModeManager::BackgroundModeData::ExecuteApplication(
    int item) {
  Browser* browser = GetBrowserWindow();
  const Extension* extension = applications_->GetExtension(item);
  browser->OpenApplicationTab(profile_, extension, NEW_FOREGROUND_TAB);
}

Browser* BackgroundModeManager::BackgroundModeData::GetBrowserWindow() {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser) {
    Browser::OpenEmptyWindow(profile_);
    browser = BrowserList::GetLastActiveWithProfile(profile_);
  }
  return browser;
}

void BackgroundModeManager::BackgroundModeData::UpdateContextMenuEntryIcon(
    const Extension* extension) {
  if (!context_menu_)
    return;
  context_menu_->SetIcon(
      context_menu_application_offset_ +
      applications_->GetPosition(extension),
      *(applications_->GetIcon(extension)));

  status_icon_->SetContextMenu(context_menu_);  // for Update effect
}

bool BackgroundModeManager::BackgroundModeData::HasBackgroundApp() {
  return (applications_->size() > 0);
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, public
BackgroundModeManager::BackgroundModeManager(CommandLine* command_line)
    : status_tray_(NULL),
      background_app_count_(0),
      in_background_mode_(false),
      keep_alive_for_startup_(false) {
  // If background mode is currently disabled, just exit - don't listen for any
  // notifications.
  if (IsBackgroundModePermanentlyDisabled(command_line))
    return;

  // Listen for the background mode preference changing.
  if (g_browser_process->local_state()) {  // Skip for unit tests
    pref_registrar_.Init(g_browser_process->local_state());
    pref_registrar_.Add(prefs::kBackgroundModeEnabled, this);
  }

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
  if (command_line->HasSwitch(switches::kKeepAliveForTest))
    OnBackgroundAppLoaded();

  // Listen for the application shutting down so we can decrement our KeepAlive
  // count.
  registrar_.Add(this, NotificationType::APP_TERMINATING,
                 NotificationService::AllSources());
}

BackgroundModeManager::~BackgroundModeManager() {
  for (std::map<Profile*, BackgroundModeInfo>::iterator it =
       background_mode_data_.begin();
       it != background_mode_data_.end();
       ++it) {
    it->second->applications_->RemoveObserver(this);
  }

  // We're going away, so exit background mode (does nothing if we aren't in
  // background mode currently). This is primarily needed for unit tests,
  // because in an actual running system we'd get an APP_TERMINATING
  // notification before being destroyed.
  EndBackgroundMode();
}

// static
void BackgroundModeManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kUserCreatedLoginItem, false);
  prefs->RegisterBooleanPref(prefs::kBackgroundModeEnabled, true);
}


void BackgroundModeManager::RegisterProfile(Profile* profile) {
  // We don't want to register multiple times for one profile.
  DCHECK(background_mode_data_.find(profile) == background_mode_data_.end());
  BackgroundModeInfo bmd(new BackgroundModeData(profile, this));
  background_mode_data_[profile] = bmd;

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

  background_mode_data_[profile]->applications_->AddObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, NotificationObserver overrides
void BackgroundModeManager::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::PREF_CHANGED:
      DCHECK(*Details<std::string>(details).ptr() ==
             prefs::kBackgroundModeEnabled);
      if (IsBackgroundModePrefEnabled())
        EnableBackgroundMode();
      else
        DisableBackgroundMode();
      break;
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
          Profile* profile = Source<Profile>(source).ptr();
          if (profile->GetExtensionService()->is_ready())
            OnBackgroundAppInstalled(extension, profile);
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
        Profile* profile = Source<Profile>(source).ptr();
        OnBackgroundAppUninstalled(profile);
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

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, BackgroundApplicationListModel::Observer overrides
void BackgroundModeManager::OnApplicationDataChanged(
    const Extension* extension, Profile* profile) {
  UpdateContextMenuEntryIcon(extension, profile);
}

void BackgroundModeManager::OnApplicationListChanged(Profile* profile) {
  UpdateStatusTrayIconContextMenu(profile);
}


///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, private
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
  // The count here is across all profiles since we must have background
  // mode if there is even one.
  background_app_count_++;
  if (background_app_count_ == 1)
    StartBackgroundMode();
}

void BackgroundModeManager::StartBackgroundMode() {
  // Don't bother putting ourselves in background mode if we're already there
  // or if background mode is disabled.
  if (in_background_mode_ || !IsBackgroundModePrefEnabled())
    return;

  // Mark ourselves as running in background mode.
  in_background_mode_ = true;

  // Put ourselves in KeepAlive mode and create a status tray icon.
  BrowserList::StartKeepAlive();

  // Display a status icon to exit Chrome.
  InitStatusTrayIcons();
}

void BackgroundModeManager::InitStatusTrayIcons() {
  // Only initialize status tray icons for those profiles which actually
  // have a background app running.
  for (std::map<Profile*, BackgroundModeInfo>::iterator it =
       background_mode_data_.begin();
       it != background_mode_data_.end();
       ++it) {
    if (it->second->HasBackgroundApp())
      CreateStatusTrayIcon(it->first);
  }
}

void BackgroundModeManager::OnBackgroundAppUnloaded() {
  // When a background app unloads, decrement our count and also end
  // KeepAlive mode if appropriate.
  background_app_count_--;
  DCHECK_GE(background_app_count_, 0);
  if (background_app_count_ == 0)
    EndBackgroundMode();
}

void BackgroundModeManager::EndBackgroundMode() {
  if (!in_background_mode_)
    return;
  in_background_mode_ = false;

  // End KeepAlive mode and blow away our status tray icon.
  BrowserList::EndKeepAlive();
  // There is a status tray icon for each profile. Blow them all away.
  for (std::map<Profile*, BackgroundModeInfo>::iterator it =
       background_mode_data_.begin();
       it != background_mode_data_.end();
       ++it) {
    RemoveStatusTrayIcon(it->first);
  }
}

void BackgroundModeManager::EnableBackgroundMode() {
  DCHECK(IsBackgroundModePrefEnabled());
  // If background mode should be enabled, but isn't, turn it on.
  if (background_app_count_ > 0 && !in_background_mode_) {
    StartBackgroundMode();
    EnableLaunchOnStartup(true);
  }
}

void BackgroundModeManager::DisableBackgroundMode() {
  DCHECK(!IsBackgroundModePrefEnabled());
  // If background mode is currently enabled, turn it off.
  if (in_background_mode_) {
    EndBackgroundMode();
    EnableLaunchOnStartup(false);
  }
}

void BackgroundModeManager::OnBackgroundAppInstalled(
    const Extension* extension, Profile* profile) {
  // Background mode is disabled - don't do anything.
  if (!IsBackgroundModePrefEnabled())
    return;

  // We're installing a background app. If this is the first background app
  // being installed, make sure we are set to launch on startup.
  if (background_app_count_ == 0)
    EnableLaunchOnStartup(true);

  // Check if we need a status tray icon and make one if we do.
  CreateStatusTrayIcon(profile);

  // Notify the user that a background app has been installed.
  if (extension)  // NULL when called by unit tests.
    DisplayAppInstalledNotification(extension, profile);
}

void BackgroundModeManager::OnBackgroundAppUninstalled(Profile* profile) {
  // Check if we need to remove the status tray icon if there are no
  // more background apps.
  BackgroundModeInfo bmd = GetBackgroundModeInfo(profile);
  DCHECK(bmd.get());
  // If there are still background apps for this profile, don't remove
  // the status tray icon.
  if (!bmd->HasBackgroundApp())
    RemoveStatusTrayIcon(profile);

  // When uninstalling a background app, disable launch on startup if
  // we have no more background apps.
  if (background_app_count_ == 0)
    EnableLaunchOnStartup(false);
}

void BackgroundModeManager::CreateStatusTrayIcon(Profile* profile) {
  // Only need status icons on windows/linux. ChromeOS doesn't allow exiting
  // Chrome and Mac can use the dock icon instead.

  // Since there are multiple profiles which share the status tray, we now
  // use the browser process to keep track of it.
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  if (!status_tray_)
    status_tray_ = g_browser_process->status_tray();
#endif

  // If the platform doesn't support status icons, or we've already created
  // our status icon, just return.
  BackgroundModeInfo bmd = GetBackgroundModeInfo(profile);
  if (!status_tray_ || bmd->status_icon_)
    return;

  bmd->status_icon_ = status_tray_->CreateStatusIcon();
  if (!bmd->status_icon_)
    return;

  // Set the image and add ourselves as a click observer on it.
  // TODO(rlp): Status tray icon should have submenus for each profile.
  SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON);
  bmd->status_icon_->SetImage(*bitmap);
  bmd->status_icon_->SetToolTip(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  UpdateStatusTrayIconContextMenu(profile);
}

void BackgroundModeManager::UpdateContextMenuEntryIcon(
    const Extension* extension, Profile* profile) {
  BackgroundModeInfo bmd = GetBackgroundModeInfo(profile);
  DCHECK(bmd.get());
  bmd->UpdateContextMenuEntryIcon(extension);
}

void BackgroundModeManager::UpdateStatusTrayIconContextMenu(Profile* profile) {
  BackgroundModeInfo bmd = GetBackgroundModeInfo(profile);
  DCHECK(bmd.get());
  if (!bmd->status_icon_) {
    // If no status icon exists, it's either because one wasn't created when
    // it should have been which can happen when extensions load after the
    // profile has already been registered with the background mode manager.
    // The other case is if we aren't in background mode.
    if (in_background_mode_)
      CreateStatusTrayIcon(profile);
    else
      return;
  }

  // TODO(rlp): Add current profile color.
  // Create a context menu item for Chrome.
  ui::SimpleMenuModel* menu = new ui::SimpleMenuModel(bmd.get());
  // Add About item
  menu->AddItem(IDC_ABOUT, l10n_util::GetStringFUTF16(IDS_ABOUT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  menu->AddItem(IDC_OPTIONS, GetPreferencesMenuLabel());
  menu->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  menu->AddSeparator();
  int position = 0;
  bmd->context_menu_application_offset_ = menu->GetItemCount();
  for (ExtensionList::const_iterator cursor = bmd->applications_->begin();
       cursor != bmd->applications_->end();
       ++cursor, ++position) {
    const SkBitmap* icon = bmd->applications_->GetIcon(*cursor);
    DCHECK(position == bmd->applications_->GetPosition(*cursor));
    const std::string& name = (*cursor)->name();
    menu->AddItem(position, UTF8ToUTF16(name));
    if (icon)
      menu->SetIcon(menu->GetItemCount() - 1, *icon);
  }
  if (bmd->applications_->size() > 0)
    menu->AddSeparator();
  menu->AddCheckItemWithStringId(
      IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND,
      IDS_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND);
  menu->AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  bmd->context_menu_ = menu;
  bmd->status_icon_->SetContextMenu(menu);
}

void BackgroundModeManager::RemoveStatusTrayIcon(Profile* profile) {
  BackgroundModeInfo bmd = GetBackgroundModeInfo(profile);
  DCHECK(bmd.get());

  if (bmd->status_icon_)
    status_tray_->RemoveStatusIcon(bmd->status_icon_);
  bmd->status_icon_ = NULL;
  bmd->context_menu_ = NULL;  // Do not delete, points within |status_icon_|.
}

BackgroundModeManager::BackgroundModeInfo
BackgroundModeManager::GetBackgroundModeInfo(Profile* profile) {
  DCHECK(background_mode_data_.find(profile) != background_mode_data_.end());
  return background_mode_data_[profile];
}

// static
bool BackgroundModeManager::IsBackgroundModePermanentlyDisabled(
    const CommandLine* command_line) {

  // Background mode is disabled if the appropriate flag is passed, or if
  // extensions are disabled, or if the associated preference is unset. It's
  // always disabled on chromeos since chrome is always running on that
  // platform, making it superfluous.
#if defined(OS_CHROMEOS)
  return true;
#else
  bool background_mode_disabled =
      command_line->HasSwitch(switches::kDisableBackgroundMode) ||
      command_line->HasSwitch(switches::kDisableExtensions);
  return background_mode_disabled;
#endif
}

bool BackgroundModeManager::IsBackgroundModePrefEnabled() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBackgroundModeEnabled);
}