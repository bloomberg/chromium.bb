// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include <set>
#include <vector>

#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/extension_utils.h"
#include "chrome/browser/ui/ash/launcher/launcher_app_icon_loader.h"
#include "chrome/browser/ui/ash/launcher/launcher_app_tab_helper.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/shell_window_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/aura/window.h"

using extensions::Extension;

namespace {

// Max loading animation time in milliseconds.
const int kMaxLoadingTimeMs = 60 * 1000;

// Item controller for an app shortcut. Shortcuts track app and launcher ids,
// but do not have any associated windows (opening a shortcut will replace the
// item with the appropriate LauncherItemController type).
class AppShortcutLauncherItemController : public LauncherItemController {
 public:
  AppShortcutLauncherItemController(const std::string& app_id,
                                    ChromeLauncherController* controller)
      : LauncherItemController(TYPE_SHORTCUT, app_id, controller) {
  }

  virtual ~AppShortcutLauncherItemController() {}

  // LauncherItemController overrides:
  virtual string16 GetTitle() OVERRIDE {
    return GetAppTitle();
  }

  virtual bool HasWindow(aura::Window* window) const OVERRIDE {
    return false;
  }

  virtual bool IsOpen() const OVERRIDE {
    return false;
  }

  virtual void Open(int event_flags) OVERRIDE {
    // This will open the app for app_id(), replacing this with an appropriate
    // LauncherItemController.
    launcher_controller()->OpenAppID(app_id(), event_flags);
  }

  virtual void Close() OVERRIDE {
    // TODO: maybe should treat as unpin?
  }

  virtual void Clicked() OVERRIDE {
    Open(ui::EF_NONE);
  }

  virtual void OnRemoved() OVERRIDE {
    // AppShortcutLauncherItemController is unowned; delete on removal.
    delete this;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppShortcutLauncherItemController);
};

}  // namespace

// ChromeLauncherController ----------------------------------------------------

// statics
ChromeLauncherController* ChromeLauncherController::instance_ = NULL;

ChromeLauncherController::ChromeLauncherController(Profile* profile,
                                                   ash::LauncherModel* model)
    : model_(model),
      profile_(profile),
      observed_sync_service_(NULL) {
  if (!profile_) {
    // Use the original profile as on chromeos we may get a temporary off the
    // record profile.
    profile_ = ProfileManager::GetDefaultProfile()->GetOriginalProfile();

    // Monitor app sync on chromeos.
    PrefService* prefs = profile_->GetPrefs();
    if (!IsLoggedInAsGuest() &&
        prefs->GetBoolean(prefs::kLauncherShouldRunSyncAnimation)) {
      prefs->SetBoolean(prefs::kLauncherShouldRunSyncAnimation, false);
      observed_sync_service_ =
          ProfileSyncServiceFactory::GetForProfile(profile_);
      if (observed_sync_service_)
        observed_sync_service_->AddObserver(this);
    }
  }

  instance_ = this;
  model_->AddObserver(this);
  // TODO(stevenjb): Find a better owner for shell_window_controller_?
  shell_window_controller_.reset(new ShellWindowLauncherController(this));
  app_tab_helper_.reset(new LauncherAppTabHelper(profile_));
  app_icon_loader_.reset(new LauncherAppIconLoader(profile_, this));

  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_EXTENSION_LOADED,
                              content::Source<Profile>(profile_));
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_EXTENSION_UNLOADED,
                              content::Source<Profile>(profile_));
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(prefs::kPinnedLauncherApps, this);
  pref_change_registrar_.Add(prefs::kShelfAlignment, this);
  pref_change_registrar_.Add(prefs::kShelfAutoHideBehavior, this);
}

ChromeLauncherController::~ChromeLauncherController() {
  // Reset the shell window controller here since it has a weak pointer to this.
  shell_window_controller_.reset();

  model_->RemoveObserver(this);
  for (IDToItemControllerMap::iterator i = id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    i->second->OnRemoved();
    model_->RemoveItemAt(model_->ItemIndexByID(i->first));
  }
  if (instance_ == this)
    instance_ = NULL;

  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->RemoveShellObserver(this);

  if (observed_sync_service_)
    observed_sync_service_->RemoveObserver(this);
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
    SetShelfAutoHideBehaviorFromPrefs();
    SetShelfAlignmentFromPrefs();
    ash::Shell::GetInstance()->AddShellObserver(this);
  }
}

ash::LauncherID ChromeLauncherController::CreateTabbedLauncherItem(
    LauncherItemController* controller,
    IncognitoState is_incognito,
    ash::LauncherItemStatus status) {
  ash::LauncherID id = model_->next_id();
  DCHECK(!HasItemController(id));
  DCHECK(controller);
  id_to_item_controller_map_[id] = controller;
  controller->set_launcher_id(id);

  ash::LauncherItem item;
  item.type = ash::TYPE_TABBED;
  item.is_incognito = (is_incognito == STATE_INCOGNITO);
  item.status = status;
  model_->Add(item);
  return id;
}

ash::LauncherID ChromeLauncherController::CreateAppLauncherItem(
    LauncherItemController* controller,
    const std::string& app_id,
    ash::LauncherItemStatus status) {
  DCHECK(controller);
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

void ChromeLauncherController::SetItemController(
    ash::LauncherID id,
    LauncherItemController* controller) {
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  DCHECK(iter != id_to_item_controller_map_.end());
  iter->second->OnRemoved();
  iter->second = controller;
  controller->set_launcher_id(id);
}

void ChromeLauncherController::CloseLauncherItem(ash::LauncherID id) {
  if (IsPinned(id)) {
    // Create a new shortcut controller.
    IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
    DCHECK(iter != id_to_item_controller_map_.end());
    SetItemStatus(id, ash::STATUS_CLOSED);
    std::string app_id = iter->second->app_id();
    iter->second->OnRemoved();
    iter->second = new AppShortcutLauncherItemController(app_id, this);
    iter->second->set_launcher_id(id);
  } else {
    LauncherItemClosed(id);
  }
}

void ChromeLauncherController::Unpin(ash::LauncherID id) {
  DCHECK(HasItemController(id));

  LauncherItemController* controller = id_to_item_controller_map_[id];
  if (controller->type() == LauncherItemController::TYPE_APP) {
    int index = model_->ItemIndexByID(id);
    ash::LauncherItem item = model_->items()[index];
    item.type = ash::TYPE_PLATFORM_APP;
    model_->Set(index, item);
  } else {
    LauncherItemClosed(id);
  }
  if (CanPin())
    PersistPinnedState();
}

void ChromeLauncherController::Pin(ash::LauncherID id) {
  DCHECK(HasItemController(id));

  int index = model_->ItemIndexByID(id);
  ash::LauncherItem item = model_->items()[index];

  if (item.type != ash::TYPE_PLATFORM_APP)
    return;

  item.type = ash::TYPE_APP_SHORTCUT;
  model_->Set(index, item);

  if (CanPin())
    PersistPinnedState();
}

bool ChromeLauncherController::IsPinned(ash::LauncherID id) {
  int index = model_->ItemIndexByID(id);
  ash::LauncherItemType type = model_->items()[index].type;
  return type == ash::TYPE_APP_SHORTCUT;
}

void ChromeLauncherController::TogglePinned(ash::LauncherID id) {
  if (!HasItemController(id))
    return;  // May happen if item closed with menu open.

  if (IsPinned(id))
    Unpin(id);
  else
    Pin(id);
}

bool ChromeLauncherController::IsPinnable(ash::LauncherID id) const {
  int index = model_->ItemIndexByID(id);
  if (index == -1)
    return false;

  ash::LauncherItemType type = model_->items()[index].type;
  return ((type == ash::TYPE_APP_SHORTCUT || type == ash::TYPE_PLATFORM_APP) &&
          CanPin());
}

void ChromeLauncherController::Open(ash::LauncherID id, int event_flags) {
  if (!HasItemController(id))
    return;  // In case invoked from menu and item closed while menu up.
  id_to_item_controller_map_[id]->Open(event_flags);
}

void ChromeLauncherController::OpenAppID(const std::string& app_id,
                                         int event_flags) {
  // If there is an existing non-shortcut controller for this app, open it.
  ash::LauncherID id = GetLauncherIDForAppID(app_id);
  if (id > 0) {
    LauncherItemController* controller = id_to_item_controller_map_[id];
    if (controller->type() != LauncherItemController::TYPE_SHORTCUT) {
      controller->Open(event_flags);
      return;
    }
  }

  // Check if there are any open tabs for this app.
  AppIDToTabContentsListMap::iterator i =
      app_id_to_tab_contents_list_.find(app_id);

  if (i != app_id_to_tab_contents_list_.end()) {
    DCHECK(!i->second.empty());
    TabContents* tab = i->second.front();
    Browser* browser = browser::FindBrowserWithWebContents(
        tab->web_contents());
    TabStripModel* tab_strip = browser->tab_strip_model();
    int index = tab_strip->GetIndexOfTabContents(tab);
    DCHECK_NE(TabStripModel::kNoTab, index);
    tab_strip->ActivateTabAt(index, false);
    browser->window()->Show();
    ash::wm::ActivateWindow(browser->window()->GetNativeWindow());
  } else {
    const Extension* extension =
        profile_->GetExtensionService()->GetInstalledExtension(app_id);
    extension_utils::OpenExtension(GetProfileForNewWindows(),
                                   extension,
                                   event_flags);
  }
}

void ChromeLauncherController::Close(ash::LauncherID id) {
  if (!HasItemController(id))
    return;  // May happen if menu closed.
  id_to_item_controller_map_[id]->Close();
}

bool ChromeLauncherController::IsOpen(ash::LauncherID id) {
  if (!HasItemController(id))
    return false;
  return id_to_item_controller_map_[id]->IsOpen();
}

extensions::ExtensionPrefs::LaunchType ChromeLauncherController::GetLaunchType(
    ash::LauncherID id) {
  DCHECK(HasItemController(id));

  return profile_->GetExtensionService()->extension_prefs()->GetLaunchType(
      id_to_item_controller_map_[id]->app_id(),
      extensions::ExtensionPrefs::LAUNCH_DEFAULT);
}

std::string ChromeLauncherController::GetAppID(TabContents* tab) {
  return app_tab_helper_->GetAppID(tab);
}

ash::LauncherID ChromeLauncherController::GetLauncherIDForAppID(
    const std::string& app_id) {
  for (IDToItemControllerMap::const_iterator i =
           id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    if (i->second->app_id() == app_id)
      return i->first;
  }
  return 0;
}

void ChromeLauncherController::SetAppImage(const std::string& id,
                                           const gfx::ImageSkia& image) {
  // TODO: need to get this working for shortcuts.

  for (IDToItemControllerMap::const_iterator i =
           id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    if (i->second->app_id() != id)
      continue;

    // Panel items may share the same app_id as the app that created them,
    // but they set their icon image in
    // BrowserLauncherItemController::UpdateLauncher(), so do not set panel
    // images here.
    if (i->second->type() == LauncherItemController::TYPE_EXTENSION_PANEL)
      continue;

    int index = model_->ItemIndexByID(i->first);
    ash::LauncherItem item = model_->items()[index];
    item.image = image;
    model_->Set(index, item);
    // It's possible we're waiting on more than one item, so don't break.
  }
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

void ChromeLauncherController::PinAppWithID(const std::string& app_id) {
  if (CanPin())
    DoPinAppWithID(app_id);
  else
    NOTREACHED();
}

void ChromeLauncherController::SetLaunchType(
    ash::LauncherID id,
    extensions::ExtensionPrefs::LaunchType launch_type) {
  if (!HasItemController(id))
    return;

  return profile_->GetExtensionService()->extension_prefs()->SetLaunchType(
      id_to_item_controller_map_[id]->app_id(), launch_type);
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
  chrome::NewEmptyWindow(GetProfileForNewWindows()->GetOffTheRecordProfile());
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
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      value = ash::kShelfAutoHideBehaviorAlways;
      break;
    case ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      value = ash::kShelfAutoHideBehaviorNever;
      break;
  }
  profile_->GetPrefs()->SetString(prefs::kShelfAutoHideBehavior, value);
}

void ChromeLauncherController::RemoveTabFromRunningApp(
    TabContents* tab,
    const std::string& app_id) {
  tab_contents_to_app_id_.erase(tab);
  AppIDToTabContentsListMap::iterator i_app_id =
      app_id_to_tab_contents_list_.find(app_id);
  if (i_app_id != app_id_to_tab_contents_list_.end()) {
    TabContentsList* tab_list = &i_app_id->second;
    tab_list->remove(tab);
    if (tab_list->empty()) {
      app_id_to_tab_contents_list_.erase(i_app_id);
      i_app_id = app_id_to_tab_contents_list_.end();
      ash::LauncherID id = GetLauncherIDForAppID(app_id);
      if (id > 0)
        SetItemStatus(id, ash::STATUS_CLOSED);
    }
  }
}

void ChromeLauncherController::UpdateAppState(TabContents* tab,
                                              AppState app_state) {
  std::string app_id = GetAppID(tab);

  // Check the old |app_id| for a tab. If the contents has changed we need to
  // remove it from the previous app.
  if (tab_contents_to_app_id_.find(tab) != tab_contents_to_app_id_.end()) {
    std::string last_app_id = tab_contents_to_app_id_[tab];
    if (last_app_id != app_id)
      RemoveTabFromRunningApp(tab, last_app_id);
  }

  if (app_id.empty())
    return;

  tab_contents_to_app_id_[tab] = app_id;

  if (app_state == APP_STATE_REMOVED) {
    // The tab has gone away.
    RemoveTabFromRunningApp(tab, app_id);
  } else {
    TabContentsList& tab_list(app_id_to_tab_contents_list_[app_id]);
    if (app_state == APP_STATE_INACTIVE) {
      TabContentsList::const_iterator i_tab =
          std::find(tab_list.begin(), tab_list.end(), tab);
      if (i_tab == tab_list.end())
        tab_list.push_back(tab);
    } else {
      tab_list.remove(tab);
      tab_list.push_front(tab);
    }
    ash::LauncherID id = GetLauncherIDForAppID(app_id);
    if (id > 0) {
      // If the window is active, mark the app as active.
      SetItemStatus(id, app_state == APP_STATE_WINDOW_ACTIVE ?
          ash::STATUS_ACTIVE : ash::STATUS_RUNNING);
    }
  }
}

void ChromeLauncherController::CreateNewTab() {
  Browser* last_browser = browser::FindTabbedBrowser(
      GetProfileForNewWindows(), true);

  if (!last_browser) {
    CreateNewWindow();
    return;
  }

  if (!IsActiveBrowserShowingNTP(last_browser))
    chrome::NewTab(last_browser);

  aura::Window* window = last_browser->window()->GetNativeWindow();
  window->Show();
  ash::wm::ActivateWindow(window);
}

void ChromeLauncherController::CreateNewWindow() {
  chrome::NewEmptyWindow(GetProfileForNewWindows());
}

void ChromeLauncherController::ItemClicked(const ash::LauncherItem& item,
                                           int event_flags) {
  DCHECK(HasItemController(item.id));
  id_to_item_controller_map_[item.id]->Clicked();
}

int ChromeLauncherController::GetBrowserShortcutResourceId() {
  return IDR_PRODUCT_LOGO_32;
}

string16 ChromeLauncherController::GetTitle(const ash::LauncherItem& item) {
  DCHECK(HasItemController(item.id));
  return id_to_item_controller_map_[item.id]->GetTitle();
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
  for (IDToItemControllerMap::const_iterator i =
           id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ++i) {
    if (i->second->HasWindow(window))
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
  if (HasItemController(id) && IsPinned(id))
    PersistPinnedState();
}

void ChromeLauncherController::LauncherItemChanged(
    int index,
    const ash::LauncherItem& old_item) {
  if (model_->items()[index].status == ash::STATUS_ACTIVE &&
      old_item.status == ash::STATUS_RUNNING) {
    ash::LauncherID id = model_->items()[index].id;
    id_to_item_controller_map_[id]->Open(ui::EF_NONE);
  }
}

void ChromeLauncherController::LauncherStatusChanged() {
}

void ChromeLauncherController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      UpdateAppLaunchersFromPref();
      CheckAppSync();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const content::Details<extensions::UnloadedExtensionInfo> unload_info(
          details);
      const Extension* extension = unload_info->extension;
      if (IsAppPinned(extension->id()))
        DoUnpinAppsWithID(extension->id());
      app_icon_loader_->ClearImage(extension->id());
      break;
    }
    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string& pref_name(
          *content::Details<std::string>(details).ptr());
      if (pref_name == prefs::kPinnedLauncherApps)
        UpdateAppLaunchersFromPref();
      else if (pref_name == prefs::kShelfAlignment)
        SetShelfAlignmentFromPrefs();
      else if (pref_name == prefs::kShelfAutoHideBehavior)
        SetShelfAutoHideBehaviorFromPrefs();
      else
        NOTREACHED() << "Unexpected pref change for " << pref_name;
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type=" << type;
  }
}

void ChromeLauncherController::OnShelfAlignmentChanged() {
  const char* pref_value = NULL;
  switch (ash::Shell::GetInstance()->GetShelfAlignment()) {
    case ash::SHELF_ALIGNMENT_BOTTOM:
      pref_value = ash::kShelfAlignmentBottom;
      break;
    case ash::SHELF_ALIGNMENT_LEFT:
      pref_value = ash::kShelfAlignmentLeft;
      break;
    case ash::SHELF_ALIGNMENT_RIGHT:
      pref_value = ash::kShelfAlignmentRight;
      break;
  }
  profile_->GetPrefs()->SetString(prefs::kShelfAlignment, pref_value);
}

void ChromeLauncherController::OnStateChanged() {
  DCHECK(observed_sync_service_);
  CheckAppSync();
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
        if (HasItemController(id) && IsPinned(id)) {
          base::DictionaryValue* app_value = ash::CreateAppDict(
              id_to_item_controller_map_[id]->app_id());
          if (app_value)
            updater->Append(app_value);
        }
      }
    }
  }
  pref_change_registrar_.Add(prefs::kPinnedLauncherApps, this);
}

void ChromeLauncherController::SetAppTabHelperForTest(AppTabHelper* helper) {
  app_tab_helper_.reset(helper);
}

void ChromeLauncherController::SetAppIconLoaderForTest(AppIconLoader* loader) {
  app_icon_loader_.reset(loader);
}

Profile* ChromeLauncherController::GetProfileForNewWindows() {
  return ProfileManager::GetDefaultProfileOrOffTheRecord();
}

void ChromeLauncherController::LauncherItemClosed(ash::LauncherID id) {
  IDToItemControllerMap::iterator iter = id_to_item_controller_map_.find(id);
  DCHECK(iter != id_to_item_controller_map_.end());
  app_icon_loader_->ClearImage(iter->second->app_id());
  iter->second->OnRemoved();
  id_to_item_controller_map_.erase(iter);
  model_->RemoveItemAt(model_->ItemIndexByID(id));
}

void ChromeLauncherController::DoPinAppWithID(const std::string& app_id) {
  // If there is an item, do nothing and return.
  if (IsAppPinned(app_id))
    return;

  ash::LauncherID launcher_id = GetLauncherIDForAppID(app_id);
  if (launcher_id) {
    // App item exists, pin it
    Pin(launcher_id);
  } else {
    // Otherwise, create a shortcut item for it.
    CreateAppShortcutLauncherItem(app_id, model_->item_count());
    if (CanPin())
      PersistPinnedState();
  }
}

void ChromeLauncherController::DoUnpinAppsWithID(const std::string& app_id) {
  for (IDToItemControllerMap::iterator i = id_to_item_controller_map_.begin();
       i != id_to_item_controller_map_.end(); ) {
    IDToItemControllerMap::iterator current(i);
    ++i;
    if (current->second->app_id() == app_id && IsPinned(current->first))
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
        app_tab_helper_->IsValidID(app_id)) {
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

        IDToItemControllerMap::const_iterator entry =
            id_to_item_controller_map_.find(item.id);
        if (entry != id_to_item_controller_map_.end() &&
            entry->second->app_id() == *pref_app_id) {
          ++pref_app_id;
          break;
        } else {
          LauncherItemClosed(item.id);
          --index;
        }
      }
      // If the item wasn't found, that means id_to_item_controller_map_
      // is out of sync.
      DCHECK(index < model_->item_count());
    } else {
      // This app wasn't pinned before, insert a new entry.
      ash::LauncherID id = CreateAppShortcutLauncherItem(*pref_app_id, index);
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
    DoPinAppWithID(*pref_app_id);
}

void ChromeLauncherController::SetShelfAutoHideBehaviorFromPrefs() {
  // Note: To maintain sync compatibility with old images of chrome/chromeos
  // the set of values that may be encountered includes the now-extinct
  // "Default" as well as "Never" and "Always", "Default" should now
  // be treated as "Never".
  // (http://code.google.com/p/chromium/issues/detail?id=146773)
  const std::string behavior_value(
      profile_->GetPrefs()->GetString(prefs::kShelfAutoHideBehavior));
  ash::ShelfAutoHideBehavior behavior =
      ash::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
  if (behavior_value == ash::kShelfAutoHideBehaviorAlways)
    behavior = ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  ash::Shell::GetInstance()->SetShelfAutoHideBehavior(behavior);
}

void ChromeLauncherController::SetShelfAlignmentFromPrefs() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowLauncherAlignmentMenu))
    return;

  const std::string alignment_value(
      profile_->GetPrefs()->GetString(prefs::kShelfAlignment));
  ash::ShelfAlignment alignment = ash::SHELF_ALIGNMENT_BOTTOM;
  if (alignment_value == ash::kShelfAlignmentLeft)
    alignment = ash::SHELF_ALIGNMENT_LEFT;
  else if (alignment_value == ash::kShelfAlignmentRight)
    alignment = ash::SHELF_ALIGNMENT_RIGHT;
  ash::Shell::GetInstance()->SetShelfAlignment(alignment);
}

TabContents* ChromeLauncherController::GetLastActiveTabContents(
    const std::string& app_id) {
  AppIDToTabContentsListMap::const_iterator i =
      app_id_to_tab_contents_list_.find(app_id);
  if (i == app_id_to_tab_contents_list_.end())
    return NULL;
  DCHECK_GT(i->second.size(), 0u);
  return *i->second.begin();
}

ash::LauncherID ChromeLauncherController::InsertAppLauncherItem(
    LauncherItemController* controller,
    const std::string& app_id,
    ash::LauncherItemStatus status,
    int index) {
  ash::LauncherID id = model_->next_id();
  DCHECK(!HasItemController(id));
  DCHECK(controller);
  id_to_item_controller_map_[id] = controller;
  controller->set_launcher_id(id);

  ash::LauncherItem item;
  item.type = controller->GetLauncherItemType();
  item.is_incognito = false;
  item.image = Extension::GetDefaultIcon(true);

  TabContents* active_tab = GetLastActiveTabContents(app_id);
  if (active_tab) {
    Browser* browser = browser::FindBrowserWithWebContents(
        active_tab->web_contents());
    DCHECK(browser);
    if (browser->window()->IsActive())
      status = ash::STATUS_ACTIVE;
    else
      status = ash::STATUS_RUNNING;
  }
  item.status = status;

  model_->AddAt(index, item);

  if (controller->type() != LauncherItemController::TYPE_EXTENSION_PANEL)
    app_icon_loader_->FetchImage(app_id);

  return id;
}

ash::LauncherID ChromeLauncherController::CreateAppShortcutLauncherItem(
    const std::string& app_id,
    int index) {
  AppShortcutLauncherItemController* controller =
      new AppShortcutLauncherItemController(app_id, this);
  ash::LauncherID launcher_id = InsertAppLauncherItem(
      controller, app_id, ash::STATUS_CLOSED, index);
  return launcher_id;
}

bool ChromeLauncherController::HasItemController(ash::LauncherID id) const {
  return id_to_item_controller_map_.find(id) !=
         id_to_item_controller_map_.end();
}

void ChromeLauncherController::CheckAppSync() {
  if (!observed_sync_service_ ||
      !observed_sync_service_->HasSyncSetupCompleted()) {
    return;
  }

  const bool synced = observed_sync_service_->ShouldPushChanges();
  const bool has_pending_extension = profile_->GetExtensionService()->
      pending_extension_manager()->HasPendingExtensionFromSync();

  if (synced && !has_pending_extension)
    StopLoadingAnimation();
  else
    StartLoadingAnimation();
}

void ChromeLauncherController::StartLoadingAnimation() {
  DCHECK(observed_sync_service_);
  if (model_->status() == ash::LauncherModel::STATUS_LOADING)
    return;

  loading_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kMaxLoadingTimeMs),
      this, &ChromeLauncherController::StopLoadingAnimation);
  model_->SetStatus(ash::LauncherModel::STATUS_LOADING);
}

void ChromeLauncherController::StopLoadingAnimation() {
  DCHECK(observed_sync_service_);

  model_->SetStatus(ash::LauncherModel::STATUS_NORMAL);
  loading_timer_.Stop();
  observed_sync_service_->RemoveObserver(this);
  observed_sync_service_ = NULL;
}

bool ChromeLauncherController::IsActiveBrowserShowingNTP(Browser* browser) {
  content::WebContents* current_tab = chrome::GetActiveWebContents(browser);
  if (current_tab) {
    content::NavigationEntry* active_entry =
        current_tab->GetController().GetActiveEntry();
    if (active_entry &&
        active_entry->GetURL() == GURL(chrome::kChromeUINewTabURL))
      return true;
  }
  return false;
}

