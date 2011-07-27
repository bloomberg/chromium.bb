// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/browser/user_metrics.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

BackgroundModeManager::BackgroundModeData::BackgroundModeData(
    int command_id,
    Profile* profile,
    BackgroundModeManager* background_mode_manager)
    : applications_(new BackgroundApplicationListModel(profile)),
      command_id_(command_id),
      profile_(profile),
      background_mode_manager_(background_mode_manager) {
  name_ = UTF8ToUTF16(profile_->GetProfileName());
  if (name_.empty())
    name_ = l10n_util::GetStringUTF16(IDS_PROFILES_DEFAULT_NAME);
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
  return command_id != IDC_MinimumLabelValue;
}

bool BackgroundModeManager::BackgroundModeData::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  // No accelerators for status icon context menus.
  return false;
}

void BackgroundModeManager::BackgroundModeData::ExecuteCommand(int item) {
  switch (item) {
    case IDC_MinimumLabelValue:
      // Do nothing. This is just a label.
      break;
    default:
      Browser* browser = GetBrowserWindow();
      const Extension* extension = applications_->GetExtension(item);
      browser->OpenApplicationTab(profile_, extension, NEW_FOREGROUND_TAB);
      break;
  }
}

Browser* BackgroundModeManager::BackgroundModeData::GetBrowserWindow() {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser) {
    Browser::OpenEmptyWindow(profile_);
    browser = BrowserList::GetLastActiveWithProfile(profile_);
  }
  return browser;
}

bool BackgroundModeManager::BackgroundModeData::HasBackgroundApp() {
  return (applications_->size() > 0);
}

void BackgroundModeManager::BackgroundModeData::BuildProfileMenu(
    ui::SimpleMenuModel* menu,
    ui::SimpleMenuModel* containing_menu) {
  int position = 0;
  // When there are no background applications, we want to display
  // just a label stating that none are running.
  if (applications_->size() < 1) {
    menu->AddItemWithStringId(IDC_MinimumLabelValue,
                              IDS_BACKGROUND_APP_NOT_INSTALLED);
  } else {
    for (ExtensionList::const_iterator cursor = applications_->begin();
         cursor != applications_->end();
         ++cursor, ++position) {
      const SkBitmap* icon = applications_->GetIcon(*cursor);
      DCHECK(position == applications_->GetPosition(*cursor));
      const std::string& name = (*cursor)->name();
      menu->AddItem(position, UTF8ToUTF16(name));
      if (icon)
        menu->SetIcon(menu->GetItemCount() - 1, *icon);
    }
  }
  if (containing_menu)
    containing_menu->AddSubMenu(command_id_, name_, menu);
}

// static
bool BackgroundModeManager::BackgroundModeData::BackgroundModeDataCompare(
    const BackgroundModeData* bmd1,
    const BackgroundModeData* bmd2) {
  return bmd1->name_ < bmd2->name_;
}


///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, public
BackgroundModeManager::BackgroundModeManager(CommandLine* command_line)
    : status_tray_(NULL),
      status_icon_(NULL),
      context_menu_(NULL),
      background_app_count_(0),
      in_background_mode_(false),
      keep_alive_for_startup_(false),
      current_command_id_(0) {
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
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
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
  BackgroundModeInfo bmd(new BackgroundModeData(current_command_id_++,
                                                profile, this));
  background_mode_data_[profile] = bmd;

  // Listen for when extensions are loaded/unloaded so we can track the
  // number of background apps and modify our keep-alive and launch-on-startup
  // state appropriately.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 Source<Profile>(profile));

  // Check for the presence of background apps after all extensions have been
  // loaded, to handle the case where an extension has been manually removed
  // while Chrome was not running.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 Source<Profile>(profile));

  bmd->applications_->AddObserver(this);

  // If we're adding a new profile and running in multi-profile mode, this new
  // profile should be added to the status icon if one currently exists.
  if (in_background_mode_ && status_icon_)
    UpdateStatusTrayIconContextMenu();
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, NotificationObserver overrides
void BackgroundModeManager::Observe(int type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED:
      DCHECK(*Details<std::string>(details).ptr() ==
             prefs::kBackgroundModeEnabled);
      if (IsBackgroundModePrefEnabled())
        EnableBackgroundMode();
      else
        DisableBackgroundMode();
      break;
    case chrome::NOTIFICATION_EXTENSIONS_READY:
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
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
        Extension* extension = Details<Extension>(details).ptr();
        if (BackgroundApplicationListModel::IsBackgroundApp(*extension)) {
          // Extensions loaded after the ExtensionsService is ready should be
          // treated as new installs.
          Profile* profile = Source<Profile>(source).ptr();
          if (profile->GetExtensionService()->is_ready())
            OnBackgroundAppInstalled(extension);
          OnBackgroundAppLoaded();
        }
      }
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
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
    case content::NOTIFICATION_APP_TERMINATING:
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
  UpdateStatusTrayIconContextMenu();
}

void BackgroundModeManager::OnApplicationListChanged(Profile* profile) {
  UpdateStatusTrayIconContextMenu();
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager::BackgroundModeData, ui::SimpleMenuModel overrides
bool BackgroundModeManager::IsCommandIdChecked(
    int command_id) const {
  DCHECK(command_id == IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND);
  return true;
}

bool BackgroundModeManager::IsCommandIdEnabled(
    int command_id) const {
  return command_id != IDC_MinimumLabelValue;
}

bool BackgroundModeManager::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  // No accelerators for status icon context menus.
  return false;
}

void BackgroundModeManager::ExecuteCommand(int command_id) {
  // When a browser window is necessary, we use the first profile. The windows
  // opened for these commands are not profile-specific, so any profile would
  // work and the first is convenient.
  BackgroundModeData* bmd = background_mode_data_.begin()->second.get();
  switch (command_id) {
    case IDC_ABOUT:
      bmd->GetBrowserWindow()->OpenAboutChromeDialog();
      break;
    case IDC_TASK_MANAGER:
      bmd->GetBrowserWindow()->OpenTaskManager(true);
      break;
    case IDC_EXIT:
      UserMetrics::RecordAction(UserMetricsAction("Exit"));
      BrowserList::AttemptExit();
      break;
    case IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND: {
      // Background mode must already be enabled (as otherwise this menu would
      // not be visible).
      DCHECK(IsBackgroundModePrefEnabled());
      DCHECK(BrowserList::WillKeepAlive());

      // Set the background mode pref to "disabled" - the resulting notification
      // will result in a call to DisableBackgroundMode().
      PrefService* service = g_browser_process->local_state();
      DCHECK(service);
      service->SetBoolean(prefs::kBackgroundModeEnabled, false);
      break;
    }
    default:
      bmd->ExecuteCommand(command_id);
      break;
  }
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
  InitStatusTrayIcon();
}

void BackgroundModeManager::InitStatusTrayIcon() {
  // Only initialize status tray icons for those profiles which actually
  // have a background app running.
  for (std::map<Profile*, BackgroundModeInfo>::iterator it =
       background_mode_data_.begin();
       it != background_mode_data_.end();
       ++it) {
    if (it->second->HasBackgroundApp()) {
      // Once we've found a profile with a background app, we know to create
      // the icon.
      CreateStatusTrayIcon();
      return;
    }
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

  RemoveStatusTrayIcon();
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
    const Extension* extension) {
  // Background mode is disabled - don't do anything.
  if (!IsBackgroundModePrefEnabled())
    return;

  // We're installing a background app. If this is the first background app
  // being installed, make sure we are set to launch on startup.
  if (background_app_count_ == 0)
    EnableLaunchOnStartup(true);

  // Check if we need a status tray icon and make one if we do.
  CreateStatusTrayIcon();

  // Notify the user that a background app has been installed.
  if (extension)  // NULL when called by unit tests.
    DisplayAppInstalledNotification(extension);
}

void BackgroundModeManager::OnBackgroundAppUninstalled() {
  UpdateStatusTrayIconContextMenu();

  // When uninstalling a background app, disable launch on startup if
  // we have no more background apps.
  if (background_app_count_ == 0)
    EnableLaunchOnStartup(false);
}

void BackgroundModeManager::CreateStatusTrayIcon() {
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
  if (!status_tray_ || status_icon_)
    return;

  status_icon_ = status_tray_->CreateStatusIcon();
  if (!status_icon_)
    return;

  // Set the image and add ourselves as a click observer on it.
  // TODO(rlp): Status tray icon should have submenus for each profile.
  SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON);
  status_icon_->SetImage(*bitmap);
  status_icon_->SetToolTip(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  UpdateStatusTrayIconContextMenu();
}

void BackgroundModeManager::UpdateStatusTrayIconContextMenu() {
  // If no status icon exists, it's either because one wasn't created when
  // it should have been which can happen when extensions load after the
  // profile has already been registered with the background mode manager.
  if (in_background_mode_ && !status_icon_)
     CreateStatusTrayIcon();

  // If we don't have a status icon or one could not be created succesfully,
  // then no need to continue the update.
  if (!status_icon_)
    return;

  // TODO(rlp): Add current profile color or indicator.
  // Create a context menu item for Chrome.
  ui::SimpleMenuModel* menu = new ui::SimpleMenuModel(this);
  // Add About item
  menu->AddItem(IDC_ABOUT, l10n_util::GetStringFUTF16(IDS_ABOUT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  menu->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  menu->AddSeparator();

  if (background_mode_data_.size() > 1) {
    std::vector<BackgroundModeData*> bmd_vector;
    for (std::map<Profile*, BackgroundModeInfo>::iterator it =
         background_mode_data_.begin();
         it != background_mode_data_.end();
         ++it) {
       bmd_vector.push_back(it->second.get());
    }
    std::sort(bmd_vector.begin(), bmd_vector.end(),
              &BackgroundModeData::BackgroundModeDataCompare);
    for (std::vector<BackgroundModeData*>::const_iterator bmd_it =
         bmd_vector.begin();
         bmd_it != bmd_vector.end();
         ++bmd_it) {
      BackgroundModeData* bmd = *bmd_it;
      ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(bmd);
      bmd->BuildProfileMenu(submenu, menu);
    }
  } else {
    // We should only have one profile in the list if we are not
    // using multi-profiles.
    DCHECK_EQ(background_mode_data_.size(), size_t(1));
    background_mode_data_.begin()->second->BuildProfileMenu(menu, NULL);
    menu->AddSeparator();
  }
  menu->AddCheckItemWithStringId(
      IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND,
      IDS_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND);
  menu->AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  context_menu_ = menu;
  status_icon_->SetContextMenu(menu);
}

void BackgroundModeManager::RemoveStatusTrayIcon() {
  if (status_icon_)
    status_tray_->RemoveStatusIcon(status_icon_);
  status_icon_ = NULL;
  context_menu_ = NULL;
}

BackgroundModeManager::BackgroundModeData*
BackgroundModeManager::GetBackgroundModeData(Profile* profile) {
  DCHECK(background_mode_data_.find(profile) != background_mode_data_.end());
  return background_mode_data_[profile].get();
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
