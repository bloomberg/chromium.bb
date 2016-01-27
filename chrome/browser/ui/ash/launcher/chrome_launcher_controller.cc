// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include <stddef.h>

#include <vector>

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/multi_profile_uma.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/app_icon_loader_impl.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/app_sync_ui_state.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_browser.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_tab.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_types.h"
#include "chrome/browser/ui/ash/launcher/launcher_app_tab_helper.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/url_pattern.h"
#include "grit/ash_resources.h"
#include "grit/theme_resources.h"
#include "net/base/url_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/wm/core/window_animations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/ash/launcher/multi_profile_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/multi_profile_browser_status_monitor.h"
#include "components/user_manager/user_manager.h"
#endif

using extensions::Extension;
using extensions::UnloadedExtensionInfo;
using extension_misc::kGmailAppId;
using content::WebContents;

// static
ChromeLauncherController* ChromeLauncherController::instance_ = NULL;

namespace {

// This will be used as placeholder in the list of the pinned applciatons.
// Note that this is NOT a valid extension identifier so that pre M31 versions
// will ignore it.
const char kAppShelfIdPlaceholder[] = "AppShelfIDPlaceholder--------";

std::string GetPrefKeyForRootWindow(aura::Window* root_window) {
  gfx::Display display =
      gfx::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  DCHECK(display.is_valid());

  return base::Int64ToString(display.id());
}

void UpdatePerDisplayPref(PrefService* pref_service,
                          aura::Window* root_window,
                          const char* pref_key,
                          const std::string& value) {
  std::string key = GetPrefKeyForRootWindow(root_window);
  if (key.empty())
    return;

  DictionaryPrefUpdate update(pref_service, prefs::kShelfPreferences);
  base::DictionaryValue* shelf_prefs = update.Get();
  base::DictionaryValue* prefs = NULL;
  if (!shelf_prefs->GetDictionary(key, &prefs)) {
    prefs = new base::DictionaryValue();
    shelf_prefs->Set(key, prefs);
  }
  prefs->SetStringWithoutPathExpansion(pref_key, value);
}

// Returns a pref value in |pref_service| for the display of |root_window|. The
// pref value is stored in |local_path| and |path|, but |pref_service| may have
// per-display preferences and the value can be specified by policy. Here is
// the priority:
//  * A value managed by policy. This is a single value that applies to all
//    displays.
//  * A user-set value for the specified display.
//  * A user-set value in |local_path| or |path|, if no per-display settings are
//    ever specified (see http://crbug.com/173719 for why). |local_path| is
//    preferred. See comment in |kShelfAlignment| as to why we consider two
//    prefs and why |local_path| is preferred.
//  * A value recommended by policy. This is a single value that applies to all
//    root windows.
//  * The default value for |local_path| if the value is not recommended by
//    policy.
std::string GetPrefForRootWindow(PrefService* pref_service,
                                 aura::Window* root_window,
                                 const char* local_path,
                                 const char* path) {
  const PrefService::Preference* local_pref =
      pref_service->FindPreference(local_path);
  const std::string value(pref_service->GetString(local_path));
  if (local_pref->IsManaged())
    return value;

  std::string pref_key = GetPrefKeyForRootWindow(root_window);
  bool has_per_display_prefs = false;
  if (!pref_key.empty()) {
    const base::DictionaryValue* shelf_prefs = pref_service->GetDictionary(
        prefs::kShelfPreferences);
    const base::DictionaryValue* display_pref = NULL;
    std::string per_display_value;
    if (shelf_prefs->GetDictionary(pref_key, &display_pref) &&
        display_pref->GetString(path, &per_display_value))
      return per_display_value;

    // If the pref for the specified display is not found, scan the whole prefs
    // and check if the prefs for other display is already specified.
    std::string unused_value;
    for (base::DictionaryValue::Iterator iter(*shelf_prefs);
         !iter.IsAtEnd(); iter.Advance()) {
      const base::DictionaryValue* display_pref = NULL;
      if (iter.value().GetAsDictionary(&display_pref) &&
          display_pref->GetString(path, &unused_value)) {
        has_per_display_prefs = true;
        break;
      }
    }
  }

  if (local_pref->IsRecommended() || !has_per_display_prefs)
    return value;

  const base::Value* default_value =
      pref_service->GetDefaultPrefValue(local_path);
  std::string default_string;
  default_value->GetAsString(&default_string);
  return default_string;
}

// Gets the shelf auto hide behavior from prefs for a root window.
ash::ShelfAutoHideBehavior GetShelfAutoHideBehaviorFromPrefs(
    Profile* profile,
    aura::Window* root_window) {
  DCHECK(profile);

  // Don't show the shelf in app mode.
  if (chrome::IsRunningInAppMode())
    return ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN;

  // See comment in |kShelfAlignment| as to why we consider two prefs.
  const std::string behavior_value(
      GetPrefForRootWindow(profile->GetPrefs(),
                           root_window,
                           prefs::kShelfAutoHideBehaviorLocal,
                           prefs::kShelfAutoHideBehavior));

  // Note: To maintain sync compatibility with old images of chrome/chromeos
  // the set of values that may be encountered includes the now-extinct
  // "Default" as well as "Never" and "Always", "Default" should now
  // be treated as "Never" (http://crbug.com/146773).
  if (behavior_value == ash::kShelfAutoHideBehaviorAlways)
    return ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  return ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
}

// Gets the shelf alignment from prefs for a root window.
ash::ShelfAlignment GetShelfAlignmentFromPrefs(Profile* profile,
                                               aura::Window* root_window) {
  DCHECK(profile);

  // See comment in |kShelfAlignment| as to why we consider two prefs.
  const std::string alignment_value(
      GetPrefForRootWindow(profile->GetPrefs(),
                           root_window,
                           prefs::kShelfAlignmentLocal,
                           prefs::kShelfAlignment));
  if (alignment_value == ash::kShelfAlignmentLeft)
    return ash::SHELF_ALIGNMENT_LEFT;
  else if (alignment_value == ash::kShelfAlignmentRight)
    return ash::SHELF_ALIGNMENT_RIGHT;
  else if (alignment_value == ash::kShelfAlignmentTop)
    return ash::SHELF_ALIGNMENT_TOP;
  return ash::SHELF_ALIGNMENT_BOTTOM;
}

// If prefs have synced and no user-set value exists at |local_path|, the value
// from |synced_path| is copied to |local_path|.
void MaybePropagatePrefToLocal(
    syncable_prefs::PrefServiceSyncable* pref_service,
    const char* local_path,
    const char* synced_path) {
  if (!pref_service->FindPreference(local_path)->HasUserSetting() &&
      pref_service->IsSyncing()) {
    // First time the user is using this machine, propagate from remote to
    // local.
    pref_service->SetString(local_path, pref_service->GetString(synced_path));
  }
}

std::string GetSourceFromAppListSource(ash::LaunchSource source) {
  switch (source) {
    case ash::LAUNCH_FROM_APP_LIST:
      return std::string(extension_urls::kLaunchSourceAppList);
    case ash::LAUNCH_FROM_APP_LIST_SEARCH:
      return std::string(extension_urls::kLaunchSourceAppListSearch);
    default: return std::string();
  }
}

/*
 * Return whether an app is pinned only by user.
 * This function doesn't expect an app_id neither pinned by user nor by
 * policy, the app_id in the arguments list MUST be pinned by either of
 * those. Invalid input may lead to unexpected result.
 * If this app is pinned by policy, but not by user, false is returned.
 * If this app is pinned by both policy and user, false is returned.
 * If this app is pinned not by policy, but by user, true is returned.
 */
bool IsAppForUserPinned(const std::string& app_id,
                        const base::ListValue* pinned_apps_pref,
                        const base::ListValue* policy_pinned_apps_pref) {
  for (size_t index = 0; index < pinned_apps_pref->GetSize(); ++index) {
    const base::DictionaryValue* app;
    if (pinned_apps_pref->GetDictionary(index, &app)) {
      std::string current_app_id;
      bool pinned_by_policy = false;
      if (app->GetString(ash::kPinnedAppsPrefAppIDPath, &current_app_id)) {
        if (app_id == current_app_id) {
          if (app->GetBoolean(ash::kPinnedAppsPrefPinnedByPolicy,
                              &pinned_by_policy) &&
              pinned_by_policy) {
            // Pinned by policy in the past or present.
            // Need to check policy_pinned_apps to determine
            break;
          } else {
            // User Preference Already Pinned
            return true;
          }
        }
      }
    }
  }
  for (size_t index = 0; index < policy_pinned_apps_pref->GetSize(); ++index) {
    const base::DictionaryValue* app;
    if (policy_pinned_apps_pref->GetDictionary(index, &app)) {
      std::string app_id_;
      if (app->GetString(ash::kPinnedAppsPrefAppIDPath, &app_id_)) {
        // Only pinned by policy, which is not part of user-pinned
        if (app_id == app_id_)
          return false;
      }
    }
  }
  // Default, user added new pins
  return true;
}

void update_pinned_apps(const std::string& app_id,
                        bool valid_for_current_user,
                        std::vector<std::string>* pinned_apps,
                        bool* chrome_icon_added,
                        bool* app_list_icon_added) {
  if (app_id == extension_misc::kChromeAppId) {
    *chrome_icon_added = true;
    pinned_apps->push_back(extension_misc::kChromeAppId);
  } else if (app_id == kAppShelfIdPlaceholder) {
    *app_list_icon_added = true;
    pinned_apps->push_back(kAppShelfIdPlaceholder);
  } else if (valid_for_current_user) {
    // Note: In multi profile scenarios we only want to show pinnable apps
    // here which is correct. Running applications from the other users will
    // continue to run. So no need for multi profile modifications.
    pinned_apps->push_back(app_id);
  }
}

}  // namespace

#if defined(OS_CHROMEOS)
// A class to get events from ChromeOS when a user gets changed or added.
class ChromeLauncherControllerUserSwitchObserver
    : public user_manager::UserManager::UserSessionStateObserver {
 public:
  ChromeLauncherControllerUserSwitchObserver(
      ChromeLauncherController* controller)
      : controller_(controller) {
    DCHECK(user_manager::UserManager::IsInitialized());
    user_manager::UserManager::Get()->AddSessionStateObserver(this);
  }
  ~ChromeLauncherControllerUserSwitchObserver() override {
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  }

  // user_manager::UserManager::UserSessionStateObserver overrides:
  void UserAddedToSession(const user_manager::User* added_user) override;

  // ChromeLauncherControllerUserSwitchObserver:
  void OnUserProfileReadyToSwitch(Profile* profile);

 private:
  // Add a user to the session.
  void AddUser(Profile* profile);

  // The owning ChromeLauncherController.
  ChromeLauncherController* controller_;

  // Users which were just added to the system, but which profiles were not yet
  // (fully) loaded.
  std::set<std::string> added_user_ids_waiting_for_profiles_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerUserSwitchObserver);
};

void ChromeLauncherControllerUserSwitchObserver::UserAddedToSession(
    const user_manager::User* active_user) {
  Profile* profile =
      multi_user_util::GetProfileFromAccountId(active_user->GetAccountId());
  // If we do not have a profile yet, we postpone forwarding the notification
  // until it is loaded.
  if (!profile)
    added_user_ids_waiting_for_profiles_.insert(active_user->email());
  else
    AddUser(profile);
}

void ChromeLauncherControllerUserSwitchObserver::OnUserProfileReadyToSwitch(
    Profile* profile) {
  if (!added_user_ids_waiting_for_profiles_.empty()) {
    // Check if the profile is from a user which was on the waiting list.
    std::string user_id =
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();
    std::set<std::string>::iterator it = std::find(
        added_user_ids_waiting_for_profiles_.begin(),
        added_user_ids_waiting_for_profiles_.end(),
        user_id);
    if (it != added_user_ids_waiting_for_profiles_.end()) {
      added_user_ids_waiting_for_profiles_.erase(it);
      AddUser(profile->GetOriginalProfile());
    }
  }
}

void ChromeLauncherControllerUserSwitchObserver::AddUser(Profile* profile) {
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() ==
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED)
    chrome::MultiUserWindowManager::GetInstance()->AddUser(profile);
  controller_->AdditionalUserAddedToSession(profile->GetOriginalProfile());
}
#endif

ChromeLauncherController::ChromeLauncherController(Profile* profile,
                                                   ash::ShelfModel* model)
    : model_(model),
      item_delegate_manager_(NULL),
      profile_(profile),
      app_sync_ui_state_(NULL),
      ignore_persist_pinned_state_change_(false) {
  if (!profile_) {
    // If no profile was passed, we take the currently active profile and use it
    // as the owner of the current desktop.
    // Use the original profile as on chromeos we may get a temporary off the
    // record profile, unless in guest session (where off the record profile is
    // the right one).
    profile_ = ProfileManager::GetActiveUserProfile();
    if (!profile_->IsGuestSession() && !profile_->IsSystemProfile())
      profile_ = profile_->GetOriginalProfile();

    app_sync_ui_state_ = AppSyncUIState::Get(profile_);
    if (app_sync_ui_state_)
      app_sync_ui_state_->AddObserver(this);
  }

  // All profile relevant settings get bound to the current profile.
  AttachProfile(profile_);
  model_->AddObserver(this);

  // In multi profile mode we might have a window manager. We try to create it
  // here. If the instantiation fails, the manager is not needed.
  chrome::MultiUserWindowManager::CreateInstance();

#if defined(OS_CHROMEOS)
  // On Chrome OS using multi profile we want to switch the content of the shelf
  // with a user change. Note that for unit tests the instance can be NULL.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() !=
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_OFF) {
    user_switch_observer_.reset(
        new ChromeLauncherControllerUserSwitchObserver(this));
  }

  // Create our v1/v2 application / browser monitors which will inform the
  // launcher of status changes.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() ==
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED) {
    // If running in separated destkop mode, we create the multi profile version
    // of status monitor.
    browser_status_monitor_.reset(new MultiProfileBrowserStatusMonitor(this));
    app_window_controller_.reset(
        new MultiProfileAppWindowLauncherController(this));
  } else {
    // Create our v1/v2 application / browser monitors which will inform the
    // launcher of status changes.
    browser_status_monitor_.reset(new BrowserStatusMonitor(this));
    app_window_controller_.reset(new AppWindowLauncherController(this));
  }
#else
  // Create our v1/v2 application / browser monitors which will inform the
  // launcher of status changes.
  browser_status_monitor_.reset(new BrowserStatusMonitor(this));
  app_window_controller_.reset(new AppWindowLauncherController(this));
#endif

  // Right now ash::Shell isn't created for tests.
  // TODO(mukai): Allows it to observe display change and write tests.
  if (ash::Shell::HasInstance()) {
    ash::Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
    // If it got already set, we remove the observer first again and swap the
    // ItemDelegateManager.
    if (item_delegate_manager_)
      item_delegate_manager_->RemoveObserver(this);
    item_delegate_manager_ =
        ash::Shell::GetInstance()->shelf_item_delegate_manager();
    item_delegate_manager_->AddObserver(this);
  }
}

ChromeLauncherController::~ChromeLauncherController() {
  if (item_delegate_manager_)
    item_delegate_manager_->RemoveObserver(this);

  // Reset the BrowserStatusMonitor as it has a weak pointer to this.
  browser_status_monitor_.reset();

  // Reset the app window controller here since it has a weak pointer to this.
  app_window_controller_.reset();

  for (std::set<ash::Shelf*>::iterator iter = shelves_.begin();
       iter != shelves_.end();
       ++iter)
    (*iter)->shelf_widget()->shelf_layout_manager()->RemoveObserver(this);

  model_->RemoveObserver(this);
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);
  for (IDToItemControllerMap::iterator i = id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    int index = model_->ItemIndexByID(i->first);
    // A "browser proxy" is not known to the model and this removal does
    // therefore not need to be propagated to the model.
    if (index != -1 &&
        model_->items()[index].type != ash::TYPE_BROWSER_SHORTCUT)
      model_->RemoveItemAt(index);
  }

  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->RemoveShellObserver(this);

  // Release all profile dependent resources.
  ReleaseProfile();
  if (instance_ == this)
    instance_ = NULL;

  // Get rid of the multi user window manager instance.
  chrome::MultiUserWindowManager::DeleteInstance();
}

// static
ChromeLauncherController* ChromeLauncherController::CreateInstance(
    Profile* profile,
    ash::ShelfModel* model) {
  // We do not check here for re-creation of the ChromeLauncherController since
  // it appears that it might be intentional that the ChromeLauncherController
  // can be re-created.
  instance_ = new ChromeLauncherController(profile, model);
  return instance_;
}

void ChromeLauncherController::Init() {
  CreateBrowserShortcutLauncherItem();
  UpdateAppLaunchersFromPref();

  // TODO(sky): update unit test so that this test isn't necessary.
  if (ash::Shell::HasInstance()) {
    SetShelfAutoHideBehaviorFromPrefs();
    SetShelfAlignmentFromPrefs();
#if defined(OS_CHROMEOS)
    SetVirtualKeyboardBehaviorFromPrefs();
#endif  // defined(OS_CHROMEOS)
    syncable_prefs::PrefServiceSyncable* prefs =
        PrefServiceSyncableFromProfile(profile_);
    if (!prefs->FindPreference(prefs::kShelfAlignmentLocal)->HasUserSetting() ||
        !prefs->FindPreference(prefs::kShelfAutoHideBehaviorLocal)->
            HasUserSetting()) {
      // This causes OnIsSyncingChanged to be called when the value of
      // PrefService::IsSyncing() changes.
      prefs->AddObserver(this);
    }
    ash::Shell::GetInstance()->AddShellObserver(this);
  }
}

ash::ShelfID ChromeLauncherController::CreateAppLauncherItem(
    LauncherItemController* controller,
    const std::string& app_id,
    ash::ShelfItemStatus status) {
  CHECK(controller);
  int index = 0;
  // Panels are inserted on the left so as not to push all existing panels over.
  if (controller->GetShelfItemType() != ash::TYPE_APP_PANEL)
    index = model_->item_count();
  return InsertAppLauncherItem(controller,
                               app_id,
                               status,
                               index,
                               controller->GetShelfItemType());
}

void ChromeLauncherController::SetItemStatus(ash::ShelfID id,
                                             ash::ShelfItemStatus status) {
  int index = model_->ItemIndexByID(id);
  ash::ShelfItemStatus old_status = model_->items()[index].status;
  // Since ordinary browser windows are not registered, we might get a negative
  // index here.
  if (index >= 0 && old_status != status) {
    ash::ShelfItem item = model_->items()[index];
    item.status = status;
    model_->Set(index, item);
  }
}

void ChromeLauncherController::SetItemController(
    ash::ShelfID id,
    LauncherItemController* controller) {
  CHECK(controller);
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  CHECK(iter != id_to_item_controller_map_.end());
  controller->set_shelf_id(id);
  iter->second = controller;
  // Existing controller is destroyed and replaced by registering again.
  SetShelfItemDelegate(id, controller);
}

void ChromeLauncherController::CloseLauncherItem(ash::ShelfID id) {
  CHECK(id);
  if (IsPinned(id)) {
    // Create a new shortcut controller.
    IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
    CHECK(iter != id_to_item_controller_map_.end());
    SetItemStatus(id, ash::STATUS_CLOSED);
    std::string app_id = iter->second->app_id();
    iter->second = new AppShortcutLauncherItemController(app_id, this);
    iter->second->set_shelf_id(id);
    // Existing controller is destroyed and replaced by registering again.
    SetShelfItemDelegate(id, iter->second);
  } else {
    LauncherItemClosed(id);
  }
}

bool ChromeLauncherController::CanPin(const std::string& app_id) {
  const base::ListValue* pref =
      profile_->GetPrefs()->GetList(prefs::kPolicyPinnedLauncherApps);
  if (!pref)
    return true;
  for (size_t index = 0; index < pref->GetSize(); ++index) {
    const base::DictionaryValue* app = nullptr;
    std::string the_app_id;
    if (pref->GetDictionary(index, &app) &&
        app->GetString(ash::kPinnedAppsPrefAppIDPath, &the_app_id) &&
        app_id == the_app_id)
      return false;
  }
  return true;
}

void ChromeLauncherController::Pin(ash::ShelfID id) {
  DCHECK(HasShelfIDToAppIDMapping(id));

  int index = model_->ItemIndexByID(id);
  DCHECK_GE(index, 0);

  ash::ShelfItem item = model_->items()[index];

  if (item.type == ash::TYPE_PLATFORM_APP ||
      item.type == ash::TYPE_WINDOWED_APP) {
    item.type = ash::TYPE_APP_SHORTCUT;
    model_->Set(index, item);
  } else if (item.type != ash::TYPE_APP_SHORTCUT) {
    return;
  }

  if (GetLauncherItemController(id)->CanPin())
    PersistPinnedState();
}

void ChromeLauncherController::Unpin(ash::ShelfID id) {
  LauncherItemController* controller = GetLauncherItemController(id);
  CHECK(controller);
  bool can_pin = controller->CanPin();

  if (controller->type() == LauncherItemController::TYPE_APP ||
      controller->locked()) {
    UnpinRunningAppInternal(model_->ItemIndexByID(id));
  } else {
    LauncherItemClosed(id);
  }
  if (can_pin)
    PersistPinnedState();
}

bool ChromeLauncherController::IsPinned(ash::ShelfID id) {
  int index = model_->ItemIndexByID(id);
  if (index < 0)
    return false;
  ash::ShelfItemType type = model_->items()[index].type;
  return (type == ash::TYPE_APP_SHORTCUT || type == ash::TYPE_BROWSER_SHORTCUT);
}

void ChromeLauncherController::TogglePinned(ash::ShelfID id) {
  if (!HasShelfIDToAppIDMapping(id))
    return;  // May happen if item closed with menu open.

  if (IsPinned(id))
    Unpin(id);
  else
    Pin(id);
}

bool ChromeLauncherController::IsPinnable(ash::ShelfID id) const {
  int index = model_->ItemIndexByID(id);
  if (index == -1)
    return false;

  ash::ShelfItemType type = model_->items()[index].type;
  std::string app_id;
  return ((type == ash::TYPE_APP_SHORTCUT || type == ash::TYPE_PLATFORM_APP ||
           type == ash::TYPE_WINDOWED_APP) &&
          item_delegate_manager_->GetShelfItemDelegate(id)->CanPin());
}

void ChromeLauncherController::LockV1AppWithID(const std::string& app_id) {
  ash::ShelfID id = GetShelfIDForAppID(app_id);
  if (!IsPinned(id) && !IsWindowedAppInLauncher(app_id)) {
    CreateAppShortcutLauncherItemWithType(app_id,
                                          model_->item_count(),
                                          ash::TYPE_WINDOWED_APP);
    id = GetShelfIDForAppID(app_id);
  }
  CHECK(id);
  id_to_item_controller_map_[id]->lock();
}

void ChromeLauncherController::UnlockV1AppWithID(const std::string& app_id) {
  ash::ShelfID id = GetShelfIDForAppID(app_id);
  CHECK(id);
  CHECK(IsPinned(id) || IsWindowedAppInLauncher(app_id));
  LauncherItemController* controller = id_to_item_controller_map_[id];
  controller->unlock();
  if (!controller->locked() && !IsPinned(id))
    CloseLauncherItem(id);
}

void ChromeLauncherController::Launch(ash::ShelfID id, int event_flags) {
  LauncherItemController* controller = GetLauncherItemController(id);
  if (!controller)
    return;  // In case invoked from menu and item closed while menu up.
  controller->Launch(ash::LAUNCH_FROM_UNKNOWN, event_flags);
}

void ChromeLauncherController::Close(ash::ShelfID id) {
  LauncherItemController* controller = GetLauncherItemController(id);
  if (!controller)
    return;  // May happen if menu closed.
  controller->Close();
}

bool ChromeLauncherController::IsOpen(ash::ShelfID id) {
  LauncherItemController* controller = GetLauncherItemController(id);
  if (!controller)
    return false;
  return controller->IsOpen();
}

bool ChromeLauncherController::IsPlatformApp(ash::ShelfID id) {
  if (!HasShelfIDToAppIDMapping(id))
    return false;

  std::string app_id = GetAppIDForShelfID(id);
  const Extension* extension = GetExtensionForAppID(app_id);
  // An extension can be synced / updated at any time and therefore not be
  // available.
  return extension ? extension->is_platform_app() : false;
}

void ChromeLauncherController::LaunchApp(const std::string& app_id,
                                         ash::LaunchSource source,
                                         int event_flags) {
  // |extension| could be NULL when it is being unloaded for updating.
  const Extension* extension = GetExtensionForAppID(app_id);
  if (!extension)
    return;

  if (!extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile_)) {
    // Do nothing if there is already a running enable flow.
    if (extension_enable_flow_)
      return;

    extension_enable_flow_.reset(
        new ExtensionEnableFlow(profile_, app_id, this));
    extension_enable_flow_->StartForNativeWindow(NULL);
    return;
  }

  // The app will be created for the currently active profile.
  AppLaunchParams params(
      profile_, extension, ui::DispositionFromEventFlags(event_flags),
      chrome::HOST_DESKTOP_TYPE_ASH, extensions::SOURCE_APP_LAUNCHER);
  if (source != ash::LAUNCH_FROM_UNKNOWN &&
      app_id == extensions::kWebStoreAppId) {
    // Get the corresponding source string.
    std::string source_value = GetSourceFromAppListSource(source);

    // Set an override URL to include the source.
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url, extension_urls::kWebstoreSourceField, source_value);
  }

  OpenApplication(params);
}

void ChromeLauncherController::ActivateApp(const std::string& app_id,
                                           ash::LaunchSource source,
                                           int event_flags) {
  // If there is an existing non-shortcut controller for this app, open it.
  ash::ShelfID id = GetShelfIDForAppID(app_id);
  if (id) {
    LauncherItemController* controller = GetLauncherItemController(id);
    controller->Activate(source);
    return;
  }

  // Create a temporary application launcher item and use it to see if there are
  // running instances.
  scoped_ptr<AppShortcutLauncherItemController> app_controller(
      new AppShortcutLauncherItemController(app_id, this));
  if (!app_controller->GetRunningApplications().empty())
    app_controller->Activate(source);
  else
    LaunchApp(app_id, source, event_flags);
}

extensions::LaunchType ChromeLauncherController::GetLaunchType(
    ash::ShelfID id) {
  const Extension* extension = GetExtensionForAppID(GetAppIDForShelfID(id));

  // An extension can be unloaded/updated/unavailable at any time.
  if (!extension)
    return extensions::LAUNCH_TYPE_DEFAULT;

  return extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile_),
                                   extension);
}

ash::ShelfID ChromeLauncherController::GetShelfIDForAppID(
    const std::string& app_id) {
  for (IDToItemControllerMap::const_iterator i =
           id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    if (i->second->type() == LauncherItemController::TYPE_APP_PANEL)
      continue;  // Don't include panels
    if (i->second->app_id() == app_id)
      return i->first;
  }
  return 0;
}

bool ChromeLauncherController::HasShelfIDToAppIDMapping(ash::ShelfID id) const {
  return id_to_item_controller_map_.find(id) !=
         id_to_item_controller_map_.end();
}

const std::string& ChromeLauncherController::GetAppIDForShelfID(
    ash::ShelfID id) {
  LauncherItemController* controller = GetLauncherItemController(id);
  CHECK(controller);
  return controller->app_id();
}

void ChromeLauncherController::SetAppImage(const std::string& id,
                                           const gfx::ImageSkia& image) {
  // TODO: need to get this working for shortcuts.
  for (IDToItemControllerMap::const_iterator i =
           id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    LauncherItemController* controller = i->second;
    if (controller->app_id() != id)
      continue;
    if (controller->image_set_by_controller())
      continue;
    int index = model_->ItemIndexByID(i->first);
    if (index == -1)
      continue;
    ash::ShelfItem item = model_->items()[index];
    item.image = image;
    model_->Set(index, item);
    // It's possible we're waiting on more than one item, so don't break.
  }
}

void ChromeLauncherController::OnAutoHideBehaviorChanged(
    aura::Window* root_window,
    ash::ShelfAutoHideBehavior new_behavior) {
  SetShelfAutoHideBehaviorPrefs(new_behavior, root_window);
}

void ChromeLauncherController::SetLauncherItemImage(
    ash::ShelfID shelf_id,
    const gfx::ImageSkia& image) {
  int index = model_->ItemIndexByID(shelf_id);
  if (index == -1)
    return;
  ash::ShelfItem item = model_->items()[index];
  item.image = image;
  model_->Set(index, item);
}

bool ChromeLauncherController::IsAppPinned(const std::string& app_id) {
  for (IDToItemControllerMap::const_iterator i =
           id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    if (IsPinned(i->first) && i->second->app_id() == app_id)
      return true;
  }
  return false;
}

bool ChromeLauncherController::IsWindowedAppInLauncher(
    const std::string& app_id) {
  int index = model_->ItemIndexByID(GetShelfIDForAppID(app_id));
  if (index < 0)
    return false;

  ash::ShelfItemType type = model_->items()[index].type;
  return type == ash::TYPE_WINDOWED_APP;
}

void ChromeLauncherController::PinAppWithID(const std::string& app_id) {
  if (CanPin(app_id))
    DoPinAppWithID(app_id);
  else
    NOTREACHED();
}

void ChromeLauncherController::SetLaunchType(
    ash::ShelfID id,
    extensions::LaunchType launch_type) {
  LauncherItemController* controller = GetLauncherItemController(id);
  if (!controller)
    return;

  extensions::SetLaunchType(profile_, controller->app_id(), launch_type);
}

void ChromeLauncherController::UnpinAppWithID(const std::string& app_id) {
  if (CanPin(app_id))
    DoUnpinAppWithID(app_id);
  else
    NOTREACHED();
}

void ChromeLauncherController::OnSetShelfItemDelegate(
    ash::ShelfID id,
    ash::ShelfItemDelegate* item_delegate) {
  // TODO(skuhne): This fixes crbug.com/429870, but it does not answer why we
  // get into this state in the first place.
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  if (iter == id_to_item_controller_map_.end() || item_delegate == iter->second)
    return;
  LOG(ERROR) << "Unexpected change of shelf item id: " << id;
  id_to_item_controller_map_.erase(iter);
}

bool ChromeLauncherController::IsLoggedInAsGuest() {
  return profile_->IsGuestSession();
}

void ChromeLauncherController::CreateNewWindow() {
  // Use the currently active user.
  chrome::NewEmptyWindow(profile_, chrome::HOST_DESKTOP_TYPE_ASH);
}

void ChromeLauncherController::CreateNewIncognitoWindow() {
  // Use the currently active user.
  chrome::NewEmptyWindow(profile_->GetOffTheRecordProfile(),
                         chrome::HOST_DESKTOP_TYPE_ASH);
}

void ChromeLauncherController::PersistPinnedState() {
  if (ignore_persist_pinned_state_change_)
    return;
  // It is a coding error to call PersistPinnedState() if the pinned apps are
  // not user-editable. The code should check earlier and not perform any
  // modification actions that trigger persisting the state.
  // Mutating kPinnedLauncherApps is going to notify us and trigger us to
  // process the change. We don't want that to happen so remove ourselves as a
  // listener.
  pref_change_registrar_.Remove(prefs::kPinnedLauncherApps);
  {
    scoped_ptr<const base::ListValue> pinned_apps_pref =
        profile_->GetPrefs()
            ->GetList(prefs::kPinnedLauncherApps)
            ->CreateDeepCopy();

    const base::ListValue* policy_pinned_apps_pref =
        profile_->GetPrefs()->GetList(prefs::kPolicyPinnedLauncherApps);

    ListPrefUpdate updater(profile_->GetPrefs(), prefs::kPinnedLauncherApps);
    updater->Clear();
    for (size_t i = 0; i < model_->items().size(); ++i) {
      if (model_->items()[i].type == ash::TYPE_APP_SHORTCUT) {
        ash::ShelfID id = model_->items()[i].id;
        LauncherItemController* controller = GetLauncherItemController(id);
        if (controller && IsPinned(id)) {
          base::DictionaryValue* app_value = ash::CreateAppDict(
              controller->app_id());
          if (app_value) {
            if (!IsAppForUserPinned(controller->app_id(),
                                    pinned_apps_pref.get(),
                                    policy_pinned_apps_pref))
              app_value->SetBoolean(ash::kPinnedAppsPrefPinnedByPolicy, true);
            updater->Append(app_value);
          }
        }
      } else if (model_->items()[i].type == ash::TYPE_BROWSER_SHORTCUT) {
        PersistChromeItemIndex(i);
      } else if (model_->items()[i].type == ash::TYPE_APP_LIST) {
        base::DictionaryValue* app_value = ash::CreateAppDict(
            kAppShelfIdPlaceholder);
        if (app_value)
          updater->Append(app_value);
      }
    }
  }
  pref_change_registrar_.Add(
      prefs::kPinnedLauncherApps,
      base::Bind(&ChromeLauncherController::UpdateAppLaunchersFromPref,
                 base::Unretained(this)));
}

ash::ShelfModel* ChromeLauncherController::model() {
  return model_;
}

Profile* ChromeLauncherController::profile() {
  return profile_;
}

ash::ShelfAutoHideBehavior ChromeLauncherController::GetShelfAutoHideBehavior(
    aura::Window* root_window) const {
  return GetShelfAutoHideBehaviorFromPrefs(profile_, root_window);
}

bool ChromeLauncherController::CanUserModifyShelfAutoHideBehavior(
    aura::Window* root_window) const {
#if defined(OS_WIN)
  // Disable shelf auto-hide behavior on screen sides in Metro mode.
  if (ash::Shell::GetInstance()->GetShelfAlignment(root_window) !=
      ash::SHELF_ALIGNMENT_BOTTOM) {
    return false;
  }
#endif
  return profile_->GetPrefs()->
      FindPreference(prefs::kShelfAutoHideBehaviorLocal)->IsUserModifiable();
}

void ChromeLauncherController::ToggleShelfAutoHideBehavior(
    aura::Window* root_window) {
  ash::ShelfAutoHideBehavior behavior = GetShelfAutoHideBehavior(root_window) ==
      ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS ?
          ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER :
          ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  SetShelfAutoHideBehaviorPrefs(behavior, root_window);
  return;
}

void ChromeLauncherController::UpdateAppState(content::WebContents* contents,
                                              AppState app_state) {
  std::string app_id = app_tab_helper_->GetAppID(contents);

  // Check if the gMail app is loaded and it matches the given content.
  // This special treatment is needed to address crbug.com/234268.
  if (app_id.empty() && ContentCanBeHandledByGmailApp(contents))
    app_id = kGmailAppId;

  // Check the old |app_id| for a tab. If the contents has changed we need to
  // remove it from the previous app.
  if (web_contents_to_app_id_.find(contents) != web_contents_to_app_id_.end()) {
    std::string last_app_id = web_contents_to_app_id_[contents];
    if (last_app_id != app_id) {
      ash::ShelfID id = GetShelfIDForAppID(last_app_id);
      if (id) {
        // Since GetAppState() will use |web_contents_to_app_id_| we remove
        // the connection before calling it.
        web_contents_to_app_id_.erase(contents);
        SetItemStatus(id, GetAppState(last_app_id));
      }
    }
  }

  if (app_state == APP_STATE_REMOVED)
    web_contents_to_app_id_.erase(contents);
  else
    web_contents_to_app_id_[contents] = app_id;

  ash::ShelfID id = GetShelfIDForAppID(app_id);
  if (id) {
    SetItemStatus(id, (app_state == APP_STATE_WINDOW_ACTIVE ||
                       app_state == APP_STATE_ACTIVE) ? ash::STATUS_ACTIVE :
                                                        GetAppState(app_id));
  }
}

ash::ShelfID ChromeLauncherController::GetShelfIDForWebContents(
    content::WebContents* contents) {
  DCHECK(contents);

  std::string app_id = app_tab_helper_->GetAppID(contents);

  if (app_id.empty() && ContentCanBeHandledByGmailApp(contents))
    app_id = kGmailAppId;

  ash::ShelfID id = GetShelfIDForAppID(app_id);

  if (app_id.empty() || !id) {
    int browser_index = model_->GetItemIndexForType(ash::TYPE_BROWSER_SHORTCUT);
    return model_->items()[browser_index].id;
  }

  return id;
}

void ChromeLauncherController::SetRefocusURLPatternForTest(ash::ShelfID id,
                                                           const GURL& url) {
  LauncherItemController* controller = GetLauncherItemController(id);
  DCHECK(controller);

  int index = model_->ItemIndexByID(id);
  if (index == -1) {
    NOTREACHED() << "Invalid launcher id";
    return;
  }

  ash::ShelfItemType type = model_->items()[index].type;
  if (type == ash::TYPE_APP_SHORTCUT || type == ash::TYPE_WINDOWED_APP) {
    AppShortcutLauncherItemController* app_controller =
        static_cast<AppShortcutLauncherItemController*>(controller);
    app_controller->set_refocus_url(url);
  } else {
    NOTREACHED() << "Invalid launcher type";
  }
}

const Extension* ChromeLauncherController::GetExtensionForAppID(
    const std::string& app_id) const {
  return extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING);
}

ash::ShelfItemDelegate::PerformedAction
ChromeLauncherController::ActivateWindowOrMinimizeIfActive(
    ui::BaseWindow* window,
    bool allow_minimize) {
  // In separated desktop mode we might have to teleport a window back to the
  // current user.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() ==
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED) {
    aura::Window* native_window = window->GetNativeWindow();
    const AccountId& current_account_id =
        multi_user_util::GetAccountIdFromProfile(profile());
    chrome::MultiUserWindowManager* manager =
        chrome::MultiUserWindowManager::GetInstance();
    if (!manager->IsWindowOnDesktopOfUser(native_window, current_account_id)) {
      ash::MultiProfileUMA::RecordTeleportAction(
          ash::MultiProfileUMA::TELEPORT_WINDOW_RETURN_BY_LAUNCHER);
      manager->ShowWindowForUser(native_window, current_account_id);
      window->Activate();
      return ash::ShelfItemDelegate::kExistingWindowActivated;
    }
  }

  if (window->IsActive() && allow_minimize) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableMinimizeOnSecondLauncherItemClick)) {
      AnimateWindow(window->GetNativeWindow(),
                    wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    } else {
      window->Minimize();
      return ash::ShelfItemDelegate::kExistingWindowMinimized;
    }
  } else {
    window->Show();
    window->Activate();
    return ash::ShelfItemDelegate::kExistingWindowActivated;
  }
  return ash::ShelfItemDelegate::kNoAction;
}

void ChromeLauncherController::OnShelfCreated(ash::Shelf* shelf) {
  shelves_.insert(shelf);
  shelf->shelf_widget()->shelf_layout_manager()->AddObserver(this);
}

void ChromeLauncherController::OnShelfDestroyed(ash::Shelf* shelf) {
  shelves_.erase(shelf);
  // RemoveObserver is not called here, since by the time this method is called
  // Shelf is already in its destructor.
}

void ChromeLauncherController::ShelfItemAdded(int index) {
  // The app list launcher can get added to the shelf after we applied the
  // preferences. In that case the item might be at the wrong spot. As such we
  // call the function again.
  if (model_->items()[index].type == ash::TYPE_APP_LIST)
    UpdateAppLaunchersFromPref();
}

void ChromeLauncherController::ShelfItemRemoved(int index, ash::ShelfID id) {
  // TODO(skuhne): This fixes crbug.com/429870, but it does not answer why we
  // get into this state in the first place.
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  if (iter == id_to_item_controller_map_.end())
    return;

  LOG(ERROR) << "Unexpected change of shelf item id: " << id;

  id_to_item_controller_map_.erase(iter);
}

void ChromeLauncherController::ShelfItemMoved(int start_index,
                                              int target_index) {
  const ash::ShelfItem& item = model_->items()[target_index];
  // We remember the moved item position if it is either pinnable or
  // it is the app list with the alternate shelf layout.
  if ((HasShelfIDToAppIDMapping(item.id) && IsPinned(item.id)) ||
      item.type == ash::TYPE_APP_LIST)
    PersistPinnedState();
}

void ChromeLauncherController::ShelfItemChanged(
    int index,
    const ash::ShelfItem& old_item) {
}

void ChromeLauncherController::ShelfStatusChanged() {
}

void ChromeLauncherController::ActiveUserChanged(
    const std::string& user_email) {
  // Store the order of running applications for the user which gets inactive.
  RememberUnpinnedRunningApplicationOrder();
  // Coming here the default profile is already switched. All profile specific
  // resources get released and the new profile gets attached instead.
  ReleaseProfile();
  // When coming here, the active user has already be changed so that we can
  // set it as active.
  AttachProfile(ProfileManager::GetActiveUserProfile());
  // Update the V1 applications.
  browser_status_monitor_->ActiveUserChanged(user_email);
  // Switch the running applications to the new user.
  app_window_controller_->ActiveUserChanged(user_email);
  // Update the user specific shell properties from the new user profile.
  UpdateAppLaunchersFromPref();
  SetShelfAlignmentFromPrefs();
  SetShelfAutoHideBehaviorFromPrefs();
  SetShelfBehaviorsFromPrefs();
#if defined(OS_CHROMEOS)
  SetVirtualKeyboardBehaviorFromPrefs();
#endif  // defined(OS_CHROMEOS)
  // Restore the order of running, but unpinned applications for the activated
  // user.
  RestoreUnpinnedRunningApplicationOrder(user_email);
  // Inform the system tray of the change.
  ash::Shell::GetInstance()->system_tray_delegate()->ActiveUserWasChanged();
  // Force on-screen keyboard to reset.
  if (keyboard::IsKeyboardEnabled())
    ash::Shell::GetInstance()->CreateKeyboard();
}

void ChromeLauncherController::AdditionalUserAddedToSession(Profile* profile) {
  // Switch the running applications to the new user.
  app_window_controller_->AdditionalUserAddedToSession(profile);
}

void ChromeLauncherController::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (IsAppPinned(extension->id())) {
    // Clear and re-fetch to ensure icon is up-to-date.
    app_icon_loader_->ClearImage(extension->id());
    app_icon_loader_->FetchImage(extension->id());
  }

  UpdateAppLaunchersFromPref();
}

void ChromeLauncherController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  const std::string& id = extension->id();
  const Profile* profile = Profile::FromBrowserContext(browser_context);

  // Since we might have windowed apps of this type which might have
  // outstanding locks which needs to be removed.
  if (GetShelfIDForAppID(id) &&
      reason == UnloadedExtensionInfo::REASON_UNINSTALL) {
    CloseWindowedAppsFromRemovedExtension(id, profile);
  }

  if (IsAppPinned(id)) {
    if (reason == UnloadedExtensionInfo::REASON_UNINSTALL) {
      if (profile == profile_) {
        DoUnpinAppWithID(id);
      }
      app_icon_loader_->ClearImage(id);
    } else {
      app_icon_loader_->UpdateImage(id);
    }
  }
}

void ChromeLauncherController::OnShelfAlignmentChanged(
    aura::Window* root_window) {
  const char* pref_value = NULL;
  switch (ash::Shell::GetInstance()->GetShelfAlignment(root_window)) {
    case ash::SHELF_ALIGNMENT_BOTTOM:
      pref_value = ash::kShelfAlignmentBottom;
      break;
    case ash::SHELF_ALIGNMENT_LEFT:
      pref_value = ash::kShelfAlignmentLeft;
      break;
    case ash::SHELF_ALIGNMENT_RIGHT:
      pref_value = ash::kShelfAlignmentRight;
      break;
    case ash::SHELF_ALIGNMENT_TOP:
      pref_value = ash::kShelfAlignmentTop;
  }

  UpdatePerDisplayPref(
      profile_->GetPrefs(), root_window, prefs::kShelfAlignment, pref_value);

  if (root_window == ash::Shell::GetPrimaryRootWindow()) {
    // See comment in |kShelfAlignment| about why we have two prefs here.
    profile_->GetPrefs()->SetString(prefs::kShelfAlignmentLocal, pref_value);
    profile_->GetPrefs()->SetString(prefs::kShelfAlignment, pref_value);
  }
}

void ChromeLauncherController::OnDisplayConfigurationChanged() {
  SetShelfBehaviorsFromPrefs();
}

void ChromeLauncherController::OnIsSyncingChanged() {
  syncable_prefs::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(profile_);
  MaybePropagatePrefToLocal(prefs,
                            prefs::kShelfAlignmentLocal,
                            prefs::kShelfAlignment);
  MaybePropagatePrefToLocal(prefs,
                            prefs::kShelfAutoHideBehaviorLocal,
                            prefs::kShelfAutoHideBehavior);
}

void ChromeLauncherController::OnAppSyncUIStatusChanged() {
  if (app_sync_ui_state_->status() == AppSyncUIState::STATUS_SYNCING)
    model_->SetStatus(ash::ShelfModel::STATUS_LOADING);
  else
    model_->SetStatus(ash::ShelfModel::STATUS_NORMAL);
}

void ChromeLauncherController::ExtensionEnableFlowFinished() {
  LaunchApp(extension_enable_flow_->extension_id(),
            ash::LAUNCH_FROM_UNKNOWN,
            ui::EF_NONE);
  extension_enable_flow_.reset();
}

void ChromeLauncherController::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
}

ChromeLauncherAppMenuItems ChromeLauncherController::GetApplicationList(
    const ash::ShelfItem& item,
    int event_flags) {
  // Make sure that there is a controller associated with the id and that the
  // extension itself is a valid application and not a panel.
  LauncherItemController* controller = GetLauncherItemController(item.id);
  if (!controller || !GetShelfIDForAppID(controller->app_id()))
    return ChromeLauncherAppMenuItems();

  return controller->GetApplicationList(event_flags);
}

std::vector<content::WebContents*>
ChromeLauncherController::GetV1ApplicationsFromAppId(
    const std::string& app_id) {
  ash::ShelfID id = GetShelfIDForAppID(app_id);

  // If there is no such an item pinned to the launcher, no menu gets created.
  if (id) {
    LauncherItemController* controller = GetLauncherItemController(id);
    DCHECK(controller);
    if (controller->type() == LauncherItemController::TYPE_SHORTCUT)
      return GetV1ApplicationsFromController(controller);
  }
  return std::vector<content::WebContents*>();
}

void ChromeLauncherController::ActivateShellApp(const std::string& app_id,
                                                int index) {
  ash::ShelfID id = GetShelfIDForAppID(app_id);
  if (id) {
    LauncherItemController* controller = GetLauncherItemController(id);
    if (controller && controller->type() == LauncherItemController::TYPE_APP) {
      AppWindowLauncherItemController* app_window_controller =
          static_cast<AppWindowLauncherItemController*>(controller);
      app_window_controller->ActivateIndexedApp(index);
    }
  }
}

bool ChromeLauncherController::IsWebContentHandledByApplication(
    content::WebContents* web_contents,
    const std::string& app_id) {
  if ((web_contents_to_app_id_.find(web_contents) !=
       web_contents_to_app_id_.end()) &&
      (web_contents_to_app_id_[web_contents] == app_id))
    return true;
  return (app_id == kGmailAppId && ContentCanBeHandledByGmailApp(web_contents));
}

bool ChromeLauncherController::ContentCanBeHandledByGmailApp(
    content::WebContents* web_contents) {
  ash::ShelfID id = GetShelfIDForAppID(kGmailAppId);
  if (id) {
    const GURL url = web_contents->GetURL();
    // We need to extend the application matching for the gMail app beyond the
    // manifest file's specification. This is required because of the namespace
    // overlap with the offline app ("/mail/mu/").
    if (!base::MatchPattern(url.path(), "/mail/mu/*") &&
        base::MatchPattern(url.path(), "/mail/*") &&
        GetExtensionForAppID(kGmailAppId) &&
        GetExtensionForAppID(kGmailAppId)->OverlapsWithOrigin(url))
      return true;
  }
  return false;
}

gfx::Image ChromeLauncherController::GetAppListIcon(
    content::WebContents* web_contents) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (IsIncognito(web_contents))
    return rb.GetImageNamed(IDR_ASH_SHELF_LIST_INCOGNITO_BROWSER);
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents);
  gfx::Image result = favicon_driver->GetFavicon();
  if (result.IsEmpty())
    return rb.GetImageNamed(IDR_DEFAULT_FAVICON);
  return result;
}

base::string16 ChromeLauncherController::GetAppListTitle(
    content::WebContents* web_contents) const {
  base::string16 title = web_contents->GetTitle();
  if (!title.empty())
    return title;
  WebContentsToAppIDMap::const_iterator iter =
      web_contents_to_app_id_.find(web_contents);
  if (iter != web_contents_to_app_id_.end()) {
    std::string app_id = iter->second;
    const extensions::Extension* extension = GetExtensionForAppID(app_id);
    if (extension)
      return base::UTF8ToUTF16(extension->name());
  }
  return l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
}

ash::ShelfID ChromeLauncherController::CreateAppShortcutLauncherItem(
    const std::string& app_id,
    int index) {
  return CreateAppShortcutLauncherItemWithType(app_id,
                                               index,
                                               ash::TYPE_APP_SHORTCUT);
}

void ChromeLauncherController::SetAppTabHelperForTest(AppTabHelper* helper) {
  app_tab_helper_.reset(helper);
}

void ChromeLauncherController::SetAppIconLoaderForTest(
    extensions::AppIconLoader* loader) {
  app_icon_loader_.reset(loader);
}

const std::string& ChromeLauncherController::GetAppIdFromShelfIdForTest(
    ash::ShelfID id) {
  return id_to_item_controller_map_[id]->app_id();
}

bool ChromeLauncherController::GetAppIDForShelfIDConst(
    ash::ShelfID id,
    std::string* app_id) const {
  auto app = id_to_item_controller_map_.find(id);
  if (app == id_to_item_controller_map_.end()) {
    return false;
  } else {
    *app_id = app->second->app_id();
    return true;
  }
}

void ChromeLauncherController::SetShelfItemDelegateManagerForTest(
    ash::ShelfItemDelegateManager* manager) {
  if (item_delegate_manager_)
    item_delegate_manager_->RemoveObserver(this);

  item_delegate_manager_ = manager;

  if (item_delegate_manager_)
    item_delegate_manager_->AddObserver(this);
}

void ChromeLauncherController::RememberUnpinnedRunningApplicationOrder() {
  RunningAppListIds list;
  for (int i = 0; i < model_->item_count(); i++) {
    ash::ShelfItemType type = model_->items()[i].type;
    if (type == ash::TYPE_WINDOWED_APP || type == ash::TYPE_PLATFORM_APP)
      list.push_back(GetAppIDForShelfID(model_->items()[i].id));
  }
  const std::string user_email =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();
  last_used_running_application_order_[user_email] = list;
}

void ChromeLauncherController::RestoreUnpinnedRunningApplicationOrder(
    const std::string& user_id) {
  const RunningAppListIdMap::iterator app_id_list =
      last_used_running_application_order_.find(user_id);
  if (app_id_list == last_used_running_application_order_.end())
    return;

  // Find the first insertion point for running applications.
  int running_index = model_->FirstRunningAppIndex();
  for (RunningAppListIds::iterator app_id = app_id_list->second.begin();
       app_id != app_id_list->second.end(); ++app_id) {
    ash::ShelfID shelf_id = GetShelfIDForAppID(*app_id);
    if (shelf_id) {
      int app_index = model_->ItemIndexByID(shelf_id);
      DCHECK_GE(app_index, 0);
      ash::ShelfItemType type = model_->items()[app_index].type;
      if (type == ash::TYPE_WINDOWED_APP || type == ash::TYPE_PLATFORM_APP) {
        if (running_index != app_index)
          model_->Move(running_index, app_index);
        running_index++;
      }
    }
  }
}

ash::ShelfID ChromeLauncherController::CreateAppShortcutLauncherItemWithType(
    const std::string& app_id,
    int index,
    ash::ShelfItemType shelf_item_type) {
  AppShortcutLauncherItemController* controller =
      new AppShortcutLauncherItemController(app_id, this);
  ash::ShelfID shelf_id = InsertAppLauncherItem(
      controller, app_id, ash::STATUS_CLOSED, index, shelf_item_type);
  return shelf_id;
}

LauncherItemController* ChromeLauncherController::GetLauncherItemController(
    const ash::ShelfID id) {
  if (!HasShelfIDToAppIDMapping(id))
    return NULL;
  return id_to_item_controller_map_[id];
}

bool ChromeLauncherController::IsBrowserFromActiveUser(Browser* browser) {
  // If running multi user mode with separate desktops, we have to check if the
  // browser is from the active user.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() !=
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED)
    return true;
  return multi_user_util::IsProfileFromActiveUser(browser->profile());
}

bool ChromeLauncherController::ShelfBoundsChangesProbablyWithUser(
    aura::Window* root_window,
    const std::string& user_id) const {
  Profile* other_profile = multi_user_util::GetProfileFromAccountId(
      AccountId::FromUserEmail(user_id));
  DCHECK_NE(other_profile, profile_);

  // Note: The Auto hide state from preferences is not the same as the actual
  // visibility of the shelf. Depending on all the various states (full screen,
  // no window on desktop, multi user, ..) the shelf could be shown - or not.
  bool currently_shown = ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER ==
      GetShelfAutoHideBehaviorFromPrefs(profile_, root_window);
  bool other_shown = ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER ==
      GetShelfAutoHideBehaviorFromPrefs(other_profile, root_window);

  return currently_shown != other_shown ||
         GetShelfAlignmentFromPrefs(profile_, root_window) !=
             GetShelfAlignmentFromPrefs(other_profile, root_window);
}

void ChromeLauncherController::OnUserProfileReadyToSwitch(Profile* profile) {
#if defined(OS_CHROMEOS)
  if (user_switch_observer_.get())
    user_switch_observer_->OnUserProfileReadyToSwitch(profile);
#endif
}

void ChromeLauncherController::LauncherItemClosed(ash::ShelfID id) {
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  CHECK(iter != id_to_item_controller_map_.end());
  CHECK(iter->second);
  app_icon_loader_->ClearImage(iter->second->app_id());
  id_to_item_controller_map_.erase(iter);
  int index = model_->ItemIndexByID(id);
  // A "browser proxy" is not known to the model and this removal does
  // therefore not need to be propagated to the model.
  if (index != -1)
    model_->RemoveItemAt(index);
}

void ChromeLauncherController::DoPinAppWithID(const std::string& app_id) {
  // If there is an item, do nothing and return.
  if (IsAppPinned(app_id))
    return;

  ash::ShelfID shelf_id = GetShelfIDForAppID(app_id);
  if (shelf_id) {
    // App item exists, pin it
    Pin(shelf_id);
  } else {
    // Otherwise, create a shortcut item for it.
    shelf_id = CreateAppShortcutLauncherItem(app_id, model_->item_count());
    if (CanPin(app_id))
      PersistPinnedState();
  }
}

void ChromeLauncherController::DoUnpinAppWithID(const std::string& app_id) {
  ash::ShelfID shelf_id = GetShelfIDForAppID(app_id);
  if (shelf_id && IsPinned(shelf_id))
    Unpin(shelf_id);
}

int ChromeLauncherController::PinRunningAppInternal(int index,
                                                    ash::ShelfID shelf_id) {
  int running_index = model_->ItemIndexByID(shelf_id);
  ash::ShelfItem item = model_->items()[running_index];
  DCHECK(item.type == ash::TYPE_WINDOWED_APP ||
         item.type == ash::TYPE_PLATFORM_APP);
  item.type = ash::TYPE_APP_SHORTCUT;
  model_->Set(running_index, item);
  // The |ShelfModel|'s weight system might reposition the item to a
  // new index, so we get the index again.
  running_index = model_->ItemIndexByID(shelf_id);
  if (running_index < index)
    --index;
  if (running_index != index)
    model_->Move(running_index, index);
  return index;
}

void ChromeLauncherController::UnpinRunningAppInternal(int index) {
  DCHECK_GE(index, 0);
  ash::ShelfItem item = model_->items()[index];
  DCHECK_EQ(item.type, ash::TYPE_APP_SHORTCUT);
  item.type = ash::TYPE_WINDOWED_APP;
  // A platform app and a windowed app are sharing TYPE_APP_SHORTCUT. As such
  // we have to check here what this was before it got a shortcut.
  LauncherItemController* controller = GetLauncherItemController(item.id);
  if (controller && controller->type() == LauncherItemController::TYPE_APP)
    item.type = ash::TYPE_PLATFORM_APP;
  model_->Set(index, item);
}

void ChromeLauncherController::UpdateAppLaunchersFromPref() {
  // There are various functions which will trigger a |PersistPinnedState| call
  // like a direct call to |DoPinAppWithID|, or an indirect call to the menu
  // model which will use weights to re-arrange the icons to new positions.
  // Since this function is meant to synchronize the "is state" with the
  // "sync state", it makes no sense to store any changes by this function back
  // into the pref state. Therefore we tell |persistPinnedState| to ignore any
  // invocations while we are running.
  base::AutoReset<bool> auto_reset(&ignore_persist_pinned_state_change_, true);
  std::vector<std::string> pinned_apps = GetListOfPinnedAppsAndBrowser();

  int index = 0;
  int max_index = model_->item_count();

  // When one of the two special items cannot be moved (and we do not know where
  // yet), we remember the current location in one of these variables.
  int chrome_index = -1;
  int app_list_index = -1;

  // Walk the model and |pinned_apps| from the pref lockstep, adding and
  // removing items as necessary. NB: This code uses plain old indexing instead
  // of iterators because of model mutations as part of the loop.
  std::vector<std::string>::const_iterator pref_app_id(pinned_apps.begin());
  for (; index < max_index && pref_app_id != pinned_apps.end(); ++index) {
    // Check if we have an item which we need to handle.
    if (*pref_app_id == extension_misc::kChromeAppId ||
        *pref_app_id == kAppShelfIdPlaceholder ||
        IsAppPinned(*pref_app_id)) {
      for (; index < max_index; ++index) {
        const ash::ShelfItem& item(model_->items()[index]);
        bool is_app_list = item.type == ash::TYPE_APP_LIST;
        bool is_chrome = item.type == ash::TYPE_BROWSER_SHORTCUT;
        if (item.type != ash::TYPE_APP_SHORTCUT && !is_app_list && !is_chrome)
          continue;
        LauncherItemController* controller = GetLauncherItemController(item.id);
        if ((kAppShelfIdPlaceholder == *pref_app_id && is_app_list) ||
            (extension_misc::kChromeAppId == *pref_app_id && is_chrome) ||
            (controller && controller->app_id() == *pref_app_id)) {
          // Check if an item needs to be moved here.
          MoveChromeOrApplistToFinalPosition(
              is_chrome, is_app_list, index, &chrome_index, &app_list_index);
          ++pref_app_id;
          break;
        } else {
          if (is_chrome || is_app_list) {
            // We cannot delete any of these shortcuts. As such we remember
            // their positions and move them later where they belong.
            if (is_chrome)
              chrome_index = index;
            else
              app_list_index = index;
            // And skip the item - or exit the loop if end is reached (note that
            // in that case we will reduce the index again by one and this only
            // compensates for it).
            if (index >= max_index - 1)
              break;
            ++index;
          } else {
            // Check if this is a platform or a windowed app.
            if (item.type == ash::TYPE_APP_SHORTCUT &&
                controller &&
                (controller->locked() ||
                 controller->type() == LauncherItemController::TYPE_APP)) {
              // Note: This will not change the amount of items (|max_index|).
              // Even changes to the actual |index| due to item weighting
              // changes should be fine.
              UnpinRunningAppInternal(index);
            } else {
              if (controller)
                LauncherItemClosed(item.id);
              --max_index;
            }
          }
          --index;
        }
      }
      // If the item wasn't found, that means id_to_item_controller_map_
      // is out of sync.
      DCHECK(index <= max_index);
    } else {
      // Check if the item was already running but not yet pinned.
      ash::ShelfID shelf_id = GetShelfIDForAppID(*pref_app_id);
      if (shelf_id) {
        // This app is running but not yet pinned. So pin and move it.
        index = PinRunningAppInternal(index, shelf_id);
      } else {
        // This app wasn't pinned before, insert a new entry.
        shelf_id = CreateAppShortcutLauncherItem(*pref_app_id, index);
        ++max_index;
        index = model_->ItemIndexByID(shelf_id);
      }
      ++pref_app_id;
    }
  }

  // Remove any trailing existing items.
  while (index < model_->item_count()) {
    const ash::ShelfItem& item(model_->items()[index]);
    if (item.type == ash::TYPE_APP_SHORTCUT) {
      LauncherItemController* controller = GetLauncherItemController(item.id);
      if (controller) {
        if (controller->locked() ||
            controller->type() == LauncherItemController::TYPE_APP) {
          UnpinRunningAppInternal(index);
        } else {
          LauncherItemClosed(item.id);
        }
      }
    } else {
      if (item.type == ash::TYPE_BROWSER_SHORTCUT)
        chrome_index = index;
      else if (item.type == ash::TYPE_APP_LIST)
        app_list_index = index;
      ++index;
    }
  }

  // Append unprocessed items from the pref to the end of the model.
  for (; pref_app_id != pinned_apps.end(); ++pref_app_id) {
    // All items but the chrome and / or app list shortcut needs to be added.
    bool is_chrome = *pref_app_id == extension_misc::kChromeAppId;
    bool is_app_list = *pref_app_id == kAppShelfIdPlaceholder;
    // Coming here we know the next item which can be finalized, either the
    // chrome item or the app launcher. The final position is the end of the
    // list. The menu model will make sure that the item is grouped according
    // to its weight (which we do not know here).
    if (!is_chrome && !is_app_list) {
      DoPinAppWithID(*pref_app_id);
      int target_index = FindInsertionPoint(false);
      ash::ShelfID id = GetShelfIDForAppID(*pref_app_id);
      int source_index = model_->ItemIndexByID(id);
      if (source_index != target_index)
        model_->Move(source_index, target_index);

      // Needed for the old layout - the weight might force it to be lower in
      // rank.
      if (app_list_index != -1 && target_index <= app_list_index)
        ++app_list_index;
    } else {
      int target_index = FindInsertionPoint(is_app_list);
      MoveChromeOrApplistToFinalPosition(
          is_chrome, is_app_list, target_index, &chrome_index, &app_list_index);
    }
  }
}

void ChromeLauncherController::SetShelfAutoHideBehaviorPrefs(
    ash::ShelfAutoHideBehavior behavior,
    aura::Window* root_window) {
  const char* value = NULL;
  switch (behavior) {
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      value = ash::kShelfAutoHideBehaviorAlways;
      break;
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      value = ash::kShelfAutoHideBehaviorNever;
      break;
    case ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      // This one should not be a valid preference option for now. We only want
      // to completely hide it when we run in app mode - or while we temporarily
      // hide the shelf as part of an animation (e.g. the multi user change).
      return;
  }

  UpdatePerDisplayPref(
      profile_->GetPrefs(), root_window, prefs::kShelfAutoHideBehavior, value);

  if (root_window == ash::Shell::GetPrimaryRootWindow()) {
    // See comment in |kShelfAlignment| about why we have two prefs here.
    profile_->GetPrefs()->SetString(prefs::kShelfAutoHideBehaviorLocal, value);
    profile_->GetPrefs()->SetString(prefs::kShelfAutoHideBehavior, value);
  }
}

void ChromeLauncherController::SetShelfAutoHideBehaviorFromPrefs() {
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    ash::Shell::GetInstance()->SetShelfAutoHideBehavior(
        GetShelfAutoHideBehavior(*iter), *iter);
  }
}

void ChromeLauncherController::SetShelfAlignmentFromPrefs() {
  if (!ash::ShelfWidget::ShelfAlignmentAllowed())
    return;

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    ash::Shell::GetInstance()->SetShelfAlignment(
        GetShelfAlignmentFromPrefs(profile_, *iter), *iter);
  }
}

void ChromeLauncherController::SetShelfBehaviorsFromPrefs() {
  SetShelfAutoHideBehaviorFromPrefs();
  SetShelfAlignmentFromPrefs();
}

#if defined(OS_CHROMEOS)
void ChromeLauncherController::SetVirtualKeyboardBehaviorFromPrefs() {
  const PrefService* service = profile_->GetPrefs();
  const bool was_enabled = keyboard::IsKeyboardEnabled();
  if (!service->HasPrefPath(prefs::kTouchVirtualKeyboardEnabled)) {
    keyboard::SetKeyboardShowOverride(keyboard::KEYBOARD_SHOW_OVERRIDE_NONE);
  } else {
    const bool enable = service->GetBoolean(
        prefs::kTouchVirtualKeyboardEnabled);
    keyboard::SetKeyboardShowOverride(
        enable ? keyboard::KEYBOARD_SHOW_OVERRIDE_ENABLED
                : keyboard::KEYBOARD_SHOW_OVERRIDE_DISABLED);
  }
  const bool is_enabled = keyboard::IsKeyboardEnabled();
  if (was_enabled && !is_enabled)
    ash::Shell::GetInstance()->DeactivateKeyboard();
  else if (is_enabled && !was_enabled)
    ash::Shell::GetInstance()->CreateKeyboard();
}
#endif  // defined(OS_CHROMEOS)

ash::ShelfItemStatus ChromeLauncherController::GetAppState(
    const std::string& app_id) {
  ash::ShelfItemStatus status = ash::STATUS_CLOSED;
  for (WebContentsToAppIDMap::iterator it = web_contents_to_app_id_.begin();
       it != web_contents_to_app_id_.end();
       ++it) {
    if (it->second == app_id) {
      Browser* browser = chrome::FindBrowserWithWebContents(it->first);
      // Usually there should never be an item in our |web_contents_to_app_id_|
      // list which got deleted already. However - in some situations e.g.
      // Browser::SwapTabContent there is temporarily no associated browser.
      if (!browser)
        continue;
      if (browser->window()->IsActive()) {
        return browser->tab_strip_model()->GetActiveWebContents() == it->first ?
            ash::STATUS_ACTIVE : ash::STATUS_RUNNING;
      } else {
        status = ash::STATUS_RUNNING;
      }
    }
  }
  return status;
}

ash::ShelfID ChromeLauncherController::InsertAppLauncherItem(
    LauncherItemController* controller,
    const std::string& app_id,
    ash::ShelfItemStatus status,
    int index,
    ash::ShelfItemType shelf_item_type) {
  ash::ShelfID id = model_->next_id();
  CHECK(!HasShelfIDToAppIDMapping(id));
  CHECK(controller);
  id_to_item_controller_map_[id] = controller;
  controller->set_shelf_id(id);

  ash::ShelfItem item;
  item.type = shelf_item_type;
  item.image = extensions::util::GetDefaultAppIcon();

  ash::ShelfItemStatus new_state = GetAppState(app_id);
  if (new_state != ash::STATUS_CLOSED)
    status = new_state;

  item.status = status;

  model_->AddAt(index, item);

  app_icon_loader_->FetchImage(app_id);
  app_icon_loader_->UpdateImage(app_id);

  SetShelfItemDelegate(id, controller);

  return id;
}

std::vector<content::WebContents*>
ChromeLauncherController::GetV1ApplicationsFromController(
    LauncherItemController* controller) {
  DCHECK(controller->type() == LauncherItemController::TYPE_SHORTCUT);
  AppShortcutLauncherItemController* app_controller =
      static_cast<AppShortcutLauncherItemController*>(controller);
  return app_controller->GetRunningApplications();
}

BrowserShortcutLauncherItemController*
ChromeLauncherController::GetBrowserShortcutLauncherItemController() {
  for (IDToItemControllerMap::iterator i = id_to_item_controller_map_.begin();
      i != id_to_item_controller_map_.end(); ++i) {
    int index = model_->ItemIndexByID(i->first);
    const ash::ShelfItem& item = model_->items()[index];
    if (item.type == ash::TYPE_BROWSER_SHORTCUT)
      return static_cast<BrowserShortcutLauncherItemController*>(i->second);
  }
  NOTREACHED() << "There should be always a BrowserLauncherItemController.";
  return nullptr;
}

ash::ShelfID ChromeLauncherController::CreateBrowserShortcutLauncherItem() {
  ash::ShelfItem browser_shortcut;
  browser_shortcut.type = ash::TYPE_BROWSER_SHORTCUT;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  browser_shortcut.image = *rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_32);
  ash::ShelfID id = model_->next_id();
  size_t index = GetChromeIconIndexForCreation();
  model_->AddAt(index, browser_shortcut);
  id_to_item_controller_map_[id] =
      new BrowserShortcutLauncherItemController(this);
  id_to_item_controller_map_[id]->set_shelf_id(id);
  // ShelfItemDelegateManager owns BrowserShortcutLauncherItemController.
  SetShelfItemDelegate(id, id_to_item_controller_map_[id]);
  return id;
}

void ChromeLauncherController::PersistChromeItemIndex(int index) {
  profile_->GetPrefs()->SetInteger(prefs::kShelfChromeIconIndex, index);
}

int ChromeLauncherController::GetChromeIconIndexFromPref() const {
  size_t index = profile_->GetPrefs()->GetInteger(prefs::kShelfChromeIconIndex);
  const base::ListValue* pinned_apps_pref =
      profile_->GetPrefs()->GetList(prefs::kPinnedLauncherApps);
  return std::max(static_cast<size_t>(0),
                  std::min(pinned_apps_pref->GetSize(), index));
}

void ChromeLauncherController::MoveChromeOrApplistToFinalPosition(
    bool is_chrome,
    bool is_app_list,
    int target_index,
    int* chrome_index,
    int* app_list_index) {
  if (is_chrome && *chrome_index != -1) {
    model_->Move(*chrome_index, target_index);
    if (*app_list_index != -1 &&
        *chrome_index < *app_list_index &&
        target_index > *app_list_index)
      --(*app_list_index);
    *chrome_index = -1;
  } else if (is_app_list && *app_list_index != -1) {
    model_->Move(*app_list_index, target_index);
    if (*chrome_index != -1 &&
        *app_list_index < *chrome_index &&
        target_index > *chrome_index)
      --(*chrome_index);
    *app_list_index = -1;
  }
}

int ChromeLauncherController::FindInsertionPoint(bool is_app_list) {
  // Keeping this change small to backport to M33&32 (see crbug.com/329597).
  // TODO(skuhne): With the removal of the legacy shelf layout we should remove
  // the ability to move the app list item since this was never used. We should
  // instead ask the ShelfModel::ValidateInsertionIndex or similir for an index.
  if (is_app_list)
    return 0;

  for (int i = model_->item_count() - 1; i > 0; --i) {
    ash::ShelfItemType type = model_->items()[i].type;
    if (type == ash::TYPE_APP_SHORTCUT ||
        (is_app_list && type == ash::TYPE_APP_LIST) ||
        type == ash::TYPE_BROWSER_SHORTCUT) {
      return i;
    }
  }
  return 0;
}

int ChromeLauncherController::GetChromeIconIndexForCreation() {
  // We get the list of pinned apps as they currently would get pinned.
  // Within this list the chrome icon will be the correct location.
  std::vector<std::string> pinned_apps = GetListOfPinnedAppsAndBrowser();

  std::vector<std::string>::iterator it =
      std::find(pinned_apps.begin(),
                pinned_apps.end(),
                std::string(extension_misc::kChromeAppId));
  DCHECK(it != pinned_apps.end());
  int index = it - pinned_apps.begin();

  // We should do here a comparison between the is state and the "want to be"
  // state since some apps might be able to pin but are not yet. Instead - for
  // the time being we clamp against the amount of known items and wait for the
  // next |UpdateAppLaunchersFromPref()| call to correct it - it will come since
  // the pinning will be done then.
  return std::min(model_->item_count(), index);
}

std::vector<std::string>
ChromeLauncherController::GetListOfPinnedAppsAndBrowser() {
  // Adding the app list item to the list of items requires that the ID is not
  // a valid and known ID for the extension system. The ID was constructed that
  // way - but just to make sure...
  DCHECK(!app_tab_helper_->IsValidIDForCurrentUser(kAppShelfIdPlaceholder));

  std::vector<std::string> pinned_apps;

  // Get the new incarnation of the list.
  const base::ListValue* pinned_apps_pref =
      profile_->GetPrefs()->GetList(prefs::kPinnedLauncherApps);

  const base::ListValue* policy_pinned_apps_pref =
      profile_->GetPrefs()->GetList(prefs::kPolicyPinnedLauncherApps);

  // Keep track of the addition of the chrome and the app list icon.
  bool chrome_icon_added = false;
  bool app_list_icon_added = false;
  size_t chrome_icon_index = GetChromeIconIndexFromPref();

  // See if the chrome string is already in the pinned list and remove it if
  // needed.
  base::Value* chrome_app = ash::CreateAppDict(extension_misc::kChromeAppId);
  if (chrome_app) {
    chrome_icon_added = pinned_apps_pref->Find(*chrome_app) !=
        pinned_apps_pref->end();
    delete chrome_app;
  }

  if (policy_pinned_apps_pref) {
    for (size_t index = 0; index < policy_pinned_apps_pref->GetSize();
         ++index) {
      const base::DictionaryValue* app = NULL;
      std::string app_id;
      if (policy_pinned_apps_pref->GetDictionary(index, &app) &&
          app->GetString(ash::kPinnedAppsPrefAppIDPath, &app_id) &&
          (std::find(pinned_apps.begin(), pinned_apps.end(), app_id) ==
           pinned_apps.end()))
        update_pinned_apps(
            app_id, app_tab_helper_->IsValidIDForCurrentUser(app_id),
            &pinned_apps, &chrome_icon_added, &app_list_icon_added);
    }
  }

  for (size_t index = 0; index < pinned_apps_pref->GetSize(); ++index) {
    // We need to position the chrome icon relative to it's place in the pinned
    // preference list - even if an item of that list isn't shown yet.
    if (index == chrome_icon_index && !chrome_icon_added) {
      pinned_apps.push_back(extension_misc::kChromeAppId);
      chrome_icon_added = true;
    }
    const base::DictionaryValue* app = NULL;
    std::string app_id;
    if (pinned_apps_pref->GetDictionary(index, &app) &&
        app->GetString(ash::kPinnedAppsPrefAppIDPath, &app_id) &&
        (std::find(pinned_apps.begin(), pinned_apps.end(), app_id) ==
             pinned_apps.end())) {
      bool pinned_by_policy;
      if (app->GetBoolean(ash::kPinnedAppsPrefPinnedByPolicy,
                          &pinned_by_policy) &&
          pinned_by_policy) {
        continue;
      }
      update_pinned_apps(
          app_id, app_tab_helper_->IsValidIDForCurrentUser(app_id),
          &pinned_apps, &chrome_icon_added, &app_list_icon_added);
    }
  }

  // If not added yet, the chrome item will be the last item in the list.
  if (!chrome_icon_added)
    pinned_apps.push_back(extension_misc::kChromeAppId);

  // If not added yet, add the app list item either at the end or at the
  // beginning - depending on the shelf layout.
  if (!app_list_icon_added) {
    pinned_apps.insert(pinned_apps.begin(), kAppShelfIdPlaceholder);
  }
  return pinned_apps;
}

bool ChromeLauncherController::IsIncognito(
    const content::WebContents* web_contents) const {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return profile->IsOffTheRecord() && !profile->IsGuestSession() &&
         !profile->IsSystemProfile();
}

void ChromeLauncherController::CloseWindowedAppsFromRemovedExtension(
    const std::string& app_id,
    const Profile* profile) {
  // This function cannot rely on the controller's enumeration functionality
  // since the extension has already be unloaded.
  const BrowserList* ash_browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  std::vector<Browser*> browser_to_close;
  for (BrowserList::const_reverse_iterator
           it = ash_browser_list->begin_last_active();
       it != ash_browser_list->end_last_active(); ++it) {
    Browser* browser = *it;
    if (!browser->is_type_tabbed() && browser->is_type_popup() &&
        browser->is_app() &&
        app_id ==
            web_app::GetExtensionIdFromApplicationName(browser->app_name()) &&
        profile == browser->profile()) {
      browser_to_close.push_back(browser);
    }
  }
  while (!browser_to_close.empty()) {
    TabStripModel* tab_strip = browser_to_close.back()->tab_strip_model();
    tab_strip->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
    browser_to_close.pop_back();
  }
}

void ChromeLauncherController::SetShelfItemDelegate(
    ash::ShelfID id,
    ash::ShelfItemDelegate* item_delegate) {
  DCHECK_GT(id, 0);
  DCHECK(item_delegate);
  DCHECK(item_delegate_manager_);
  item_delegate_manager_->SetShelfItemDelegate(
      id, scoped_ptr<ash::ShelfItemDelegate>(item_delegate));
}

void ChromeLauncherController::AttachProfile(Profile* profile) {
  profile_ = profile;
  // Either add the profile to the list of known profiles and make it the active
  // one for some functions of AppTabHelper or create a new one.
  if (!app_tab_helper_.get())
    app_tab_helper_.reset(new LauncherAppTabHelper(profile_));
  else
    app_tab_helper_->SetCurrentUser(profile_);
  // TODO(skuhne): The AppIconLoaderImpl has the same problem. Each loaded
  // image is associated with a profile (it's loader requires the profile).
  // Since icon size changes are possible, the icon could be requested to be
  // reloaded. However - having it not multi profile aware would cause problems
  // if the icon cache gets deleted upon user switch.
  app_icon_loader_.reset(new extensions::AppIconLoaderImpl(
      profile_, extension_misc::EXTENSION_ICON_SMALL, this));

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kPinnedLauncherApps,
      base::Bind(&ChromeLauncherController::UpdateAppLaunchersFromPref,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kPolicyPinnedLauncherApps,
      base::Bind(&ChromeLauncherController::UpdateAppLaunchersFromPref,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kShelfAlignmentLocal,
      base::Bind(&ChromeLauncherController::SetShelfAlignmentFromPrefs,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kShelfAutoHideBehaviorLocal,
      base::Bind(&ChromeLauncherController::
                     SetShelfAutoHideBehaviorFromPrefs,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kShelfPreferences,
      base::Bind(&ChromeLauncherController::SetShelfBehaviorsFromPrefs,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  pref_change_registrar_.Add(
      prefs::kTouchVirtualKeyboardEnabled,
      base::Bind(&ChromeLauncherController::SetVirtualKeyboardBehaviorFromPrefs,
                 base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)

  extensions::ExtensionRegistry::Get(profile_)->AddObserver(this);
}

void ChromeLauncherController::ReleaseProfile() {
  if (app_sync_ui_state_)
    app_sync_ui_state_->RemoveObserver(this);

  extensions::ExtensionRegistry::Get(profile_)->RemoveObserver(this);

  PrefServiceSyncableFromProfile(profile_)->RemoveObserver(this);

  pref_change_registrar_.RemoveAll();
}
