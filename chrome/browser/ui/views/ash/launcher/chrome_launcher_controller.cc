// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h"

#include <set>
#include <vector>

#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/ash/extension_utils.h"
#include "chrome/browser/ui/views/ash/launcher/browser_launcher_item_controller.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_app_icon_loader.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

using extensions::Extension;

// ChromeLauncherController::Item ----------------------------------------------

ChromeLauncherController::Item::Item()
    : item_type(TYPE_TABBED_BROWSER),
      controller(NULL) {
}

ChromeLauncherController::Item::~Item() {
}

// ChromeLauncherController ----------------------------------------------------

// static
ChromeLauncherController* ChromeLauncherController::instance_ = NULL;

ChromeLauncherController::ChromeLauncherController(Profile* profile,
                                                   ash::LauncherModel* model)
    : model_(model),
      profile_(profile),
      activation_client_(NULL) {
  if (!profile_) {
    // Use the original profile as on chromeos we may get a temporary off the
    // record profile.
    profile_ = ProfileManager::GetDefaultProfile()->GetOriginalProfile();
  }
  instance_ = this;
  model_->AddObserver(this);
  ShellWindowRegistry::Get(profile_)->AddObserver(this);
  app_icon_loader_.reset(new LauncherAppIconLoader(profile_, this));

  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_EXTENSION_LOADED,
                              content::Source<Profile>(profile_));
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_EXTENSION_UNLOADED,
                              content::Source<Profile>(profile_));
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kPinnedLauncherApps, this);
}

ChromeLauncherController::~ChromeLauncherController() {
  ShellWindowRegistry::Get(profile_)->RemoveObserver(this);
  model_->RemoveObserver(this);
  for (IDToItemMap::iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    model_->RemoveItemAt(model_->ItemIndexByID(i->first));
  }
  if (instance_ == this)
    instance_ = NULL;
  if (activation_client_)
    activation_client_->RemoveObserver(this);

  for (WindowToIDMap::iterator i = window_to_id_map_.begin();
       i != window_to_id_map_.end(); ++i) {
    i->first->RemoveObserver(this);
  }
}

void ChromeLauncherController::Init() {
  // TODO(xiyuan): Remove migration code and kUseDefaultPinnedApp after M20.
  // Migration cases:
  // - Users that unpin all apps:
  //   - have default pinned apps
  //   - kUseDefaultPinnedApps set to false
  //   Migrate them by setting an empty list for kPinnedLauncherApps.
  //
  // - Users that have customized pinned apps:
  //   - have non-default non-empty pinned apps list
  //   - kUseDefaultPinnedApps set to false
  //   Nothing needs to be done because customized pref overrides default.
  //
  // - Users that have default apps (i.e. new user or never pin/unpin):
  //   - have default pinned apps
  //   - kUseDefaultPinnedApps is still true
  //   Nothing needs to be done because they should get the default.
  if (profile_->GetPrefs()->FindPreference(
          prefs::kPinnedLauncherApps)->IsDefaultValue() &&
      !profile_->GetPrefs()->GetBoolean(prefs::kUseDefaultPinnedApps)) {
    ListPrefUpdate updater(profile_->GetPrefs(), prefs::kPinnedLauncherApps);
    updater.Get()->Clear();
  }

  UpdateAppLaunchersFromPref();

  // TODO(sky): update unit test so that this test isn't necessary.
  if (ash::Shell::HasInstance()) {
    std::string behavior_value(
        profile_->GetPrefs()->GetString(prefs::kShelfAutoHideBehavior));
    ash::ShelfAutoHideBehavior behavior =
        ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT;
    if (behavior_value == ash::kShelfAutoHideBehaviorNever)
      behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
    else if (behavior_value == ash::kShelfAutoHideBehaviorAlways)
      behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
    ash::Shell::GetInstance()->SetShelfAutoHideBehavior(behavior);
    activation_client_ =
        aura::client::GetActivationClient(
            ash::Shell::GetInstance()->GetPrimaryRootWindow());
    activation_client_->AddObserver(this);
  }
}

ash::LauncherID ChromeLauncherController::CreateTabbedLauncherItem(
    BrowserLauncherItemController* controller,
    IncognitoState is_incognito,
    ash::LauncherItemStatus status) {
  ash::LauncherID id = model_->next_id();
  DCHECK(id_to_item_map_.find(id) == id_to_item_map_.end());
  id_to_item_map_[id].item_type = TYPE_TABBED_BROWSER;
  id_to_item_map_[id].controller = controller;

  ash::LauncherItem item;
  item.type = ash::TYPE_TABBED;
  item.is_incognito = (is_incognito == STATE_INCOGNITO);
  item.status = status;
  model_->Add(item);
  return id;
}

ash::LauncherID ChromeLauncherController::CreateAppLauncherItem(
    BrowserLauncherItemController* controller,
    const std::string& app_id,
    ash::LauncherItemStatus status) {
  return InsertAppLauncherItem(controller, app_id, status,
                               model_->item_count());
}

void ChromeLauncherController::SetItemStatus(ash::LauncherID id,
                                             ash::LauncherItemStatus status) {
  int index = model_->ItemIndexByID(id);
  DCHECK_GE(index, 0);
  ash::LauncherItem item = model_->items()[index];
  item.status = status;
  model_->Set(index, item);
}

void ChromeLauncherController::LauncherItemClosed(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  id_to_item_map_.erase(id);
  model_->RemoveItemAt(model_->ItemIndexByID(id));
}

void ChromeLauncherController::Unpin(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  DCHECK(!id_to_item_map_[id].controller);
  LauncherItemClosed(id);
  if (CanPin())
    PersistPinnedState();
}

bool ChromeLauncherController::IsPinned(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  return id_to_item_map_[id].is_pinned();
}

void ChromeLauncherController::TogglePinned(ash::LauncherID id) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // May happen if item closed with menu open.

  // Only currently support unpinning.
  if (IsPinned(id))
    Unpin(id);
}

bool ChromeLauncherController::IsPinnable(ash::LauncherID id) const {
  int index = model_->ItemIndexByID(id);
  return (index != -1 &&
          model_->items()[index].type == ash::TYPE_APP_SHORTCUT &&
          CanPin());
}

void ChromeLauncherController::Open(ash::LauncherID id, int event_flags) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // In case invoked from menu and item closed while menu up.

  BrowserLauncherItemController* controller = id_to_item_map_[id].controller;
  if (controller) {
    controller->window()->Show();
    ash::wm::ActivateWindow(controller->window());
  } else {
    DCHECK_EQ(TYPE_APP, id_to_item_map_[id].item_type);

    // Do nothing for pending app shortcut.
    if (GetItemStatus(id) == ash::STATUS_IS_PENDING)
      return;

    const Extension* extension =
        profile_->GetExtensionService()->GetInstalledExtension(
            id_to_item_map_[id].app_id);
    extension_utils::OpenExtension(profile_, extension, event_flags);
  }
}

void ChromeLauncherController::Close(ash::LauncherID id) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // May happen if menu closed.

  if (!id_to_item_map_[id].controller)
    return;  // TODO: maybe should treat as unpin?

  views::Widget* widget = views::Widget::GetWidgetForNativeView(
      id_to_item_map_[id].controller->window());
  if (widget)
    widget->Close();
}

bool ChromeLauncherController::IsOpen(ash::LauncherID id) {
  return id_to_item_map_.find(id) != id_to_item_map_.end() &&
      id_to_item_map_[id].controller != NULL;
}

ExtensionPrefs::LaunchType ChromeLauncherController::GetLaunchType(
    ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());

  return profile_->GetExtensionService()->extension_prefs()->GetLaunchType(
      id_to_item_map_[id].app_id, ExtensionPrefs::LAUNCH_DEFAULT);
}

std::string ChromeLauncherController::GetAppID(TabContentsWrapper* tab) {
  return app_icon_loader_->GetAppID(tab);
}

void ChromeLauncherController::SetAppImage(const std::string& id,
                                           const SkBitmap* image) {
  // TODO: need to get this working for shortcuts.

  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (i->second.app_id != id)
      continue;

    // Panel items may share the same app_id as the app that created them,
    // but they set their icon image in
    // BrowserLauncherItemController::UpdateLauncher(), so do not set panel
    // images here.
    if (i->second.controller &&
        i->second.controller->type() ==
        BrowserLauncherItemController::TYPE_EXTENSION_PANEL) {
      continue;
    }

    int index = model_->ItemIndexByID(i->first);
    ash::LauncherItem item = model_->items()[index];
    item.image = image ? *image : Extension::GetDefaultIcon(true);
    model_->Set(index, item);
    // It's possible we're waiting on more than one item, so don't break.
  }
}

bool ChromeLauncherController::IsAppPinned(const std::string& app_id) {
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (IsPinned(i->first) && i->second.app_id == app_id)
      return true;
  }
  return false;
}

void ChromeLauncherController::PinAppWithID(const std::string& app_id) {
  if (CanPin())
    DoPinAppWithID(app_id);
  else
    NOTREACHED();
}

void ChromeLauncherController::SetLaunchType(
    ash::LauncherID id,
    ExtensionPrefs::LaunchType launch_type) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;

  return profile_->GetExtensionService()->extension_prefs()->SetLaunchType(
      id_to_item_map_[id].app_id, launch_type);
}

void ChromeLauncherController::UnpinAppsWithID(const std::string& app_id) {
  if (CanPin())
    DoUnpinAppsWithID(app_id);
  else
    NOTREACHED();
}

bool ChromeLauncherController::IsLoggedInAsGuest() {
  return ProfileManager::GetDefaultProfileOrOffTheRecord()->IsOffTheRecord();
}

void ChromeLauncherController::CreateNewIncognitoWindow() {
  Browser::NewEmptyWindow(GetProfileForNewWindows()->GetOffTheRecordProfile());
}

bool ChromeLauncherController::CanPin() const {
  const PrefService::Preference* pref =
      profile_->GetPrefs()->FindPreference(prefs::kPinnedLauncherApps);
  return pref && pref->IsUserModifiable();
}

void ChromeLauncherController::SetAutoHideBehavior(
    ash::ShelfAutoHideBehavior behavior) {
  ash::Shell::GetInstance()->SetShelfAutoHideBehavior(behavior);
  const char* value = NULL;
  switch (behavior) {
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT:
      value = ash::kShelfAutoHideBehaviorDefault;
      break;
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      value = ash::kShelfAutoHideBehaviorAlways;
      break;
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      value = ash::kShelfAutoHideBehaviorNever;
      break;
  }
  profile_->GetPrefs()->SetString(prefs::kShelfAutoHideBehavior, value);
}

void ChromeLauncherController::CreateNewTab() {
  Browser* last_browser = browser::FindTabbedBrowser(
      GetProfileForNewWindows(), true);

  if (!last_browser) {
    CreateNewWindow();
    return;
  }

  last_browser->NewTab();
  aura::Window* window = last_browser->window()->GetNativeWindow();
  window->Show();
  ash::wm::ActivateWindow(window);
}

void ChromeLauncherController::CreateNewWindow() {
  Browser::NewEmptyWindow(GetProfileForNewWindows());
}

void ChromeLauncherController::ItemClicked(const ash::LauncherItem& item,
                                           int event_flags) {
  DCHECK(id_to_item_map_.find(item.id) != id_to_item_map_.end());
  BrowserLauncherItemController* controller =
      id_to_item_map_[item.id].controller;
  if (controller) {
    views::Widget* widget =
        views::Widget::GetWidgetForNativeView(controller->window());
    if (widget && widget->IsActive()) {
      widget->Minimize();
      return;
    }
    // else case, fall through to show window.
  }
  Open(item.id, event_flags);
}

int ChromeLauncherController::GetBrowserShortcutResourceId() {
  return IDR_PRODUCT_LOGO_32;
}

string16 ChromeLauncherController::GetTitle(const ash::LauncherItem& item) {
  DCHECK(id_to_item_map_.find(item.id) != id_to_item_map_.end());
  BrowserLauncherItemController* controller =
      id_to_item_map_[item.id].controller;
  if (controller) {
    if (id_to_item_map_[item.id].item_type == TYPE_TABBED_BROWSER) {
      return controller->tab_model()->GetActiveTabContents() ?
          controller->tab_model()->GetActiveTabContents()->web_contents()->
          GetTitle() : string16();
    }
    // Fall through to get title from extension.
  }
  const Extension* extension = profile_->GetExtensionService()->
      GetInstalledExtension(id_to_item_map_[item.id].app_id);
  return extension ? UTF8ToUTF16(extension->name()) : string16();
}

ui::MenuModel* ChromeLauncherController::CreateContextMenu(
    const ash::LauncherItem& item) {
  return new LauncherContextMenu(this, &item);
}

ui::MenuModel* ChromeLauncherController::CreateContextMenuForLauncher() {
  return new LauncherContextMenu(this, NULL);
}

ash::LauncherID ChromeLauncherController::GetIDByWindow(
    aura::Window* window) {
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (i->second.controller && i->second.controller->window() == window)
      return i->first;
  }
  return 0;
}

bool ChromeLauncherController::IsDraggable(const ash::LauncherItem& item) {
  return item.type == ash::TYPE_APP_SHORTCUT ? CanPin() : true;
}

void ChromeLauncherController::LauncherItemAdded(int index) {
}

void ChromeLauncherController::LauncherItemRemoved(int index,
                                                   ash::LauncherID id) {
}

void ChromeLauncherController::LauncherItemMoved(
    int start_index,
    int target_index) {
  ash::LauncherID id = model_->items()[target_index].id;
  if (id_to_item_map_.find(id) != id_to_item_map_.end() && IsPinned(id))
    PersistPinnedState();
}

void ChromeLauncherController::LauncherItemChanged(
    int index,
    const ash::LauncherItem& old_item) {
  if (model_->items()[index].status == ash::STATUS_ACTIVE &&
      old_item.status == ash::STATUS_RUNNING) {
    ash::LauncherID id = model_->items()[index].id;
    if (id_to_item_map_[id].controller) {
      aura::Window* window_to_activate =
          id_to_item_map_[id].controller->window();
      if (window_to_activate && ash::wm::IsActiveWindow(window_to_activate))
        return;
      window_to_activate->Show();
      ash::wm::ActivateWindow(window_to_activate);
    }
  }
}

void ChromeLauncherController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      UpdateAppLaunchersFromPref();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const content::Details<extensions::UnloadedExtensionInfo> unload_info(
          details);
      const Extension* extension = unload_info->extension;
      if (IsAppPinned(extension->id())) {
        if (unload_info->reason == extension_misc::UNLOAD_REASON_UNINSTALL)
          DoUnpinAppsWithID(extension->id());
        else
          MarkAppPending(extension->id());
      }
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string& pref_name(
          *content::Details<std::string>(details).ptr());
      if (pref_name == prefs::kPinnedLauncherApps) {
        UpdateAppLaunchersFromPref();
      } else {
        NOTREACHED() << "Unexpected pref change for " << pref_name;
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type=" << type;
  }
}

void ChromeLauncherController::OnShellWindowAdded(ShellWindow* shell_window) {
  aura::Window* window = shell_window->GetNativeWindow();
  ash::LauncherItemStatus status =
      ash::wm::IsActiveWindow(window) ?
          ash::STATUS_ACTIVE : ash::STATUS_RUNNING;
  window->AddObserver(this);
  const std::string app_id = shell_window->extension()->id();
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (i->second.app_id == app_id) {
      window_to_id_map_[window] = i->first;
      SetItemStatus(i->first, status);
      return;
    }
  }
  ash::LauncherID id = CreateAppLauncherItem(NULL, app_id, status);
  window_to_id_map_[window] = id;
}

void ChromeLauncherController::OnShellWindowRemoved(ShellWindow* shell_window) {
  // Window removal is handled in OnWindowRemovingFromRootWindow() below.
}

void ChromeLauncherController::OnWindowActivated(aura::Window* active,
                                                 aura::Window* old_active) {
  if (window_to_id_map_.find(old_active) != window_to_id_map_.end()) {
    if (window_to_id_map_.find(active) != window_to_id_map_.end() &&
        window_to_id_map_[old_active] == window_to_id_map_[active]) {
      // Old and new windows are for the same item. Don't change the status.
      return;
    }
    SetItemStatus(window_to_id_map_[old_active], ash::STATUS_RUNNING);
  }
  if (window_to_id_map_.find(active) != window_to_id_map_.end())
    SetItemStatus(window_to_id_map_[active], ash::STATUS_ACTIVE);
}

void ChromeLauncherController::OnWindowRemovingFromRootWindow(
    aura::Window* window) {
  window->RemoveObserver(this);
  DCHECK(window_to_id_map_.find(window) != window_to_id_map_.end());
  ash::LauncherID id = window_to_id_map_[window];
  window_to_id_map_.erase(window);

  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  ShellWindowRegistry::ShellWindowSet remaining_windows =
      ShellWindowRegistry::Get(profile_)->GetShellWindowsForApp(
      id_to_item_map_[id].app_id);

  // We can't count on getting called before or after the ShellWindowRegistry.
  if (remaining_windows.size() > 1 ||
      (remaining_windows.size() == 1 &&
          (*remaining_windows.begin())->GetNativeWindow() != window)) {
    return;
  }

  // Close or remove item.
  int index = model_->ItemIndexByID(id);
  DCHECK_GE(index, 0);
  ash::LauncherItem item = model_->items()[index];
  if (item.type == ash::TYPE_APP_SHORTCUT)
    SetItemStatus(id, ash::STATUS_CLOSED);
  else
    LauncherItemClosed(id);
}

void ChromeLauncherController::PersistPinnedState() {
  // It is a coding error to call PersistPinnedState() if the pinned apps are
  // not user-editable. The code should check earlier and not perform any
  // modification actions that trigger persisting the state.
  if (!CanPin()) {
    NOTREACHED() << "Can't pin but pinned state being updated";
    return;
  }

  // Set kUseDefaultPinnedApps to false and use pinned apps list from prefs
  // from now on.
  profile_->GetPrefs()->SetBoolean(prefs::kUseDefaultPinnedApps, false);

  // Mutating kPinnedLauncherApps is going to notify us and trigger us to
  // process the change. We don't want that to happen so remove ourselves as a
  // listener.
  pref_change_registrar_.Remove(prefs::kPinnedLauncherApps, this);
  {
    ListPrefUpdate updater(profile_->GetPrefs(), prefs::kPinnedLauncherApps);
    updater->Clear();
    for (size_t i = 0; i < model_->items().size(); ++i) {
      if (model_->items()[i].type == ash::TYPE_APP_SHORTCUT) {
        ash::LauncherID id = model_->items()[i].id;
        if (id_to_item_map_.find(id) != id_to_item_map_.end() &&
            IsPinned(id)) {
          base::DictionaryValue* app_value = ash::CreateAppDict(
              id_to_item_map_[id].app_id);
          if (app_value)
            updater->Append(app_value);
        }
      }
    }
  }
  pref_change_registrar_.Add(prefs::kPinnedLauncherApps, this);
}

void ChromeLauncherController::SetAppIconLoaderForTest(AppIconLoader* loader) {
  app_icon_loader_.reset(loader);
}

Profile* ChromeLauncherController::GetProfileForNewWindows() {
  return ProfileManager::GetDefaultProfileOrOffTheRecord();
}

ash::LauncherItemStatus ChromeLauncherController::GetItemStatus(
    ash::LauncherID id) const {
  int index = model_->ItemIndexByID(id);
  DCHECK_GE(index, 0);
  const ash::LauncherItem& item = model_->items()[index];
  return item.status;
}

void ChromeLauncherController::MarkAppPending(const std::string& app_id) {
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (i->second.item_type == TYPE_APP && i->second.app_id == app_id) {
      if (GetItemStatus(i->first) == ash::STATUS_CLOSED)
        SetItemStatus(i->first, ash::STATUS_IS_PENDING);

      break;
    }
  }
}

void ChromeLauncherController::DoPinAppWithID(const std::string& app_id) {
  // If there is an item, do nothing and return.
  if (IsAppPinned(app_id))
    return;

  // Otherwise, create an item for it.
  CreateAppLauncherItem(NULL, app_id, ash::STATUS_CLOSED);
  if (CanPin())
    PersistPinnedState();
}

void ChromeLauncherController::DoUnpinAppsWithID(const std::string& app_id) {
  for (IDToItemMap::iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ) {
    IDToItemMap::iterator current(i);
    ++i;
    if (current->second.app_id == app_id && IsPinned(current->first))
      Unpin(current->first);
  }
}

void ChromeLauncherController::UpdateAppLaunchersFromPref() {
  // Construct a vector representation of to-be-pinned apps from the pref.
  std::vector<std::string> pinned_apps;
  const base::ListValue* pinned_apps_pref =
      profile_->GetPrefs()->GetList(prefs::kPinnedLauncherApps);
  for (base::ListValue::const_iterator it(pinned_apps_pref->begin());
       it != pinned_apps_pref->end(); ++it) {
    DictionaryValue* app = NULL;
    std::string app_id;
    if ((*it)->GetAsDictionary(&app) &&
        app->GetString(ash::kPinnedAppsPrefAppIDPath, &app_id) &&
        std::find(pinned_apps.begin(), pinned_apps.end(), app_id) ==
            pinned_apps.end() &&
        app_icon_loader_->IsValidID(app_id)) {
      pinned_apps.push_back(app_id);
    }
  }

  // Walk the model and |pinned_apps| from the pref lockstep, adding and
  // removing items as necessary. NB: This code uses plain old indexing instead
  // of iterators because of model mutations as part of the loop.
  std::vector<std::string>::const_iterator pref_app_id(pinned_apps.begin());
  int index = 0;
  for (; index < model_->item_count() && pref_app_id != pinned_apps.end();
       ++index) {
    // If the next app launcher according to the pref is present in the model,
    // delete all app launcher entries in between.
    if (IsAppPinned(*pref_app_id)) {
      for (; index < model_->item_count(); ++index) {
        const ash::LauncherItem& item(model_->items()[index]);
        if (item.type != ash::TYPE_APP_SHORTCUT)
          continue;

        IDToItemMap::const_iterator entry(id_to_item_map_.find(item.id));
        if (entry != id_to_item_map_.end() &&
            entry->second.app_id == *pref_app_id) {
          // Current item will be kept. Reset its pending state and ensure
          // its icon is loaded since it has to be valid to be in |pinned_apps|.
          if (item.status == ash::STATUS_IS_PENDING) {
            SetItemStatus(item.id, ash::STATUS_CLOSED);
            app_icon_loader_->FetchImage(*pref_app_id);
          }

          ++pref_app_id;
          break;
        } else {
          LauncherItemClosed(item.id);
          --index;
        }
      }
      // If the item wasn't found, that means id_to_item_map_ is out of sync.
      DCHECK(index < model_->item_count());
    } else {
      // This app wasn't pinned before, insert a new entry.
      ash::LauncherID id = InsertAppLauncherItem(NULL, *pref_app_id,
                                                 ash::STATUS_CLOSED, index);
      index = model_->ItemIndexByID(id);
      ++pref_app_id;
    }
  }

  // Remove any trailing existing items.
  while (index < model_->item_count()) {
    const ash::LauncherItem& item(model_->items()[index]);
    if (item.type == ash::TYPE_APP_SHORTCUT)
      LauncherItemClosed(item.id);
    else
      ++index;
  }

  // Append unprocessed items from the pref to the end of the model.
  for (; pref_app_id != pinned_apps.end(); ++pref_app_id)
    CreateAppLauncherItem(NULL, *pref_app_id, ash::STATUS_CLOSED);
}

ash::LauncherID ChromeLauncherController::InsertAppLauncherItem(
    BrowserLauncherItemController* controller,
    const std::string& app_id,
    ash::LauncherItemStatus status,
    int index) {
  ash::LauncherID id = model_->next_id();
  DCHECK(id_to_item_map_.find(id) == id_to_item_map_.end());
  id_to_item_map_[id].item_type = TYPE_APP;
  id_to_item_map_[id].app_id = app_id;
  id_to_item_map_[id].controller = controller;

  ash::LauncherItem item;
  if (!controller) {
    if (status == ash::STATUS_CLOSED)
      item.type = ash::TYPE_APP_SHORTCUT;
    else
      item.type = ash::TYPE_PLATFORM_APP;
  } else if (controller->type() ==
             BrowserLauncherItemController::TYPE_APP_PANEL ||
             controller->type() ==
             BrowserLauncherItemController::TYPE_EXTENSION_PANEL) {
    item.type = ash::TYPE_APP_PANEL;
  } else {
    item.type = ash::TYPE_TABBED;
  }
  item.is_incognito = false;
  item.image = Extension::GetDefaultIcon(true);
  if (item.type == ash::TYPE_APP_SHORTCUT &&
      !app_icon_loader_->IsValidID(app_id)) {
    item.status = ash::STATUS_IS_PENDING;
  } else {
    item.status = status;
  }
  model_->AddAt(index, item);

  if (!controller || controller->type() !=
      BrowserLauncherItemController::TYPE_EXTENSION_PANEL) {
    if (item.status != ash::STATUS_IS_PENDING)
      app_icon_loader_->FetchImage(app_id);
  }
  return id;
}
