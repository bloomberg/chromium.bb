// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/chrome_unscaled_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::UserMetricsAction;
using extensions::Extension;
using extensions::UpdatedExtensionPermissionsInfo;

BackgroundModeManager::BackgroundModeData::BackgroundModeData(
    int command_id,
    Profile* profile)
    : applications_(new BackgroundApplicationListModel(profile)),
      command_id_(command_id),
      profile_(profile) {
}

BackgroundModeManager::BackgroundModeData::~BackgroundModeData() {
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager::BackgroundModeData, ui::SimpleMenuModel overrides
bool BackgroundModeManager::BackgroundModeData::IsCommandIdChecked(
    int command_id) const {
  NOTREACHED() << "There are no checked items in the profile submenu.";
  return false;
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
      // Launch the app associated with this item.
      const Extension* extension = applications_->
          GetExtension(item);
      BackgroundModeManager::LaunchBackgroundApplication(profile_, extension);
      break;
  }
}

Browser* BackgroundModeManager::BackgroundModeData::GetBrowserWindow() {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_,
      chrome::GetActiveDesktop());
  return browser ? browser : chrome::OpenEmptyWindow(profile_);
}

int BackgroundModeManager::BackgroundModeData::GetBackgroundAppCount() const {
  return applications_->size();
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
    for (extensions::ExtensionList::const_iterator cursor =
             applications_->begin();
         cursor != applications_->end();
         ++cursor, ++position) {
      const gfx::ImageSkia* icon = applications_->GetIcon(*cursor);
      DCHECK(position == applications_->GetPosition(*cursor));
      const std::string& name = (*cursor)->name();
      menu->AddItem(position, UTF8ToUTF16(name));
      if (icon)
        menu->SetIcon(menu->GetItemCount() - 1, gfx::Image(*icon));
    }
  }
  if (containing_menu)
    containing_menu->AddSubMenu(command_id_, name_, menu);
}

void BackgroundModeManager::BackgroundModeData::SetName(
    const string16& new_profile_name) {
  name_ = new_profile_name;
}

string16 BackgroundModeManager::BackgroundModeData::name() {
  return name_;
}

// static
bool BackgroundModeManager::BackgroundModeData::BackgroundModeDataCompare(
    const BackgroundModeData* bmd1,
    const BackgroundModeData* bmd2) {
  return bmd1->name_ < bmd2->name_;
}


///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, public
BackgroundModeManager::BackgroundModeManager(
    CommandLine* command_line,
    ProfileInfoCache* profile_cache)
    : profile_cache_(profile_cache),
      status_tray_(NULL),
      status_icon_(NULL),
      context_menu_(NULL),
      in_background_mode_(false),
      keep_alive_for_startup_(false),
      keep_alive_for_test_(false),
      current_command_id_(0) {
  // We should never start up if there is no browser process or if we are
  // currently quitting.
  CHECK(g_browser_process != NULL);
  CHECK(!browser_shutdown::IsTryingToQuit());

  // Add self as an observer for the profile info cache so we know when profiles
  // are deleted and their names change.
  profile_cache_->AddObserver(this);

  // Listen for the background mode preference changing.
  if (g_browser_process->local_state()) {  // Skip for unit tests
    pref_registrar_.Init(g_browser_process->local_state());
    pref_registrar_.Add(
        prefs::kBackgroundModeEnabled,
        base::Bind(&BackgroundModeManager::OnBackgroundModeEnabledPrefChanged,
                   base::Unretained(this)));
  }

  // Keep the browser alive until extensions are done loading - this is needed
  // by the --no-startup-window flag. We want to stay alive until we load
  // extensions, at which point we should either run in background mode (if
  // there are background apps) or exit if there are none.
  if (command_line->HasSwitch(switches::kNoStartupWindow)) {
    keep_alive_for_startup_ = true;
    browser::StartKeepAlive();
  }

  // If the -keep-alive-for-test flag is passed, then always keep chrome running
  // in the background until the user explicitly terminates it.
  if (command_line->HasSwitch(switches::kKeepAliveForTest))
    keep_alive_for_test_ = true;

  if (ShouldBeInBackgroundMode())
    StartBackgroundMode();

  // Listen for the application shutting down so we can decrement our KeepAlive
  // count.
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

BackgroundModeManager::~BackgroundModeManager() {
  // Remove ourselves from the application observer list (only needed by unit
  // tests since APP_TERMINATING is what does this in a real running system).
  for (BackgroundModeInfoMap::iterator it =
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
void BackgroundModeManager::RegisterPrefs(PrefServiceSimple* prefs) {
  prefs->RegisterBooleanPref(prefs::kUserCreatedLoginItem, false);
  prefs->RegisterBooleanPref(prefs::kUserRemovedLoginItem, false);
  prefs->RegisterBooleanPref(prefs::kBackgroundModeEnabled, true);
}


void BackgroundModeManager::RegisterProfile(Profile* profile) {
  // We don't want to register multiple times for one profile.
  DCHECK(background_mode_data_.find(profile) == background_mode_data_.end());
  BackgroundModeInfo bmd(new BackgroundModeData(current_command_id_++,
                                                profile));
  background_mode_data_[profile] = bmd;

  // Initially set the name for this background mode data.
  size_t index = profile_cache_->GetIndexOfProfileWithPath(profile->GetPath());
  string16 name = l10n_util::GetStringUTF16(IDS_PROFILES_DEFAULT_NAME);
  if (index != std::string::npos)
    name = profile_cache_->GetNameOfProfileAtIndex(index);
  bmd->SetName(name);

  // Listen for when extensions are loaded or add the background permission so
  // we can display a "background app installed" notification and enter
  // "launch on login" mode on the Mac.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
                 content::Source<Profile>(profile));


  // Check for the presence of background apps after all extensions have been
  // loaded, to handle the case where an extension has been manually removed
  // while Chrome was not running.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile));

  bmd->applications_->AddObserver(this);

  // If we're adding a new profile and running in multi-profile mode, this new
  // profile should be added to the status icon if one currently exists.
  if (in_background_mode_ && status_icon_)
    UpdateStatusTrayIconContextMenu();
}

// static
void BackgroundModeManager::LaunchBackgroundApplication(
    Profile* profile,
    const Extension* extension) {
  application_launch::OpenApplication(application_launch::LaunchParams(
          profile, extension, NEW_FOREGROUND_TAB));
}

bool BackgroundModeManager::IsBackgroundModeActiveForTest() {
  return in_background_mode_;
}

int BackgroundModeManager::NumberOfBackgroundModeData() {
  return background_mode_data_.size();
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, content::NotificationObserver overrides
void BackgroundModeManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY:
      // Extensions are loaded, so we don't need to manually keep the browser
      // process alive any more when running in no-startup-window mode.
      EndKeepAliveForStartup();
      break;

    case chrome::NOTIFICATION_EXTENSION_LOADED: {
        Extension* extension = content::Details<Extension>(details).ptr();
        Profile* profile = content::Source<Profile>(source).ptr();
        if (BackgroundApplicationListModel::IsBackgroundApp(
                *extension, profile)) {
          // Extensions loaded after the ExtensionsService is ready should be
          // treated as new installs.
          if (extensions::ExtensionSystem::Get(profile)->extension_service()->
                  is_ready()) {
            OnBackgroundAppInstalled(extension);
          }
        }
      }
      break;
    case chrome::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED: {
        UpdatedExtensionPermissionsInfo* info =
            content::Details<UpdatedExtensionPermissionsInfo>(details).ptr();
        if (info->permissions->HasAPIPermission(
                extensions::APIPermission::kBackground) &&
            info->reason == UpdatedExtensionPermissionsInfo::ADDED) {
          // Turned on background permission, so treat this as a new install.
          OnBackgroundAppInstalled(info->extension);
        }
      }
      break;
    case chrome::NOTIFICATION_APP_TERMINATING:
      // Make sure we aren't still keeping the app alive (only happens if we
      // don't receive an EXTENSIONS_READY notification for some reason).
      EndKeepAliveForStartup();
      // Performing an explicit shutdown, so exit background mode (does nothing
      // if we aren't in background mode currently).
      EndBackgroundMode();
      // Shutting down, so don't listen for any more notifications so we don't
      // try to re-enter/exit background mode again.
      registrar_.RemoveAll();
      for (BackgroundModeInfoMap::iterator it =
               background_mode_data_.begin();
           it != background_mode_data_.end();
           ++it) {
        it->second->applications_->RemoveObserver(this);
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

void BackgroundModeManager::OnBackgroundModeEnabledPrefChanged() {
  if (IsBackgroundModePrefEnabled())
    EnableBackgroundMode();
  else
    DisableBackgroundMode();
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, BackgroundApplicationListModel::Observer overrides
void BackgroundModeManager::OnApplicationDataChanged(
    const Extension* extension, Profile* profile) {
  UpdateStatusTrayIconContextMenu();
}

void BackgroundModeManager::OnApplicationListChanged(Profile* profile) {
  if (!IsBackgroundModePrefEnabled())
    return;

  // Update the profile cache with the fact whether background apps are running
  // for this profile.
  size_t profile_index = profile_cache_->GetIndexOfProfileWithPath(
      profile->GetPath());
  if (profile_index != std::string::npos) {
    profile_cache_->SetBackgroundStatusOfProfileAtIndex(
        profile_index, GetBackgroundAppCountForProfile(profile) != 0);
  }

  if (!ShouldBeInBackgroundMode()) {
    // We've uninstalled our last background app, make sure we exit background
    // mode and no longer launch on startup.
    EnableLaunchOnStartup(false);
    EndBackgroundMode();
  } else {
    // We have at least one background app running - make sure we're in
    // background mode.
    if (!in_background_mode_) {
      // We're entering background mode - make sure we have launch-on-startup
      // enabled.
      // On a Mac, we use 'login items' mechanism which has user-facing UI so we
      // don't want to stomp on user choice every time we start and load
      // registered extensions. This means that if a background app is removed
      // or added while Chrome is not running, we could leave Chrome in the
      // wrong state, but this is better than constantly forcing Chrome to
      // launch on startup even after the user removes the LoginItem manually.
#if !defined(OS_MACOSX)
      EnableLaunchOnStartup(true);
#endif

      StartBackgroundMode();
    }
    // List of applications changed so update the UI.
    UpdateStatusTrayIconContextMenu();
  }
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, ProfileInfoCacheObserver overrides
void BackgroundModeManager::OnProfileAdded(const FilePath& profile_path) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  string16 profile_name = cache.GetNameOfProfileAtIndex(
      cache.GetIndexOfProfileWithPath(profile_path));
  // At this point, the profile should be registered with the background mode
  // manager, but when it's actually added to the cache is when its name is
  // set so we need up to update that with the background_mode_data.
  for (BackgroundModeInfoMap::const_iterator it =
       background_mode_data_.begin();
       it != background_mode_data_.end();
       ++it) {
    if (it->first->GetPath() == profile_path) {
      it->second->SetName(profile_name);
      UpdateStatusTrayIconContextMenu();
      return;
    }
  }
}

void BackgroundModeManager::OnProfileWillBeRemoved(
    const FilePath& profile_path) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  string16 profile_name = cache.GetNameOfProfileAtIndex(
      cache.GetIndexOfProfileWithPath(profile_path));
  // Remove the profile from our map of profiles.
  BackgroundModeInfoMap::iterator it =
      GetBackgroundModeIterator(profile_name);
  // If a profile isn't running a background app, it may not be in the map.
  if (it != background_mode_data_.end()) {
    background_mode_data_.erase(it);
    UpdateStatusTrayIconContextMenu();
  }
}

void BackgroundModeManager::OnProfileWasRemoved(
    const FilePath& profile_path,
    const string16& profile_name) {
}

void BackgroundModeManager::OnProfileNameChanged(
    const FilePath& profile_path,
    const string16& old_profile_name) {
  ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  string16 new_profile_name = cache.GetNameOfProfileAtIndex(
      cache.GetIndexOfProfileWithPath(profile_path));
  BackgroundModeInfoMap::const_iterator it =
      GetBackgroundModeIterator(old_profile_name);
  // We check that the returned iterator is valid due to unittests, but really
  // this should only be called on profiles already known by the background
  // mode manager.
  if (it != background_mode_data_.end()) {
    it->second->SetName(new_profile_name);
    UpdateStatusTrayIconContextMenu();
  }
}

void BackgroundModeManager::OnProfileAvatarChanged(
    const FilePath& profile_path) {

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
  if (command_id == IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND) {
    PrefService* service = g_browser_process->local_state();
    DCHECK(service);
    return service->IsUserModifiablePreference(prefs::kBackgroundModeEnabled);
  }
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
      chrome::ShowAboutChrome(bmd->GetBrowserWindow());
      break;
    case IDC_TASK_MANAGER:
      chrome::OpenTaskManager(bmd->GetBrowserWindow(), true);
      break;
    case IDC_EXIT:
      content::RecordAction(UserMetricsAction("Exit"));
      browser::AttemptExit();
      break;
    case IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND: {
      // Background mode must already be enabled (as otherwise this menu would
      // not be visible).
      DCHECK(IsBackgroundModePrefEnabled());
      DCHECK(browser::WillKeepAlive());

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
        FROM_HERE, base::Bind(&browser::EndKeepAlive));
  }
}

void BackgroundModeManager::StartBackgroundMode() {
  DCHECK(ShouldBeInBackgroundMode());
  // Don't bother putting ourselves in background mode if we're already there
  // or if background mode is disabled.
  if (in_background_mode_)
    return;

  // Mark ourselves as running in background mode.
  in_background_mode_ = true;

  // Put ourselves in KeepAlive mode and create a status tray icon.
  browser::StartKeepAlive();

  // Display a status icon to exit Chrome.
  InitStatusTrayIcon();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_MODE_CHANGED,
      content::Source<BackgroundModeManager>(this),
      content::Details<bool>(&in_background_mode_));
}

void BackgroundModeManager::InitStatusTrayIcon() {
  // Only initialize status tray icons for those profiles which actually
  // have a background app running.
  if (ShouldBeInBackgroundMode())
    CreateStatusTrayIcon();
}

void BackgroundModeManager::EndBackgroundMode() {
  if (!in_background_mode_)
    return;
  in_background_mode_ = false;

  // End KeepAlive mode and blow away our status tray icon.
  browser::EndKeepAlive();

  RemoveStatusTrayIcon();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_MODE_CHANGED,
      content::Source<BackgroundModeManager>(this),
      content::Details<bool>(&in_background_mode_));
}

void BackgroundModeManager::EnableBackgroundMode() {
  DCHECK(IsBackgroundModePrefEnabled());
  // If background mode should be enabled, but isn't, turn it on.
  if (!in_background_mode_ && ShouldBeInBackgroundMode()) {
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

int BackgroundModeManager::GetBackgroundAppCount() const {
  int count = 0;
  // Walk the BackgroundModeData for all profiles and count the number of apps.
  for (BackgroundModeInfoMap::const_iterator it =
       background_mode_data_.begin();
       it != background_mode_data_.end();
       ++it) {
    count += it->second->GetBackgroundAppCount();
  }
  DCHECK(count >= 0);
  return count;
}

int BackgroundModeManager::GetBackgroundAppCountForProfile(
    Profile* const profile) const {
  BackgroundModeData* bmd = GetBackgroundModeData(profile);
  return bmd->GetBackgroundAppCount();
}

bool BackgroundModeManager::ShouldBeInBackgroundMode() const {
  return IsBackgroundModePrefEnabled() &&
      (GetBackgroundAppCount() > 0 || keep_alive_for_test_);
}

void BackgroundModeManager::OnBackgroundAppInstalled(
    const Extension* extension) {
  // Background mode is disabled - don't do anything.
  if (!IsBackgroundModePrefEnabled())
    return;

  // Special behavior for the Mac: We enable "launch-on-startup" only on new app
  // installation rather than every time we go into background mode. This is
  // because the Mac exposes "Open at Login" UI to the user and we don't want to
  // clobber the user's selection on every browser launch.
  // Other platforms enable launch-on-startup in OnApplicationListChanged().
#if defined(OS_MACOSX)
  EnableLaunchOnStartup(true);
#endif

  // Check if we need a status tray icon and make one if we do (needed so we
  // can display the app-installed notification below).
  CreateStatusTrayIcon();

  // Notify the user that a background app has been installed.
  if (extension)  // NULL when called by unit tests.
    DisplayAppInstalledNotification(extension);
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
  gfx::ImageSkia* image_skia = ui::ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  status_icon_->SetImage(*image_skia);
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

  // We should only get here if we have a profile loaded, or if we're running
  // in test mode.
  if (background_mode_data_.empty()) {
    DCHECK(keep_alive_for_test_);
    return;
  }

  // TODO(rlp): Add current profile color or indicator.
  // Create a context menu item for Chrome.
  ui::SimpleMenuModel* menu = new ui::SimpleMenuModel(this);
  // Add About item
  menu->AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
  menu->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  menu->AddSeparator(ui::NORMAL_SEPARATOR);

  if (profile_cache_->GetNumberOfProfiles() > 1) {
    std::vector<BackgroundModeData*> bmd_vector;
    for (BackgroundModeInfoMap::iterator it =
         background_mode_data_.begin();
         it != background_mode_data_.end();
         ++it) {
       bmd_vector.push_back(it->second.get());
    }
    std::sort(bmd_vector.begin(), bmd_vector.end(),
              &BackgroundModeData::BackgroundModeDataCompare);
    int profiles_with_apps = 0;
    for (std::vector<BackgroundModeData*>::const_iterator bmd_it =
         bmd_vector.begin();
         bmd_it != bmd_vector.end();
         ++bmd_it) {
      BackgroundModeData* bmd = *bmd_it;
      // We should only display the profile in the status icon if it has at
      // least one background app.
      if (bmd->GetBackgroundAppCount() > 0) {
        ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(bmd);
        bmd->BuildProfileMenu(submenu, menu);
        profiles_with_apps++;
      }
    }
    // We should only be displaying the status tray icon if there is at least
    // one profile with a background app.
    DCHECK_GT(profiles_with_apps, 0);
  } else {
    // We should only have one profile in the cache if we are not
    // using multi-profiles. If keep_alive_for_test_ is set, then we may not
    // have any profiles in the cache.
    DCHECK(profile_cache_->GetNumberOfProfiles() == size_t(1) ||
           keep_alive_for_test_);
    background_mode_data_.begin()->second->BuildProfileMenu(menu, NULL);
    menu->AddSeparator(ui::NORMAL_SEPARATOR);
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
BackgroundModeManager::GetBackgroundModeData(Profile* const profile) const {
  DCHECK(background_mode_data_.find(profile) != background_mode_data_.end());
  return background_mode_data_.find(profile)->second.get();
}

BackgroundModeManager::BackgroundModeInfoMap::iterator
BackgroundModeManager::GetBackgroundModeIterator(
    const string16& profile_name) {
  BackgroundModeInfoMap::iterator profile_it =
      background_mode_data_.end();
  for (BackgroundModeInfoMap::iterator it =
       background_mode_data_.begin();
       it != background_mode_data_.end();
       ++it) {
    if (it->second->name() == profile_name) {
      profile_it = it;
    }
  }
  return profile_it;
}

bool BackgroundModeManager::IsBackgroundModePrefEnabled() const {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBackgroundModeEnabled);
}
