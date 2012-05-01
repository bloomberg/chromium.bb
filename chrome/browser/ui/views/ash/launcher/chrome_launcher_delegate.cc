// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"

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
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_app_icon_loader.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/views/ash/launcher/launcher_updater.h"
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

namespace {

// See description in PersistPinnedState().
const char kAppIDPath[] = "id";

// Values used for prefs::kShelfAutoHideBehavior.
const char kShelfAutoHideBehaviorAlways[] = "Always";
const char kShelfAutoHideBehaviorDefault[] = "Default";
const char kShelfAutoHideBehaviorNever[] = "Never";

// App ID of default pinned apps.
const char* kDefaultPinnedApps[] = {
  "pjkljhegncpnkpknbcohdijeoejaedia",  // Gamil
  "coobgpohoikkiipiblmjeljniedjpjpf",  // Search
  "apdfllckaahabafndbhieahigkjlhalf",  // Doc
  "blpcfgokakmgnkcojhhkbfbldkacnbeo",  // YouTube
};

base::DictionaryValue* CreateAppDict(const std::string& app_id) {
  scoped_ptr<base::DictionaryValue> app_value(new base::DictionaryValue);
  app_value->SetString(kAppIDPath, app_id);
  return app_value.release();
}

base::ListValue* CreateDefaultPinnedAppsList() {
  scoped_ptr<base::ListValue> apps(new base::ListValue);
  for (size_t i = 0; i < arraysize(kDefaultPinnedApps); ++i)
    apps->Append(CreateAppDict(kDefaultPinnedApps[i]));

  return apps.release();
}

}  // namespace

// ChromeLauncherDelegate::Item ------------------------------------------------

ChromeLauncherDelegate::Item::Item()
    : item_type(TYPE_TABBED_BROWSER),
      updater(NULL) {
}

ChromeLauncherDelegate::Item::~Item() {
}

// ChromeLauncherDelegate ------------------------------------------------------

// static
ChromeLauncherDelegate* ChromeLauncherDelegate::instance_ = NULL;

ChromeLauncherDelegate::ChromeLauncherDelegate(Profile* profile,
                                               ash::LauncherModel* model)
    : model_(model),
      profile_(profile) {
  if (!profile_) {
    // Use the original profile as on chromeos we may get a temporary off the
    // record profile.
    profile_ = ProfileManager::GetDefaultProfile()->GetOriginalProfile();
  }
  instance_ = this;
  model_->AddObserver(this);
  app_icon_loader_.reset(new LauncherAppIconLoader(profile_, this));

  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

ChromeLauncherDelegate::~ChromeLauncherDelegate() {
  model_->RemoveObserver(this);
  for (IDToItemMap::iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    model_->RemoveItemAt(model_->ItemIndexByID(i->first));
  }
  if (instance_ == this)
    instance_ = NULL;
}

void ChromeLauncherDelegate::Init() {
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

  const base::ListValue* pinned_apps =
      profile_->GetPrefs()->GetList(prefs::kPinnedLauncherApps);
  for (size_t i = 0; i < pinned_apps->GetSize(); ++i) {
    DictionaryValue* app = NULL;
    if (pinned_apps->GetDictionary(i, &app)) {
      std::string app_id;
      if (app->GetString(kAppIDPath, &app_id)) {
        if (app_icon_loader_->IsValidID(app_id)) {
          CreateAppLauncherItem(NULL, app_id, ash::STATUS_CLOSED);
        } else {
          Item pending_item;
          pending_item.item_type = TYPE_APP;
          pending_item.app_id = app_id;
          pending_pinned_apps_.push_back(pending_item);
        }
      }
    }
  }
  // TODO(sky): update unit test so that this test isn't necessary.
  if (ash::Shell::HasInstance()) {
    std::string behavior_value(
        profile_->GetPrefs()->GetString(prefs::kShelfAutoHideBehavior));
    ash::ShelfAutoHideBehavior behavior =
        ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT;
    if (behavior_value == kShelfAutoHideBehaviorNever)
      behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
    else if (behavior_value == kShelfAutoHideBehaviorAlways)
      behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
    ash::Shell::GetInstance()->SetShelfAutoHideBehavior(behavior);
  }
}

// static
void ChromeLauncherDelegate::RegisterUserPrefs(PrefService* user_prefs) {
  // TODO: If we want to support multiple profiles this will likely need to be
  // pushed to local state and we'll need to track profile per item.
  user_prefs->RegisterBooleanPref(prefs::kUseDefaultPinnedApps,
                                  true,
                                  PrefService::SYNCABLE_PREF);
  user_prefs->RegisterListPref(prefs::kPinnedLauncherApps,
                               CreateDefaultPinnedAppsList(),
                               PrefService::SYNCABLE_PREF);
  user_prefs->RegisterStringPref(prefs::kShelfAutoHideBehavior,
                                 kShelfAutoHideBehaviorDefault,
                                 PrefService::SYNCABLE_PREF);
}

ash::LauncherID ChromeLauncherDelegate::CreateTabbedLauncherItem(
    LauncherUpdater* updater,
    IncognitoState is_incognito,
    ash::LauncherItemStatus status) {
  ash::LauncherID id = model_->next_id();
  ash::LauncherItem item;
  item.type = ash::TYPE_TABBED;
  item.is_incognito = (is_incognito == STATE_INCOGNITO);
  item.status = status;
  model_->Add(item);
  DCHECK(id_to_item_map_.find(id) == id_to_item_map_.end());
  id_to_item_map_[id].item_type = TYPE_TABBED_BROWSER;
  id_to_item_map_[id].updater = updater;
  return id;
}

ash::LauncherID ChromeLauncherDelegate::CreateAppLauncherItem(
    LauncherUpdater* updater,
    const std::string& app_id,
    ash::LauncherItemStatus status) {
  ash::LauncherID id = model_->next_id();
  ash::LauncherItem item;
  if (!updater) {
    item.type = ash::TYPE_APP_SHORTCUT;
  } else if (updater->type() == LauncherUpdater::TYPE_APP_PANEL ||
             updater->type() == LauncherUpdater::TYPE_EXTENSION_PANEL) {
    item.type = ash::TYPE_APP_PANEL;
  } else {
    item.type = ash::TYPE_TABBED;
  }
  item.is_incognito = false;
  item.image = Extension::GetDefaultIcon(true);
  item.status = status;
  model_->Add(item);
  DCHECK(id_to_item_map_.find(id) == id_to_item_map_.end());
  id_to_item_map_[id].item_type = TYPE_APP;
  id_to_item_map_[id].app_id = app_id;
  id_to_item_map_[id].updater = updater;

  if (!updater || updater->type() != LauncherUpdater::TYPE_EXTENSION_PANEL)
    app_icon_loader_->FetchImage(app_id);
  return id;
}

void ChromeLauncherDelegate::SetItemStatus(ash::LauncherID id,
                                           ash::LauncherItemStatus status) {
  int index = model_->ItemIndexByID(id);
  DCHECK_GE(index, 0);
  ash::LauncherItem item = model_->items()[index];
  item.status = status;
  model_->Set(index, item);
}

void ChromeLauncherDelegate::LauncherItemClosed(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  id_to_item_map_.erase(id);
  model_->RemoveItemAt(model_->ItemIndexByID(id));
}

void ChromeLauncherDelegate::Unpin(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  DCHECK(!id_to_item_map_[id].updater);
  LauncherItemClosed(id);
  PersistPinnedState();
}

bool ChromeLauncherDelegate::IsPinned(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  return id_to_item_map_[id].is_pinned();
}

void ChromeLauncherDelegate::TogglePinned(ash::LauncherID id) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // May happen if item closed with menu open.

  // Only currently support unpinning.
  if (IsPinned(id))
    Unpin(id);
}

bool ChromeLauncherDelegate::IsPinnable(ash::LauncherID id) const {
  int index = model_->ItemIndexByID(id);
  return index != -1 && model_->items()[index].type == ash::TYPE_APP_SHORTCUT;
}

void ChromeLauncherDelegate::Open(ash::LauncherID id) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // In case invoked from menu and item closed while menu up.

  LauncherUpdater* updater = id_to_item_map_[id].updater;
  if (updater) {
    updater->window()->Show();
    ash::wm::ActivateWindow(updater->window());
  } else {
    DCHECK_EQ(TYPE_APP, id_to_item_map_[id].item_type);

    const Extension* extension =
        profile_->GetExtensionService()->GetInstalledExtension(
            id_to_item_map_[id].app_id);
    DCHECK(extension);

    extension_misc::LaunchContainer launch_container =
        profile_->GetExtensionService()->extension_prefs()->GetLaunchContainer(
            extension, ExtensionPrefs::LAUNCH_DEFAULT);

    Browser::OpenApplication(GetProfileForNewWindows(),
                             extension,
                             launch_container,
                             GURL(),
                             NEW_FOREGROUND_TAB);
  }
}

void ChromeLauncherDelegate::Close(ash::LauncherID id) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // May happen if menu closed.

  if (!id_to_item_map_[id].updater)
    return;  // TODO: maybe should treat as unpin?

  views::Widget* widget = views::Widget::GetWidgetForNativeView(
      id_to_item_map_[id].updater->window());
  if (widget)
    widget->Close();
}

bool ChromeLauncherDelegate::IsOpen(ash::LauncherID id) {
  return id_to_item_map_.find(id) != id_to_item_map_.end() &&
      id_to_item_map_[id].updater != NULL;
}

ExtensionPrefs::LaunchType ChromeLauncherDelegate::GetLaunchType(
    ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());

  return profile_->GetExtensionService()->extension_prefs()->GetLaunchType(
      id_to_item_map_[id].app_id, ExtensionPrefs::LAUNCH_DEFAULT);
}

std::string ChromeLauncherDelegate::GetAppID(TabContentsWrapper* tab) {
  return app_icon_loader_->GetAppID(tab);
}

void ChromeLauncherDelegate::SetAppImage(const std::string& id,
                                         const SkBitmap* image) {
  // TODO: need to get this working for shortcuts.

  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (i->second.app_id != id)
      continue;

    // Panel items may share the same app_id as the app that created them,
    // but they set their icon image in LauncherUpdater::UpdateLauncher(),
    // so do not set panel images here.
    if (i->second.updater &&
        i->second.updater->type() == LauncherUpdater::TYPE_EXTENSION_PANEL) {
      continue;
    }

    int index = model_->ItemIndexByID(i->first);
    ash::LauncherItem item = model_->items()[index];
    item.image = image ? *image : Extension::GetDefaultIcon(true);
    model_->Set(index, item);
    // It's possible we're waiting on more than one item, so don't break.
  }
}

bool ChromeLauncherDelegate::IsAppPinned(const std::string& app_id) {
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (IsPinned(i->first) && i->second.app_id == app_id)
      return true;
  }
  return false;
}

void ChromeLauncherDelegate::PinAppWithID(const std::string& app_id) {
  // If there is an item, do nothing and return.
  if (IsAppPinned(app_id))
    return;

  // Otherwise, create an item for it.
  CreateAppLauncherItem(NULL, app_id, ash::STATUS_CLOSED);
  PersistPinnedState();
}

void ChromeLauncherDelegate::SetLaunchType(
    ash::LauncherID id,
    ExtensionPrefs::LaunchType launch_type) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;

  return profile_->GetExtensionService()->extension_prefs()->SetLaunchType(
      id_to_item_map_[id].app_id, launch_type);
}

void ChromeLauncherDelegate::UnpinAppsWithID(const std::string& app_id) {
  for (IDToItemMap::iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ) {
    IDToItemMap::iterator current(i);
    ++i;
    if (current->second.app_id == app_id && IsPinned(current->first))
      Unpin(current->first);
  }
}

bool ChromeLauncherDelegate::IsLoggedInAsGuest() {
  return ProfileManager::GetDefaultProfileOrOffTheRecord()->IsOffTheRecord();
}

void ChromeLauncherDelegate::CreateNewIncognitoWindow() {
  Browser::NewEmptyWindow(GetProfileForNewWindows()->GetOffTheRecordProfile());
}

void ChromeLauncherDelegate::SetAutoHideBehavior(
    ash::ShelfAutoHideBehavior behavior) {
  ash::Shell::GetInstance()->SetShelfAutoHideBehavior(behavior);
  const char* value = NULL;
  switch (behavior) {
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT:
      value = kShelfAutoHideBehaviorDefault;
      break;
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      value = kShelfAutoHideBehaviorAlways;
      break;
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      value = kShelfAutoHideBehaviorNever;
      break;
  }
  profile_->GetPrefs()->SetString(prefs::kShelfAutoHideBehavior, value);
}

void ChromeLauncherDelegate::CreateNewTab() {
  Browser *last_browser = BrowserList::FindTabbedBrowser(
      GetProfileForNewWindows(), true);

  if (!last_browser) {
    CreateNewWindow();
    return;
  }

  last_browser->NewTab();
  aura::Window* window = last_browser->window()->GetNativeHandle();
  window->Show();
  ash::wm::ActivateWindow(window);
}

void ChromeLauncherDelegate::CreateNewWindow() {
  Browser::NewEmptyWindow(GetProfileForNewWindows());
}

void ChromeLauncherDelegate::ItemClicked(const ash::LauncherItem& item) {
  DCHECK(id_to_item_map_.find(item.id) != id_to_item_map_.end());
  LauncherUpdater* updater = id_to_item_map_[item.id].updater;
  if (updater) {
    views::Widget* widget =
        views::Widget::GetWidgetForNativeView(updater->window());
    if (widget && widget->IsActive()) {
      widget->Minimize();
      return;
    }
    // else case, fall through to show window.
  }
  Open(item.id);
}

int ChromeLauncherDelegate::GetBrowserShortcutResourceId() {
  return IDR_PRODUCT_LOGO_32;
}

string16 ChromeLauncherDelegate::GetTitle(const ash::LauncherItem& item) {
  DCHECK(id_to_item_map_.find(item.id) != id_to_item_map_.end());
  LauncherUpdater* updater = id_to_item_map_[item.id].updater;
  if (updater) {
    if (id_to_item_map_[item.id].item_type == TYPE_TABBED_BROWSER) {
      return updater->tab_model()->GetActiveTabContents() ?
          updater->tab_model()->GetActiveTabContents()->web_contents()->
          GetTitle() : string16();
    }
    // Fall through to get title from extension.
  }
  const Extension* extension = profile_->GetExtensionService()->
      GetInstalledExtension(id_to_item_map_[item.id].app_id);
  return extension ? UTF8ToUTF16(extension->name()) : string16();
}

ui::MenuModel* ChromeLauncherDelegate::CreateContextMenu(
    const ash::LauncherItem& item) {
  return new LauncherContextMenu(this, &item);
}

ui::MenuModel* ChromeLauncherDelegate::CreateContextMenuForLauncher() {
  return new LauncherContextMenu(this, NULL);
}

ash::LauncherID ChromeLauncherDelegate::GetIDByWindow(
    aura::Window* window) {
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (i->second.updater && i->second.updater->window() == window)
      return i->first;
  }
  return 0;
}

void ChromeLauncherDelegate::LauncherItemAdded(int index) {
}

void ChromeLauncherDelegate::LauncherItemRemoved(int index,
                                                 ash::LauncherID id) {
}

void ChromeLauncherDelegate::LauncherItemMoved(
    int start_index,
    int target_index) {
  ash::LauncherID id = model_->items()[target_index].id;
  if (id_to_item_map_.find(id) != id_to_item_map_.end() && IsPinned(id))
    PersistPinnedState();
}

void ChromeLauncherDelegate::LauncherItemChanged(
    int index,
    const ash::LauncherItem& old_item) {
  if (model_->items()[index].status == ash::STATUS_ACTIVE &&
      old_item.status == ash::STATUS_RUNNING) {
    ash::LauncherID id = model_->items()[index].id;
    if (id_to_item_map_[id].updater) {
      aura::Window* window_to_activate = id_to_item_map_[id].updater->window();
      if (window_to_activate && ash::wm::IsActiveWindow(window_to_activate))
        return;
      window_to_activate->Show();
      ash::wm::ActivateWindow(window_to_activate);
    }
  }
}

void ChromeLauncherDelegate::LauncherItemWillChange(int index) {
}

void ChromeLauncherDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      ProcessPendingPinnedApps();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      if (IsAppPinned(extension->id())) {
        // TODO(dpolukhin): also we need to remember index of the app to show
        // it on the same place when it gets loaded again.
        Item pending_item;
        pending_item.item_type = TYPE_APP;
        pending_item.app_id = extension->id();
        pending_pinned_apps_.push_back(pending_item);
        UnpinAppsWithID(extension->id());
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      std::string id = *content::Details<const std::string>(details).ptr();
      for (std::deque<Item>::iterator it = pending_pinned_apps_.begin();
           it != pending_pinned_apps_.end(); ++it) {
        if (it->app_id == id) {
          pending_pinned_apps_.erase(it);
          break;
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type=" << type;
  }
}

void ChromeLauncherDelegate::PersistPinnedState() {
  // Set kUseDefaultPinnedApps to false and use pinned apps list from prefs
  // from now on.
  profile_->GetPrefs()->SetBoolean(prefs::kUseDefaultPinnedApps, false);

  ListPrefUpdate updater(profile_->GetPrefs(), prefs::kPinnedLauncherApps);
  updater->Clear();
  for (size_t i = 0; i < model_->items().size(); ++i) {
    if (model_->items()[i].type == ash::TYPE_APP_SHORTCUT) {
      ash::LauncherID id = model_->items()[i].id;
      if (id_to_item_map_.find(id) != id_to_item_map_.end() &&
          IsPinned(id)) {
        base::DictionaryValue* app_value = CreateAppDict(
            id_to_item_map_[id].app_id);
        if (app_value)
          updater->Append(app_value);
      }
    }
  }
}

void ChromeLauncherDelegate::SetAppIconLoaderForTest(AppIconLoader* loader) {
  app_icon_loader_.reset(loader);
}

Profile* ChromeLauncherDelegate::GetProfileForNewWindows() {
  return ProfileManager::GetDefaultProfileOrOffTheRecord();
}

void ChromeLauncherDelegate::ProcessPendingPinnedApps() {
  while (!pending_pinned_apps_.empty()) {
    const Item& item = pending_pinned_apps_.front();

    if (!app_icon_loader_->IsValidID(item.app_id))
      return;

    PinAppWithID(item.app_id);
    pending_pinned_apps_.pop_front();
  }
}
