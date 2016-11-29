// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_impl.h"

#include <stddef.h>

#include <vector>

#include "ash/common/multi_profile_uma.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/app_sync_ui_state.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_browser.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_tab.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_types.h"
#include "chrome/browser/ui/ash/launcher/launcher_arc_app_updater.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/launcher_extension_app_updater.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/multi_profile_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/multi_profile_browser_status_monitor.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/url_pattern.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/wm/core/window_animations.h"

using extensions::Extension;
using extensions::UnloadedExtensionInfo;
using extension_misc::kGmailAppId;
using content::WebContents;

namespace {

int64_t GetDisplayIDForShelf(ash::WmShelf* shelf) {
  display::Display display =
      shelf->GetWindow()->GetRootWindow()->GetDisplayNearestWindow();
  DCHECK(display.is_valid());
  return display.id();
}

}  // namespace

// A class to get events from ChromeOS when a user gets changed or added.
class ChromeLauncherControllerUserSwitchObserver
    : public user_manager::UserManager::UserSessionStateObserver {
 public:
  ChromeLauncherControllerUserSwitchObserver(
      ChromeLauncherControllerImpl* controller)
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

  // The owning ChromeLauncherControllerImpl.
  ChromeLauncherControllerImpl* controller_;

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
  if (!profile) {
    added_user_ids_waiting_for_profiles_.insert(
        active_user->GetAccountId().GetUserEmail());
  } else {
    AddUser(profile);
  }
}

void ChromeLauncherControllerUserSwitchObserver::OnUserProfileReadyToSwitch(
    Profile* profile) {
  if (!added_user_ids_waiting_for_profiles_.empty()) {
    // Check if the profile is from a user which was on the waiting list.
    // TODO(alemate): added_user_ids_waiting_for_profiles_ should be
    // a set<AccountId>
    std::string user_id =
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();
    std::set<std::string>::iterator it =
        std::find(added_user_ids_waiting_for_profiles_.begin(),
                  added_user_ids_waiting_for_profiles_.end(), user_id);
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

ChromeLauncherControllerImpl::ChromeLauncherControllerImpl(
    Profile* profile,
    ash::ShelfModel* model)
    : model_(model), weak_ptr_factory_(this) {
  DCHECK(model_);
  if (!profile) {
    // If no profile was passed, we take the currently active profile and use it
    // as the owner of the current desktop.
    // Use the original profile as on chromeos we may get a temporary off the
    // record profile, unless in guest session (where off the record profile is
    // the right one).
    profile = ProfileManager::GetActiveUserProfile();
    if (!profile->IsGuestSession() && !profile->IsSystemProfile())
      profile = profile->GetOriginalProfile();

    app_sync_ui_state_ = AppSyncUIState::Get(profile);
    if (app_sync_ui_state_)
      app_sync_ui_state_->AddObserver(this);
  }

  // All profile relevant settings get bound to the current profile.
  AttachProfile(profile);
  model_->AddObserver(this);

  if (arc::ArcSessionManager::IsAllowedForProfile(this->profile()))
    arc_deferred_launcher_.reset(new ArcAppDeferredLauncherController(this));

  // In multi profile mode we might have a window manager. We try to create it
  // here. If the instantiation fails, the manager is not needed.
  chrome::MultiUserWindowManager::CreateInstance();

  // On Chrome OS using multi profile we want to switch the content of the shelf
  // with a user change. Note that for unit tests the instance can be NULL.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() !=
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_OFF) {
    user_switch_observer_.reset(
        new ChromeLauncherControllerUserSwitchObserver(this));
  }

  std::unique_ptr<AppWindowLauncherController> extension_app_window_controller;
  // Create our v1/v2 application / browser monitors which will inform the
  // launcher of status changes.
  if (chrome::MultiUserWindowManager::GetMultiProfileMode() ==
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED) {
    // If running in separated destkop mode, we create the multi profile version
    // of status monitor.
    browser_status_monitor_.reset(new MultiProfileBrowserStatusMonitor(this));
    extension_app_window_controller.reset(
        new MultiProfileAppWindowLauncherController(this));
  } else {
    // Create our v1/v2 application / browser monitors which will inform the
    // launcher of status changes.
    browser_status_monitor_.reset(new BrowserStatusMonitor(this));
    extension_app_window_controller.reset(
        new ExtensionAppWindowLauncherController(this));
  }
  app_window_controllers_.push_back(std::move(extension_app_window_controller));

  std::unique_ptr<AppWindowLauncherController> arc_app_window_controller;
  arc_app_window_controller.reset(
      new ArcAppWindowLauncherController(this, this));
  app_window_controllers_.push_back(std::move(arc_app_window_controller));

  // Right now ash::Shell isn't created for tests.
  // TODO(mukai): Allows it to observe display change and write tests.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
}

ChromeLauncherControllerImpl::~ChromeLauncherControllerImpl() {
  // Reset the BrowserStatusMonitor as it has a weak pointer to this.
  browser_status_monitor_.reset();

  // Reset the app window controllers here since it has a weak pointer to this.
  app_window_controllers_.clear();

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

  // Release all profile dependent resources.
  ReleaseProfile();

  // Get rid of the multi user window manager instance.
  chrome::MultiUserWindowManager::DeleteInstance();
}

void ChromeLauncherControllerImpl::Init() {
  CreateBrowserShortcutLauncherItem();
  UpdateAppLaunchersFromPref();

  // TODO(sky): update unit test so that this test isn't necessary.
  if (ash::Shell::HasInstance())
    SetVirtualKeyboardBehaviorFromPrefs();

  prefs_observer_ =
      ash::launcher::ChromeLauncherPrefsObserver::CreateIfNecessary(profile());
}

ash::ShelfID ChromeLauncherControllerImpl::CreateAppLauncherItem(
    LauncherItemController* controller,
    const std::string& app_id,
    ash::ShelfItemStatus status) {
  return InsertAppLauncherItem(controller, app_id, status, model_->item_count(),
                               controller->GetShelfItemType());
}

void ChromeLauncherControllerImpl::SetItemStatus(ash::ShelfID id,
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

void ChromeLauncherControllerImpl::SetItemController(
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

void ChromeLauncherControllerImpl::CloseLauncherItem(ash::ShelfID id) {
  CHECK(id);
  if (IsPinned(id)) {
    // Create a new shortcut controller.
    IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
    CHECK(iter != id_to_item_controller_map_.end());
    SetItemStatus(id, ash::STATUS_CLOSED);
    std::string app_id = iter->second->app_id();
    std::string launch_id = iter->second->launch_id();
    iter->second =
        AppShortcutLauncherItemController::Create(app_id, launch_id, this);
    iter->second->set_shelf_id(id);
    // Existing controller is destroyed and replaced by registering again.
    SetShelfItemDelegate(id, iter->second);
  } else {
    LauncherItemClosed(id);
  }
}

void ChromeLauncherControllerImpl::Pin(ash::ShelfID id) {
  DCHECK(HasShelfIDToAppIDMapping(id));

  int index = model_->ItemIndexByID(id);
  DCHECK_GE(index, 0);

  ash::ShelfItem item = model_->items()[index];

  if (item.type == ash::TYPE_APP) {
    item.type = ash::TYPE_APP_SHORTCUT;
    model_->Set(index, item);
  } else if (item.type != ash::TYPE_APP_SHORTCUT) {
    return;
  }

  SyncPinPosition(id);
}

void ChromeLauncherControllerImpl::Unpin(ash::ShelfID id) {
  UnpinAndUpdatePrefs(id, true /* update_prefs */);
}

void ChromeLauncherControllerImpl::UnpinAndUpdatePrefs(ash::ShelfID id,
                                                       bool update_prefs) {
  LauncherItemController* controller = GetLauncherItemController(id);
  CHECK(controller);

  if (update_prefs) {
    ash::launcher::RemovePinPosition(
        profile(), ash::launcher::AppLauncherId(GetAppIDForShelfID(id),
                                                GetLaunchIDForShelfID(id)));
  }

  if (controller->type() == LauncherItemController::TYPE_APP ||
      controller->locked()) {
    UnpinRunningAppInternal(model_->ItemIndexByID(id));
  } else {
    LauncherItemClosed(id);
  }
}

bool ChromeLauncherControllerImpl::IsPinned(ash::ShelfID id) {
  int index = model_->ItemIndexByID(id);
  if (index < 0)
    return false;
  ash::ShelfItemType type = model_->items()[index].type;
  return (type == ash::TYPE_APP_SHORTCUT || type == ash::TYPE_BROWSER_SHORTCUT);
}

void ChromeLauncherControllerImpl::TogglePinned(ash::ShelfID id) {
  if (!HasShelfIDToAppIDMapping(id))
    return;  // May happen if item closed with menu open.

  if (IsPinned(id))
    Unpin(id);
  else
    Pin(id);
}

bool ChromeLauncherControllerImpl::IsPinnable(ash::ShelfID id) const {
  int index = model_->ItemIndexByID(id);
  if (index == -1)
    return false;

  ash::ShelfItemType type = model_->items()[index].type;
  std::string app_id;
  return ((type == ash::TYPE_APP_SHORTCUT || type == ash::TYPE_APP) &&
          model_->GetShelfItemDelegate(id)->CanPin());
}

void ChromeLauncherControllerImpl::LockV1AppWithID(const std::string& app_id) {
  ash::ShelfID id = GetShelfIDForAppID(app_id);
  if (id == ash::kInvalidShelfID) {
    CreateAppShortcutLauncherItemWithType(ash::launcher::AppLauncherId(app_id),
                                          model_->item_count(), ash::TYPE_APP);
    id = GetShelfIDForAppID(app_id);
  }
  CHECK(id);
  GetLauncherItemController(id)->lock();
}

void ChromeLauncherControllerImpl::UnlockV1AppWithID(
    const std::string& app_id) {
  ash::ShelfID id = GetShelfIDForAppID(app_id);
  CHECK_NE(id, ash::kInvalidShelfID);
  LauncherItemController* controller = GetLauncherItemController(id);
  controller->unlock();
  if (!controller->locked() && !IsPinned(id))
    CloseLauncherItem(id);
}

void ChromeLauncherControllerImpl::Launch(ash::ShelfID id, int event_flags) {
  LauncherItemController* controller = GetLauncherItemController(id);
  if (!controller)
    return;  // In case invoked from menu and item closed while menu up.
  controller->Launch(ash::LAUNCH_FROM_UNKNOWN, event_flags);
}

void ChromeLauncherControllerImpl::Close(ash::ShelfID id) {
  ash::ShelfItemDelegate* delegate = model_->GetShelfItemDelegate(id);
  if (!delegate)
    return;  // May happen if menu closed.
  delegate->Close();
}

bool ChromeLauncherControllerImpl::IsOpen(ash::ShelfID id) {
  const int index = model_->ItemIndexByID(id);
  return index >= 0 && model_->items()[index].status != ash::STATUS_CLOSED;
}

bool ChromeLauncherControllerImpl::IsPlatformApp(ash::ShelfID id) {
  if (!HasShelfIDToAppIDMapping(id))
    return false;

  std::string app_id = GetAppIDForShelfID(id);
  const Extension* extension = GetExtensionForAppID(app_id, profile());
  // An extension can be synced / updated at any time and therefore not be
  // available.
  return extension ? extension->is_platform_app() : false;
}

void ChromeLauncherControllerImpl::ActivateApp(const std::string& app_id,
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
  std::unique_ptr<AppShortcutLauncherItemController> app_controller(
      AppShortcutLauncherItemController::Create(app_id, "", this));
  if (!app_controller->GetRunningApplications().empty())
    app_controller->Activate(source);
  else
    LaunchApp(app_id, source, event_flags);
}

extensions::LaunchType ChromeLauncherControllerImpl::GetLaunchType(
    ash::ShelfID id) {
  const Extension* extension =
      GetExtensionForAppID(GetAppIDForShelfID(id), profile());

  // An extension can be unloaded/updated/unavailable at any time.
  if (!extension)
    return extensions::LAUNCH_TYPE_DEFAULT;

  return extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile()),
                                   extension);
}

void ChromeLauncherControllerImpl::SetLauncherItemImage(
    ash::ShelfID shelf_id,
    const gfx::ImageSkia& image) {
  int index = model_->ItemIndexByID(shelf_id);
  if (index == -1)
    return;
  ash::ShelfItem item = model_->items()[index];
  item.image = image;
  model_->Set(index, item);
}

void ChromeLauncherControllerImpl::SetLaunchType(
    ash::ShelfID id,
    extensions::LaunchType launch_type) {
  LauncherItemController* controller = GetLauncherItemController(id);
  if (!controller)
    return;

  extensions::SetLaunchType(profile(), controller->app_id(), launch_type);
}

void ChromeLauncherControllerImpl::UpdateAppState(
    content::WebContents* contents,
    AppState app_state) {
  std::string app_id = launcher_controller_helper()->GetAppID(contents);

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
                       app_state == APP_STATE_ACTIVE)
                          ? ash::STATUS_ACTIVE
                          : GetAppState(app_id));
  }
}

ash::ShelfID ChromeLauncherControllerImpl::GetShelfIDForWebContents(
    content::WebContents* contents) {
  DCHECK(contents);

  std::string app_id = launcher_controller_helper()->GetAppID(contents);

  if (app_id.empty() && ContentCanBeHandledByGmailApp(contents))
    app_id = kGmailAppId;

  ash::ShelfID id = GetShelfIDForAppID(app_id);

  if (app_id.empty() || !id) {
    int browser_index = model_->GetItemIndexForType(ash::TYPE_BROWSER_SHORTCUT);
    return model_->items()[browser_index].id;
  }

  return id;
}

void ChromeLauncherControllerImpl::SetRefocusURLPatternForTest(
    ash::ShelfID id,
    const GURL& url) {
  int index = model_->ItemIndexByID(id);
  if (index == -1) {
    NOTREACHED() << "Invalid launcher id";
    return;
  }

  ash::ShelfItemType type = model_->items()[index].type;
  if ((type == ash::TYPE_APP_SHORTCUT || type == ash::TYPE_APP) &&
      !IsPlatformApp(id)) {
    LauncherItemController* controller = GetLauncherItemController(id);
    DCHECK(controller);
    AppShortcutLauncherItemController* app_controller =
        static_cast<AppShortcutLauncherItemController*>(controller);
    app_controller->set_refocus_url(url);
  } else {
    NOTREACHED() << "Invalid launcher type";
  }
}

ash::ShelfItemDelegate::PerformedAction
ChromeLauncherControllerImpl::ActivateWindowOrMinimizeIfActive(
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
    window->Minimize();
    return ash::ShelfItemDelegate::kNoAction;
  }

  window->Show();
  window->Activate();
  return ash::ShelfItemDelegate::kExistingWindowActivated;
}

void ChromeLauncherControllerImpl::ActiveUserChanged(
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
  for (auto& controller : app_window_controllers_)
    controller->ActiveUserChanged(user_email);
  // Update the user specific shell properties from the new user profile.
  UpdateAppLaunchersFromPref();
  SetShelfBehaviorsFromPrefs();
  SetVirtualKeyboardBehaviorFromPrefs();

  // Restore the order of running, but unpinned applications for the activated
  // user.
  RestoreUnpinnedRunningApplicationOrder(user_email);
  // Inform the system tray of the change.
  ash::WmShell::Get()->system_tray_delegate()->ActiveUserWasChanged();
  // Force on-screen keyboard to reset.
  if (keyboard::IsKeyboardEnabled())
    ash::Shell::GetInstance()->CreateKeyboard();
}

void ChromeLauncherControllerImpl::AdditionalUserAddedToSession(
    Profile* profile) {
  // Switch the running applications to the new user.
  for (auto& controller : app_window_controllers_)
    controller->AdditionalUserAddedToSession(profile);
}

ChromeLauncherAppMenuItems ChromeLauncherControllerImpl::GetApplicationList(
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
ChromeLauncherControllerImpl::GetV1ApplicationsFromAppId(
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

void ChromeLauncherControllerImpl::ActivateShellApp(const std::string& app_id,
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

bool ChromeLauncherControllerImpl::IsWebContentHandledByApplication(
    content::WebContents* web_contents,
    const std::string& app_id) {
  if ((web_contents_to_app_id_.find(web_contents) !=
       web_contents_to_app_id_.end()) &&
      (web_contents_to_app_id_[web_contents] == app_id))
    return true;
  return (app_id == kGmailAppId && ContentCanBeHandledByGmailApp(web_contents));
}

bool ChromeLauncherControllerImpl::ContentCanBeHandledByGmailApp(
    content::WebContents* web_contents) {
  ash::ShelfID id = GetShelfIDForAppID(kGmailAppId);
  if (id) {
    const GURL url = web_contents->GetURL();
    // We need to extend the application matching for the gMail app beyond the
    // manifest file's specification. This is required because of the namespace
    // overlap with the offline app ("/mail/mu/").
    if (!base::MatchPattern(url.path(), "/mail/mu/*") &&
        base::MatchPattern(url.path(), "/mail/*") &&
        GetExtensionForAppID(kGmailAppId, profile()) &&
        GetExtensionForAppID(kGmailAppId, profile())->OverlapsWithOrigin(url))
      return true;
  }
  return false;
}

gfx::Image ChromeLauncherControllerImpl::GetAppListIcon(
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

base::string16 ChromeLauncherControllerImpl::GetAppListTitle(
    content::WebContents* web_contents) const {
  base::string16 title = web_contents->GetTitle();
  if (!title.empty())
    return title;
  WebContentsToAppIDMap::const_iterator iter =
      web_contents_to_app_id_.find(web_contents);
  if (iter != web_contents_to_app_id_.end()) {
    std::string app_id = iter->second;
    const extensions::Extension* extension =
        GetExtensionForAppID(app_id, profile());
    if (extension)
      return base::UTF8ToUTF16(extension->name());
  }
  return l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
}

BrowserShortcutLauncherItemController*
ChromeLauncherControllerImpl::GetBrowserShortcutLauncherItemController() {
  for (IDToItemControllerMap::iterator i = id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    int index = model_->ItemIndexByID(i->first);
    const ash::ShelfItem& item = model_->items()[index];
    if (item.type == ash::TYPE_BROWSER_SHORTCUT)
      return static_cast<BrowserShortcutLauncherItemController*>(i->second);
  }
  NOTREACHED()
      << "There should be always be a BrowserShortcutLauncherItemController.";
  return nullptr;
}

LauncherItemController* ChromeLauncherControllerImpl::GetLauncherItemController(
    const ash::ShelfID id) {
  if (!HasShelfIDToAppIDMapping(id))
    return nullptr;
  return id_to_item_controller_map_[id];
}

bool ChromeLauncherControllerImpl::ShelfBoundsChangesProbablyWithUser(
    ash::WmShelf* shelf,
    const AccountId& account_id) const {
  Profile* other_profile = multi_user_util::GetProfileFromAccountId(account_id);
  if (!other_profile || other_profile == profile())
    return false;

  // Note: The Auto hide state from preferences is not the same as the actual
  // visibility of the shelf. Depending on all the various states (full screen,
  // no window on desktop, multi user, ..) the shelf could be shown - or not.
  PrefService* prefs = profile()->GetPrefs();
  PrefService* other_prefs = other_profile->GetPrefs();
  const int64_t display = GetDisplayIDForShelf(shelf);
  const bool currently_shown =
      ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER ==
      ash::launcher::GetShelfAutoHideBehaviorPref(prefs, display);
  const bool other_shown =
      ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER ==
      ash::launcher::GetShelfAutoHideBehaviorPref(other_prefs, display);

  return currently_shown != other_shown ||
         ash::launcher::GetShelfAlignmentPref(prefs, display) !=
             ash::launcher::GetShelfAlignmentPref(other_prefs, display);
}

void ChromeLauncherControllerImpl::OnUserProfileReadyToSwitch(
    Profile* profile) {
  if (user_switch_observer_.get())
    user_switch_observer_->OnUserProfileReadyToSwitch(profile);
}

ArcAppDeferredLauncherController*
ChromeLauncherControllerImpl::GetArcDeferredLauncher() {
  return arc_deferred_launcher_.get();
}

const std::string& ChromeLauncherControllerImpl::GetLaunchIDForShelfID(
    ash::ShelfID id) {
  LauncherItemController* controller = GetLauncherItemController(id);
  return controller ? controller->launch_id() : base::EmptyString();
}

void ChromeLauncherControllerImpl::AttachProfile(Profile* profile_to_attach) {
  // The base class implementation updates the helper and app icon loaders.
  ChromeLauncherController::AttachProfile(profile_to_attach);

  pref_change_registrar_.Init(profile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kPolicyPinnedLauncherApps,
      base::Bind(&ChromeLauncherControllerImpl::UpdateAppLaunchersFromPref,
                 base::Unretained(this)));
  // Handling of prefs::kArcEnabled change should be called deferred to avoid
  // race condition when OnAppUninstalledPrepared for Arc apps is called after
  // UpdateAppLaunchersFromPref.
  pref_change_registrar_.Add(
      prefs::kArcEnabled,
      base::Bind(
          &ChromeLauncherControllerImpl::ScheduleUpdateAppLaunchersFromPref,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kShelfAlignmentLocal,
      base::Bind(&ChromeLauncherController::SetShelfAlignmentFromPrefs,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kShelfAutoHideBehaviorLocal,
      base::Bind(&ChromeLauncherController::SetShelfAutoHideBehaviorFromPrefs,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kShelfPreferences,
      base::Bind(&ChromeLauncherController::SetShelfBehaviorsFromPrefs,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kTouchVirtualKeyboardEnabled,
      base::Bind(
          &ChromeLauncherControllerImpl::SetVirtualKeyboardBehaviorFromPrefs,
          base::Unretained(this)));

  std::unique_ptr<LauncherAppUpdater> extension_app_updater(
      new LauncherExtensionAppUpdater(this, profile()));
  app_updaters_.push_back(std::move(extension_app_updater));

  if (arc::ArcSessionManager::IsAllowedForProfile(profile())) {
    std::unique_ptr<LauncherAppUpdater> arc_app_updater(
        new LauncherArcAppUpdater(this, profile()));
    app_updaters_.push_back(std::move(arc_app_updater));
  }

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  if (app_service)
    app_service->AddObserverAndStart(this);
}

///////////////////////////////////////////////////////////////////////////////
// ash::ShelfDelegate:

ash::ShelfID ChromeLauncherControllerImpl::GetShelfIDForAppID(
    const std::string& app_id) {
  // Get shelf id for app_id and empty launch_id.
  return GetShelfIDForAppIDAndLaunchID(app_id, std::string());
}

ash::ShelfID ChromeLauncherControllerImpl::GetShelfIDForAppIDAndLaunchID(
    const std::string& app_id,
    const std::string& launch_id) {
  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  for (const auto& id_to_item_controller_pair : id_to_item_controller_map_) {
    if (id_to_item_controller_pair.second->app_id() == shelf_app_id &&
        id_to_item_controller_pair.second->launch_id() == launch_id) {
      return id_to_item_controller_pair.first;
    }
  }
  return ash::kInvalidShelfID;
}

bool ChromeLauncherControllerImpl::HasShelfIDToAppIDMapping(
    ash::ShelfID id) const {
  return id_to_item_controller_map_.find(id) !=
         id_to_item_controller_map_.end();
}

const std::string& ChromeLauncherControllerImpl::GetAppIDForShelfID(
    ash::ShelfID id) {
  LauncherItemController* controller = GetLauncherItemController(id);
  if (controller)
    return controller->app_id();
  ash::ShelfItems::const_iterator item = model_->ItemByID(id);
  return item != model_->items().end() ? item->app_id : base::EmptyString();
}

void ChromeLauncherControllerImpl::PinAppWithID(const std::string& app_id) {
  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  if (GetPinnableForAppID(shelf_app_id, profile()) ==
      AppListControllerDelegate::PIN_EDITABLE)
    DoPinAppWithID(shelf_app_id);
  else
    NOTREACHED();
}

bool ChromeLauncherControllerImpl::IsAppPinned(const std::string& app_id) {
  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  for (IDToItemControllerMap::const_iterator i =
           id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    if (IsPinned(i->first) && i->second->app_id() == shelf_app_id)
      return true;
  }
  return false;
}

void ChromeLauncherControllerImpl::UnpinAppWithID(const std::string& app_id) {
  const std::string shelf_app_id =
      ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
  if (GetPinnableForAppID(shelf_app_id, profile()) ==
      AppListControllerDelegate::PIN_EDITABLE)
    DoUnpinAppWithID(shelf_app_id, true /* update_prefs */);
  else
    NOTREACHED();
}

///////////////////////////////////////////////////////////////////////////////
// LauncherAppUpdater::Delegate:

void ChromeLauncherControllerImpl::OnAppInstalled(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  if (IsAppPinned(app_id)) {
    // Clear and re-fetch to ensure icon is up-to-date.
    AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
    if (app_icon_loader) {
      app_icon_loader->ClearImage(app_id);
      app_icon_loader->FetchImage(app_id);
    }
  }

  UpdateAppLaunchersFromPref();
}

void ChromeLauncherControllerImpl::OnAppUpdated(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
  if (app_icon_loader)
    app_icon_loader->UpdateImage(app_id);
}

void ChromeLauncherControllerImpl::OnAppUninstalledPrepared(
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  // Since we might have windowed apps of this type which might have
  // outstanding locks which needs to be removed.
  const Profile* profile = Profile::FromBrowserContext(browser_context);
  if (GetShelfIDForAppID(app_id))
    CloseWindowedAppsFromRemovedExtension(app_id, profile);

  if (IsAppPinned(app_id)) {
    if (profile == this->profile()) {
      // Some apps may be removed locally. Don't remove pin position from sync
      // model. When needed, it is automatically deleted on app list model
      // update.
      DoUnpinAppWithID(app_id, false /* update_prefs */);
    }
    AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
    if (app_icon_loader)
      app_icon_loader->ClearImage(app_id);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ChromeLauncherControllerImpl protected:

ash::ShelfID ChromeLauncherControllerImpl::CreateAppShortcutLauncherItem(
    const ash::launcher::AppLauncherId& app_launcher_id,
    int index) {
  return CreateAppShortcutLauncherItemWithType(app_launcher_id, index,
                                               ash::TYPE_APP_SHORTCUT);
}

///////////////////////////////////////////////////////////////////////////////
// ChromeLauncherControllerImpl private:

void ChromeLauncherControllerImpl::RememberUnpinnedRunningApplicationOrder() {
  RunningAppListIds list;
  for (int i = 0; i < model_->item_count(); i++) {
    if (model_->items()[i].type == ash::TYPE_APP)
      list.push_back(GetAppIDForShelfID(model_->items()[i].id));
  }
  const std::string user_email =
      multi_user_util::GetAccountIdFromProfile(profile()).GetUserEmail();
  last_used_running_application_order_[user_email] = list;
}

void ChromeLauncherControllerImpl::RestoreUnpinnedRunningApplicationOrder(
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
      if (model_->items()[app_index].type == ash::TYPE_APP) {
        if (running_index != app_index)
          model_->Move(running_index, app_index);
        running_index++;
      }
    }
  }
}

ash::ShelfID
ChromeLauncherControllerImpl::CreateAppShortcutLauncherItemWithType(
    const ash::launcher::AppLauncherId& app_launcher_id,
    int index,
    ash::ShelfItemType shelf_item_type) {
  AppShortcutLauncherItemController* controller =
      AppShortcutLauncherItemController::Create(
          app_launcher_id.app_id(), app_launcher_id.launch_id(), this);
  ash::ShelfID shelf_id =
      InsertAppLauncherItem(controller, app_launcher_id.app_id(),
                            ash::STATUS_CLOSED, index, shelf_item_type);
  return shelf_id;
}

void ChromeLauncherControllerImpl::LauncherItemClosed(ash::ShelfID id) {
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  CHECK(iter != id_to_item_controller_map_.end());
  CHECK(iter->second);
  const std::string& app_id = iter->second->app_id();
  AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
  if (app_icon_loader)
    app_icon_loader->ClearImage(app_id);
  id_to_item_controller_map_.erase(iter);
  int index = model_->ItemIndexByID(id);
  // A "browser proxy" is not known to the model and this removal does
  // therefore not need to be propagated to the model.
  if (index != -1)
    model_->RemoveItemAt(index);
}

void ChromeLauncherControllerImpl::DoPinAppWithID(const std::string& app_id) {
  // If there is an item, do nothing and return.
  if (IsAppPinned(app_id))
    return;

  ash::ShelfID shelf_id = GetShelfIDForAppID(app_id);
  if (shelf_id) {
    // App item exists, pin it
    Pin(shelf_id);
  } else {
    // Otherwise, create a shortcut item for it.
    shelf_id = CreateAppShortcutLauncherItem(
        ash::launcher::AppLauncherId(app_id), model_->item_count());
    SyncPinPosition(shelf_id);
  }
}

void ChromeLauncherControllerImpl::DoUnpinAppWithID(const std::string& app_id,
                                                    bool update_prefs) {
  ash::ShelfID shelf_id = GetShelfIDForAppID(app_id);
  if (shelf_id && IsPinned(shelf_id))
    UnpinAndUpdatePrefs(shelf_id, update_prefs);
}

void ChromeLauncherControllerImpl::PinRunningAppInternal(
    int index,
    ash::ShelfID shelf_id) {
  int running_index = model_->ItemIndexByID(shelf_id);
  ash::ShelfItem item = model_->items()[running_index];
  DCHECK_EQ(item.type, ash::TYPE_APP);
  item.type = ash::TYPE_APP_SHORTCUT;
  model_->Set(running_index, item);
  // The |ShelfModel|'s weight system might reposition the item to a
  // new index, so we get the index again.
  running_index = model_->ItemIndexByID(shelf_id);
  if (running_index < index)
    --index;
  if (running_index != index)
    model_->Move(running_index, index);
}

void ChromeLauncherControllerImpl::UnpinRunningAppInternal(int index) {
  DCHECK_GE(index, 0);
  ash::ShelfItem item = model_->items()[index];
  DCHECK_EQ(item.type, ash::TYPE_APP_SHORTCUT);
  item.type = ash::TYPE_APP;
  model_->Set(index, item);
}

void ChromeLauncherControllerImpl::SyncPinPosition(ash::ShelfID shelf_id) {
  DCHECK(shelf_id);
  if (ignore_persist_pinned_state_change_)
    return;

  const int max_index = model_->item_count();
  const int index = model_->ItemIndexByID(shelf_id);
  DCHECK_GT(index, 0);

  const std::string& app_id = GetAppIDForShelfID(shelf_id);
  DCHECK(!app_id.empty());
  const std::string& launch_id = GetLaunchIDForShelfID(shelf_id);

  std::string app_id_before;
  std::string launch_id_before;
  std::vector<ash::launcher::AppLauncherId> app_launcher_ids_after;

  for (int i = index - 1; i > 0; --i) {
    const ash::ShelfID shelf_id_before = model_->items()[i].id;
    if (IsPinned(shelf_id_before)) {
      app_id_before = GetAppIDForShelfID(shelf_id_before);
      DCHECK(!app_id_before.empty());
      launch_id_before = GetLaunchIDForShelfID(shelf_id_before);
      break;
    }
  }

  for (int i = index + 1; i < max_index; ++i) {
    const ash::ShelfID shelf_id_after = model_->items()[i].id;
    if (IsPinned(shelf_id_after)) {
      const std::string app_id_after = GetAppIDForShelfID(shelf_id_after);
      DCHECK(!app_id_after.empty());
      const std::string launch_id_after = GetLaunchIDForShelfID(shelf_id_after);
      app_launcher_ids_after.push_back(
          ash::launcher::AppLauncherId(app_id_after, launch_id_after));
    }
  }

  ash::launcher::AppLauncherId app_launcher_id_before =
      app_id_before.empty()
          ? ash::launcher::AppLauncherId()
          : ash::launcher::AppLauncherId(app_id_before, launch_id_before);

  ash::launcher::SetPinPosition(profile(),
                                ash::launcher::AppLauncherId(app_id, launch_id),
                                app_launcher_id_before, app_launcher_ids_after);
}

void ChromeLauncherControllerImpl::OnSyncModelUpdated() {
  UpdateAppLaunchersFromPref();
}

void ChromeLauncherControllerImpl::ScheduleUpdateAppLaunchersFromPref() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ChromeLauncherControllerImpl::UpdateAppLaunchersFromPref,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ChromeLauncherControllerImpl::UpdateAppLaunchersFromPref() {
  // There are various functions which will trigger a |SyncPinPosition| call
  // like a direct call to |DoPinAppWithID|, or an indirect call to the menu
  // model which will use weights to re-arrange the icons to new positions.
  // Since this function is meant to synchronize the "is state" with the
  // "sync state", it makes no sense to store any changes by this function back
  // into the pref state. Therefore we tell |persistPinnedState| to ignore any
  // invocations while we are running.
  base::AutoReset<bool> auto_reset(&ignore_persist_pinned_state_change_, true);
  const std::vector<ash::launcher::AppLauncherId> pinned_apps =
      ash::launcher::GetPinnedAppsFromPrefs(profile()->GetPrefs(),
                                            launcher_controller_helper());

  int index = 0;
  // Skip app list items if it exists.
  if (model_->items()[0].type == ash::TYPE_APP_LIST)
    ++index;

  // Apply pins in two steps. At the first step, go through the list of apps to
  // pin, move existing pin to current position specified by |index| or create
  // the new pin at that position.
  for (const auto& pref_app_launcher_id : pinned_apps) {
    // Filter out apps that may be mapped wrongly.
    // TODO(khmel):  b/31703859 is to refactore shelf mapping.
    const std::string app_id = pref_app_launcher_id.app_id();
    const std::string shelf_app_id =
        ArcAppWindowLauncherController::GetShelfAppIdFromArcAppId(app_id);
    if (shelf_app_id != app_id)
      continue;

    // Update apps icon if applicable.
    OnAppUpdated(profile(), app_id);

    // Find existing pin or app from the right of current |index|.
    int app_index = index;
    for (; app_index < model_->item_count(); ++app_index) {
      const ash::ShelfItem& item = model_->items()[app_index];
      const IDToItemControllerMap::iterator it =
          id_to_item_controller_map_.find(item.id);
      if (it != id_to_item_controller_map_.end() &&
          it->second->app_id() == app_id &&
          it->second->launch_id() == pref_app_launcher_id.launch_id()) {
        break;
      }
    }
    if (app_index < model_->item_count()) {
      // Found existing pin or running app.
      const ash::ShelfItem item = model_->items()[app_index];
      if (item.type == ash::TYPE_APP_SHORTCUT ||
          item.type == ash::TYPE_BROWSER_SHORTCUT) {
        // Just move to required position or keep it inplace.
        model_->Move(app_index, index);
      } else {
        PinRunningAppInternal(index, item.id);
      }
      DCHECK_EQ(model_->ItemIndexByID(item.id), index);
    } else {
      // This is fresh pin. Create new one.
      DCHECK_NE(app_id, extension_misc::kChromeAppId);
      CreateAppShortcutLauncherItem(pref_app_launcher_id, index);
    }
    ++index;
  }

  // At second step remove any pin to the right from the current index.
  while (index < model_->item_count()) {
    const ash::ShelfItem item = model_->items()[index];
    if (item.type != ash::TYPE_APP_SHORTCUT) {
      ++index;
      continue;
    }

    const LauncherItemController* controller =
        GetLauncherItemController(item.id);
    DCHECK(controller);
    DCHECK_NE(controller->app_id(), extension_misc::kChromeAppId);

    if (controller->locked() ||
        controller->type() == LauncherItemController::TYPE_APP) {
      UnpinRunningAppInternal(index);
      // Note, item can be moved to the right due weighting in shelf model.
      DCHECK_GE(model_->ItemIndexByID(item.id), index);
    } else {
      LauncherItemClosed(item.id);
    }
  }
}

void ChromeLauncherControllerImpl::SetVirtualKeyboardBehaviorFromPrefs() {
  const PrefService* service = profile()->GetPrefs();
  const bool was_enabled = keyboard::IsKeyboardEnabled();
  if (!service->HasPrefPath(prefs::kTouchVirtualKeyboardEnabled)) {
    keyboard::SetKeyboardShowOverride(keyboard::KEYBOARD_SHOW_OVERRIDE_NONE);
  } else {
    const bool enable =
        service->GetBoolean(prefs::kTouchVirtualKeyboardEnabled);
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

ash::ShelfItemStatus ChromeLauncherControllerImpl::GetAppState(
    const std::string& app_id) {
  ash::ShelfItemStatus status = ash::STATUS_CLOSED;
  for (WebContentsToAppIDMap::iterator it = web_contents_to_app_id_.begin();
       it != web_contents_to_app_id_.end(); ++it) {
    if (it->second == app_id) {
      Browser* browser = chrome::FindBrowserWithWebContents(it->first);
      // Usually there should never be an item in our |web_contents_to_app_id_|
      // list which got deleted already. However - in some situations e.g.
      // Browser::SwapTabContent there is temporarily no associated browser.
      if (!browser)
        continue;
      if (browser->window()->IsActive()) {
        return browser->tab_strip_model()->GetActiveWebContents() == it->first
                   ? ash::STATUS_ACTIVE
                   : ash::STATUS_RUNNING;
      } else {
        status = ash::STATUS_RUNNING;
      }
    }
  }
  return status;
}

ash::ShelfID ChromeLauncherControllerImpl::InsertAppLauncherItem(
    LauncherItemController* controller,
    const std::string& app_id,
    ash::ShelfItemStatus status,
    int index,
    ash::ShelfItemType shelf_item_type) {
  ash::ShelfID id = model_->next_id();
  CHECK(!HasShelfIDToAppIDMapping(id));
  CHECK(controller);
  // Ash's ShelfWindowWatcher handles app panel windows separately.
  DCHECK_NE(ash::TYPE_APP_PANEL, controller->GetShelfItemType());
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

  AppIconLoader* app_icon_loader = GetAppIconLoaderForApp(app_id);
  if (app_icon_loader) {
    app_icon_loader->FetchImage(app_id);
    app_icon_loader->UpdateImage(app_id);
  }

  SetShelfItemDelegate(id, controller);

  return id;
}

std::vector<content::WebContents*>
ChromeLauncherControllerImpl::GetV1ApplicationsFromController(
    LauncherItemController* controller) {
  DCHECK(controller->type() == LauncherItemController::TYPE_SHORTCUT);
  AppShortcutLauncherItemController* app_controller =
      static_cast<AppShortcutLauncherItemController*>(controller);
  return app_controller->GetRunningApplications();
}

void ChromeLauncherControllerImpl::CreateBrowserShortcutLauncherItem() {
  ash::ShelfItem browser_shortcut;
  browser_shortcut.type = ash::TYPE_BROWSER_SHORTCUT;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  browser_shortcut.image = *rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_32);
  ash::ShelfID id = model_->next_id();
  model_->AddAt(0, browser_shortcut);
  id_to_item_controller_map_[id] =
      new BrowserShortcutLauncherItemController(this, model_);
  id_to_item_controller_map_[id]->set_shelf_id(id);
  // ShelfModel owns BrowserShortcutLauncherItemController.
  SetShelfItemDelegate(id, id_to_item_controller_map_[id]);
}

bool ChromeLauncherControllerImpl::IsIncognito(
    const content::WebContents* web_contents) const {
  const Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return profile->IsOffTheRecord() && !profile->IsGuestSession() &&
         !profile->IsSystemProfile();
}

int ChromeLauncherControllerImpl::FindInsertionPoint() {
  DCHECK_GT(model_->item_count(), 0);
  for (int i = model_->item_count() - 1; i > 0; --i) {
    ash::ShelfItemType type = model_->items()[i].type;
    DCHECK_NE(ash::TYPE_APP_LIST, type);
    if (type == ash::TYPE_APP_SHORTCUT ||
        type == ash::TYPE_BROWSER_SHORTCUT) {
      return i;
    }
  }
  return 0;
}

void ChromeLauncherControllerImpl::CloseWindowedAppsFromRemovedExtension(
    const std::string& app_id,
    const Profile* profile) {
  // This function cannot rely on the controller's enumeration functionality
  // since the extension has already been unloaded.
  const BrowserList* browser_list = BrowserList::GetInstance();
  std::vector<Browser*> browser_to_close;
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_last_active();
       it != browser_list->end_last_active(); ++it) {
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
    if (!tab_strip->empty())
      tab_strip->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
    browser_to_close.pop_back();
  }
}

void ChromeLauncherControllerImpl::SetShelfItemDelegate(
    ash::ShelfID id,
    ash::ShelfItemDelegate* item_delegate) {
  DCHECK_GT(id, 0);
  DCHECK(item_delegate);
  model_->SetShelfItemDelegate(
      id, std::unique_ptr<ash::ShelfItemDelegate>(item_delegate));
}

void ChromeLauncherControllerImpl::ReleaseProfile() {
  if (app_sync_ui_state_)
    app_sync_ui_state_->RemoveObserver(this);

  app_updaters_.clear();

  prefs_observer_.reset();

  pref_change_registrar_.RemoveAll();

  app_list::AppListSyncableService* app_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  if (app_service)
    app_service->RemoveObserver(this);
}

///////////////////////////////////////////////////////////////////////////////
// ash::ShelfModelObserver:

void ChromeLauncherControllerImpl::ShelfItemAdded(int index) {}

void ChromeLauncherControllerImpl::ShelfItemRemoved(int index,
                                                    ash::ShelfID id) {
  // TODO(skuhne): This fixes crbug.com/429870, but it does not answer why we
  // get into this state in the first place.
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  if (iter == id_to_item_controller_map_.end())
    return;

  LOG(ERROR) << "Unexpected removal of shelf item, id: " << id;
  id_to_item_controller_map_.erase(iter);
}

void ChromeLauncherControllerImpl::ShelfItemMoved(int start_index,
                                                  int target_index) {
  const ash::ShelfItem& item = model_->items()[target_index];
  // We remember the moved item position if it is either pinnable or
  // it is the app list with the alternate shelf layout.
  DCHECK_NE(ash::TYPE_APP_LIST, item.type);
  if (HasShelfIDToAppIDMapping(item.id) && IsPinned(item.id))
    SyncPinPosition(item.id);
}

void ChromeLauncherControllerImpl::ShelfItemChanged(
    int index,
    const ash::ShelfItem& old_item) {}

void ChromeLauncherControllerImpl::OnSetShelfItemDelegate(
    ash::ShelfID id,
    ash::ShelfItemDelegate* item_delegate) {
  // TODO(skuhne): This fixes crbug.com/429870, but it does not answer why we
  // get into this state in the first place.
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  if (iter == id_to_item_controller_map_.end() || item_delegate == iter->second)
    return;
  LOG(ERROR) << "Unexpected change of shelf item delegate, id: " << id;
  id_to_item_controller_map_.erase(iter);
}

///////////////////////////////////////////////////////////////////////////////
// ash::WindowTreeHostManager::Observer:

void ChromeLauncherControllerImpl::OnDisplayConfigurationChanged() {
  // In BOTTOM_LOCKED state, ignore the call of SetShelfBehaviorsFromPrefs.
  // Because it might be called by some operations, like crbug.com/627040
  // rotating screen.
  ash::WmShelf* shelf =
      ash::WmShelf::ForWindow(ash::WmShell::Get()->GetPrimaryRootWindow());
  if (shelf->alignment() != ash::SHELF_ALIGNMENT_BOTTOM_LOCKED)
    SetShelfBehaviorsFromPrefs();
}

///////////////////////////////////////////////////////////////////////////////
// AppSyncUIStateObserver:

void ChromeLauncherControllerImpl::OnAppSyncUIStatusChanged() {
  if (app_sync_ui_state_->status() == AppSyncUIState::STATUS_SYNCING)
    model_->set_status(ash::ShelfModel::STATUS_LOADING);
  else
    model_->set_status(ash::ShelfModel::STATUS_NORMAL);
}

///////////////////////////////////////////////////////////////////////////////
// AppIconLoaderDelegate:

void ChromeLauncherControllerImpl::OnAppImageUpdated(
    const std::string& id,
    const gfx::ImageSkia& image) {
  // TODO: need to get this working for shortcuts.
  for (int index = 0; index < model_->item_count(); ++index) {
    ash::ShelfItem item = model_->items()[index];
    if (GetAppIDForShelfID(item.id) != id)
      continue;
    LauncherItemController* controller = GetLauncherItemController(item.id);
    if (!controller || controller->image_set_by_controller())
      continue;
    item.image = image;
    if (arc_deferred_launcher_)
      arc_deferred_launcher_->MaybeApplySpinningEffect(id, &item.image);
    model_->Set(index, item);
    // It's possible we're waiting on more than one item, so don't break.
  }
}
