// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"

#include <set>
#include <vector>

#include "ash/multi_user/multi_user_window_manager.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"

namespace {

// Used for UMA metrics. Do not reorder.
enum TeleportWindowType {
  TELEPORT_WINDOW_BROWSER = 0,
  TELEPORT_WINDOW_INCOGNITO_BROWSER,
  TELEPORT_WINDOW_V1_APP,
  TELEPORT_WINDOW_V2_APP,
  DEPRECATED_TELEPORT_WINDOW_PANEL,
  TELEPORT_WINDOW_POPUP,
  TELEPORT_WINDOW_UNKNOWN,
  NUM_TELEPORT_WINDOW_TYPES
};

// Records the type of window which was transferred to another desktop.
void RecordUMAForTransferredWindowType(aura::Window* window) {
  // We need to figure out what kind of window this is to record the transfer.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  TeleportWindowType window_type = TELEPORT_WINDOW_UNKNOWN;
  if (browser) {
    if (browser->profile()->IsOffTheRecord()) {
      window_type = TELEPORT_WINDOW_INCOGNITO_BROWSER;
    } else if (browser->is_app()) {
      window_type = TELEPORT_WINDOW_V1_APP;
    } else if (browser->is_type_popup()) {
      window_type = TELEPORT_WINDOW_POPUP;
    } else {
      window_type = TELEPORT_WINDOW_BROWSER;
    }
  } else {
    // Unit tests might come here without a profile manager.
    if (!g_browser_process->profile_manager())
      return;
    // If it is not a browser, it is probably be a V2 application. In that case
    // one of the AppWindowRegistry instances should know about it.
    extensions::AppWindow* app_window = NULL;
    std::vector<Profile*> profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    for (std::vector<Profile*>::iterator it = profiles.begin();
         it != profiles.end() && app_window == NULL; it++) {
      app_window =
          extensions::AppWindowRegistry::Get(*it)->GetAppWindowForNativeWindow(
              window);
    }
    if (app_window)
      window_type = TELEPORT_WINDOW_V2_APP;
  }
  UMA_HISTOGRAM_ENUMERATION("MultiProfile.TeleportWindowType", window_type,
                            NUM_TELEPORT_WINDOW_TYPES);
}

// Returns the WindowMus to use when sending messages to the server.
aura::WindowMus* GetWindowMus(aura::Window* window) {
  if (!aura::WindowMus::Get(window))
    return nullptr;

  aura::Window* root_window = window->GetRootWindow();
  if (!root_window)
    return nullptr;

  // DesktopNativeWidgetAura creates two aura Windows. GetNativeWindow() returns
  // the child window. Get the widget for |window| and its root. If the Widgets
  // are the same, it means |window| is the native window of a
  // DesktopNativeWidgetAura. Use the root window to notify the server as that
  // corresponds to the top-level window that ash knows about.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  views::Widget* root_widget =
      views::Widget::GetWidgetForNativeView(root_window);
  return widget == root_widget ? aura::WindowMus::Get(root_window) : nullptr;
}

}  // namespace

// This class keeps track of all applications which were started for a user.
// When an app gets created, the window will be tagged for that user. Note
// that the destruction does not need to be tracked here since the universal
// window observer will take care of that.
class AppObserver : public extensions::AppWindowRegistry::Observer {
 public:
  explicit AppObserver(const std::string& user_id) : user_id_(user_id) {}
  ~AppObserver() override {}

  // AppWindowRegistry::Observer overrides:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override {
    aura::Window* window = app_window->GetNativeWindow();
    DCHECK(window);
    MultiUserWindowManagerChromeOS::GetInstance()->SetWindowOwner(
        window, AccountId::FromUserEmail(user_id_));
  }

 private:
  std::string user_id_;

  DISALLOW_COPY_AND_ASSIGN(AppObserver);
};

MultiUserWindowManagerChromeOS::MultiUserWindowManagerChromeOS(
    const AccountId& current_account_id)
    : current_account_id_(current_account_id),
      ash_multi_user_window_manager_(
          std::make_unique<ash::MultiUserWindowManager>(this,
                                                        current_account_id)) {
  if (features::IsUsingWindowService()) {
    multi_user_window_manager_mojom_ =
        views::MusClient::Get()
            ->window_tree_client()
            ->BindWindowManagerInterface<ash::mojom::MultiUserWindowManager>();
  }
}

MultiUserWindowManagerChromeOS::~MultiUserWindowManagerChromeOS() {
  // This may trigger callbacks to us, delete it early on.
  ash_multi_user_window_manager_.reset();

  // Remove all window observers.
  WindowToEntryMap::iterator window = window_to_entry_.begin();
  while (window != window_to_entry_.end()) {
    // Explicitly remove this from window observer list since OnWindowDestroyed
    // no longer does that.
    window->first->RemoveObserver(this);
    OnWindowDestroyed(window->first);
    window = window_to_entry_.begin();
  }

  // Remove all app observers.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // might be nullptr in unit tests.
  if (!profile_manager)
    return;

  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (auto it = profiles.begin(); it != profiles.end(); ++it) {
    const AccountId account_id = multi_user_util::GetAccountIdFromProfile(*it);
    AccountIdToAppWindowObserver::iterator app_observer_iterator =
        account_id_to_app_observer_.find(account_id);
    if (app_observer_iterator != account_id_to_app_observer_.end()) {
      extensions::AppWindowRegistry::Get(*it)->RemoveObserver(
          app_observer_iterator->second);
      delete app_observer_iterator->second;
      account_id_to_app_observer_.erase(app_observer_iterator);
    }
  }
}

void MultiUserWindowManagerChromeOS::Init() {
  // Since we are setting the SessionStateObserver and adding the user, this
  // function should get called only once.
  DCHECK(account_id_to_app_observer_.find(current_account_id_) ==
         account_id_to_app_observer_.end());

  // The BrowserListObserver would have been better to use then the old
  // notification system, but that observer fires before the window got created.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllSources());

  // Add an app window observer & all already running apps.
  Profile* profile =
      multi_user_util::GetProfileFromAccountId(current_account_id_);
  if (profile)
    AddUser(profile);
}

void MultiUserWindowManagerChromeOS::SetWindowOwner(
    aura::Window* window,
    const AccountId& account_id) {
  // Make sure the window is valid and there was no owner yet.
  DCHECK(window);
  DCHECK(account_id.is_valid());
  if (GetWindowOwner(window) == account_id)
    return;

  DCHECK(GetWindowOwner(window).empty());
  window_to_entry_[window] = new WindowEntry(account_id);

  // Check if this window was created due to a user interaction. If it was,
  // transfer it to the current user.
  const bool show_for_current_user =
      window->GetProperty(aura::client::kCreatedByUserGesture);
  if (window->env()->mode() == aura::Env::Mode::MUS) {
    aura::WindowMus* window_mus = GetWindowMus(window);
    if (window_mus) {
      multi_user_window_manager_mojom_->SetWindowOwner(
          window_mus->server_id(), account_id, show_for_current_user);
    }  // else case can happen during shutdown, or for child windows.
  } else {
    ash_multi_user_window_manager_->SetWindowOwner(window, account_id,
                                                   show_for_current_user);
  }

  // Add observers to track state changes.
  window->AddObserver(this);

  if (show_for_current_user)
    window_to_entry_[window]->set_show_for_user(current_account_id_);

  // Notify entry adding.
  for (Observer& observer : observers_)
    observer.OnOwnerEntryAdded(window);
}

const AccountId& MultiUserWindowManagerChromeOS::GetWindowOwner(
    aura::Window* window) const {
  WindowToEntryMap::const_iterator it = window_to_entry_.find(window);
  return it != window_to_entry_.end() ? it->second->owner() : EmptyAccountId();
}

void MultiUserWindowManagerChromeOS::ShowWindowForUser(
    aura::Window* window,
    const AccountId& account_id) {
  if (!window)
    return;

  if (window->env()->mode() == aura::Env::Mode::MUS) {
    aura::WindowMus* window_mus = GetWindowMus(window);
    if (window_mus) {
      multi_user_window_manager_mojom_->ShowWindowForUser(
          window_mus->server_id(), account_id);
    }
  } else {
    ash_multi_user_window_manager_->ShowWindowForUser(window, account_id);
  }
}

bool MultiUserWindowManagerChromeOS::AreWindowsSharedAmongUsers() const {
  WindowToEntryMap::const_iterator it = window_to_entry_.begin();
  for (; it != window_to_entry_.end(); ++it) {
    if (it->second->owner() != it->second->show_for_user())
      return true;
  }
  return false;
}

void MultiUserWindowManagerChromeOS::GetOwnersOfVisibleWindows(
    std::set<AccountId>* account_ids) const {
  for (WindowToEntryMap::const_iterator it = window_to_entry_.begin();
       it != window_to_entry_.end(); ++it) {
    if (it->first->IsVisible())
      account_ids->insert(it->second->owner());
  }
}

bool MultiUserWindowManagerChromeOS::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const AccountId& account_id) const {
  const AccountId& presenting_user = GetUserPresentingWindow(window);
  return (!presenting_user.is_valid()) || presenting_user == account_id;
}

const AccountId& MultiUserWindowManagerChromeOS::GetUserPresentingWindow(
    aura::Window* window) const {
  WindowToEntryMap::const_iterator it = window_to_entry_.find(window);
  // If the window is not owned by anyone it is shown on all desktops and we
  // return the empty string.
  if (it == window_to_entry_.end())
    return EmptyAccountId();
  // Otherwise we ask the object for its desktop.
  return it->second->show_for_user();
}

void MultiUserWindowManagerChromeOS::AddUser(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  const AccountId& account_id(
      multi_user_util::GetAccountIdFromProfile(profile));
  if (account_id_to_app_observer_.find(account_id) !=
      account_id_to_app_observer_.end())
    return;

  account_id_to_app_observer_[account_id] =
      new AppObserver(account_id.GetUserEmail());
  extensions::AppWindowRegistry::Get(profile)->AddObserver(
      account_id_to_app_observer_[account_id]);

  // Account all existing application windows of this user accordingly.
  const extensions::AppWindowRegistry::AppWindowList& app_windows =
      extensions::AppWindowRegistry::Get(profile)->app_windows();
  extensions::AppWindowRegistry::AppWindowList::const_iterator it =
      app_windows.begin();
  for (; it != app_windows.end(); ++it)
    account_id_to_app_observer_[account_id]->OnAppWindowAdded(*it);

  // Account all existing browser windows of this user accordingly.
  BrowserList* browser_list = BrowserList::GetInstance();
  BrowserList::const_iterator browser_it = browser_list->begin();
  for (; browser_it != browser_list->end(); ++browser_it) {
    if ((*browser_it)->profile()->GetOriginalProfile() == profile)
      AddBrowserWindow(*browser_it);
  }
}

void MultiUserWindowManagerChromeOS::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MultiUserWindowManagerChromeOS::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MultiUserWindowManagerChromeOS::OnWindowDestroyed(aura::Window* window) {
  // Remove the window from the owners list.
  delete window_to_entry_[window];
  window_to_entry_.erase(window);
}

void MultiUserWindowManagerChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_OPENED, type);
  AddBrowserWindow(content::Source<Browser>(source).ptr());
}

void MultiUserWindowManagerChromeOS::OnOwnerEntryChanged(
    aura::Window* window,
    const AccountId& account_id,
    bool was_minimized,
    bool teleported) {
  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  if (it == window_to_entry_.end())
    return;

  if (was_minimized)
    RecordUMAForTransferredWindowType(window);

  it->second->set_show_for_user(account_id);

  const AccountId& owner = GetWindowOwner(window);
  if (owner.is_valid()) {
    const user_manager::User* const window_owner =
        user_manager::UserManager::IsInitialized()
            ? user_manager::UserManager::Get()->FindUser(owner)
            : nullptr;
    if (window_owner && teleported) {
      window->SetProperty(
          aura::client::kAvatarIconKey,
          new gfx::ImageSkia(GetAvatarImageForUser(window_owner)));
    } else {
      window->ClearProperty(aura::client::kAvatarIconKey);
    }
  }

  for (Observer& observer : observers_)
    observer.OnOwnerEntryChanged(window);
}

void MultiUserWindowManagerChromeOS::OnWillSwitchActiveAccount(
    const AccountId& account_id) {
  current_account_id_ = account_id;
}

void MultiUserWindowManagerChromeOS::OnTransitionUserShelfToNewAccount() {
  ChromeLauncherController* chrome_launcher_controller =
      ChromeLauncherController::instance();
  // Some unit tests have no ChromeLauncherController.
  if (chrome_launcher_controller) {
    chrome_launcher_controller->ActiveUserChanged(
        current_account_id_.GetUserEmail());
  }
}

void MultiUserWindowManagerChromeOS::OnDidSwitchActiveAccount() {
  for (Observer& observer : observers_)
    observer.OnUserSwitchAnimationFinished();
}

const AccountId& MultiUserWindowManagerChromeOS::GetCurrentUserForTest() const {
  return current_account_id_;
}

void MultiUserWindowManagerChromeOS::AddBrowserWindow(Browser* browser) {
  // A unit test (e.g. CrashRestoreComplexTest.RestoreSessionForThreeUsers) can
  // come here with no valid window.
  if (!browser->window() || !browser->window()->GetNativeWindow())
    return;
  SetWindowOwner(browser->window()->GetNativeWindow(),
                 multi_user_util::GetAccountIdFromProfile(browser->profile()));
}
