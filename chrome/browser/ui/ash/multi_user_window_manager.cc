// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user_window_manager.h"

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "ash/ash_switches.h"
#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace {
chrome::MultiUserWindowManager* g_instance = NULL;

// Get the user id from a given profile.
std::string GetUserIDFromProfile(Profile* profile) {
  return gaia::CanonicalizeEmail(gaia::SanitizeEmail(
      profile->GetOriginalProfile()->GetProfileName()));
}

}  // namespace

namespace chrome {

// This class keeps track of all applications which were started for a user.
// When an app gets created, the window will be tagged for that user. Note
// that the destruction does not need to be tracked here since the universal
// window observer will take care of that.
class AppObserver : public apps::ShellWindowRegistry::Observer {
 public:
  explicit AppObserver(const std::string& user_id) : user_id_(user_id) {}
  virtual ~AppObserver() {}

  // ShellWindowRegistry::Observer overrides:
  virtual void OnShellWindowAdded(apps::ShellWindow* shell_window) OVERRIDE {
    aura::Window* window = shell_window->GetNativeWindow();
    DCHECK(window);
    chrome::MultiUserWindowManager::GetInstance()->SetWindowOwner(window,
                                                                  user_id_);
  }
  virtual void OnShellWindowIconChanged(apps::ShellWindow* shell_window)
      OVERRIDE {}
  virtual void OnShellWindowRemoved(apps::ShellWindow* shell_window)
      OVERRIDE {}

 private:
  std::string user_id_;

  DISALLOW_COPY_AND_ASSIGN(AppObserver);
};

// static
MultiUserWindowManager* MultiUserWindowManager::GetInstance() {
  if (!g_instance &&
      ash::Shell::GetInstance()->delegate()->IsMultiProfilesEnabled() &&
      !ash::switches::UseFullMultiProfileMode()) {
    g_instance = CreateInstanceInternal(
        GetUserIDFromProfile(ProfileManager::GetDefaultProfile()));
  }
  return g_instance;
}

// static
void MultiUserWindowManager::DeleteInstance() {
  if (g_instance)
    delete g_instance;
  g_instance = NULL;
}

void MultiUserWindowManager::SetWindowOwner(aura::Window* window,
                                            const std::string& user_id) {
  // Make sure the window is valid and there was no owner yet.
  DCHECK(window);
  DCHECK(!user_id.empty());
  if (GetWindowOwner(window) == user_id)
    return;
  DCHECK(GetWindowOwner(window).empty());
  window_to_entry_[window] = new WindowEntry(user_id);

  // Set the window and the state observer.
  window->AddObserver(this);
  ash::wm::GetWindowState(window)->AddObserver(this);

  if (user_id != current_user_id_)
    SetWindowVisibility(window, false);
}

const std::string& MultiUserWindowManager::GetWindowOwner(
    aura::Window* window) {
  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  return it != window_to_entry_.end() ? it->second->owner() : EmptyString();
}

void MultiUserWindowManager::ShowWindowForUser(aura::Window* window,
                                               const std::string& user_id) {
  // If there is either no owner, or the owner is the current user, no action
  // is required.
  const std::string& owner = GetWindowOwner(window);
  if (owner.empty() ||
      (owner == user_id && IsWindowOnDesktopOfUser(window, user_id)))
    return;

  // Check that we are not trying to transfer ownership of a minimized window.
  if (user_id != owner && ash::wm::GetWindowState(window)->IsMinimized())
    return;

  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  it->second->set_show_for_user(user_id);

  // Show the window if the added user is the current one.
  if (user_id == current_user_id_)
    SetWindowVisibility(window, true);
  else
    SetWindowVisibility(window, false);
}

bool MultiUserWindowManager::AreWindowsSharedAmongUsers() {
  WindowToEntryMap::iterator it = window_to_entry_.begin();
  for (; it != window_to_entry_.end(); ++it) {
    if (it->second->owner() != it->second->show_for_user())
      return true;
  }
  return false;
}

bool MultiUserWindowManager::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const std::string& user_id) {
  const std::string& presenting_user = GetUserPresentingWindow(window);
  return presenting_user.empty() || presenting_user == user_id;
}

const std::string& MultiUserWindowManager::GetUserPresentingWindow(
    aura::Window* window) {
  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  // If the window is not owned by anyone it is shown on all desktops and we
  // return the empty string.
  if (it == window_to_entry_.end())
    return EmptyString();
  // Otherwise we ask the object for its desktop.
  return it->second->show_for_user();
}

void MultiUserWindowManager::ActiveUserChanged(const std::string& user_id) {
  DCHECK(user_id != current_user_id_);
  std::string old_user = current_user_id_;
  current_user_id_ = user_id;
  // Hide all windows which are not shown and show which should get shown.
  WindowToEntryMap::iterator it = window_to_entry_.begin();
  for (; it != window_to_entry_.end(); ++it) {
    aura::Window* window = it->first;
    bool should_be_visible =
        it->second->show_for_user() == user_id && it->second->show();
    bool is_visible = window->IsVisible();
    if (should_be_visible != is_visible)
      SetWindowVisibility(window, should_be_visible);
  }
}

void MultiUserWindowManager::UserAddedToSession(const std::string& user_id) {
  // Make sure that all newly created applications get properly added to this
  // user's account.
  AddUser(user_id);
}

void MultiUserWindowManager::OnWindowDestroyed(aura::Window* window) {
  DCHECK(!GetWindowOwner(window).empty());
  // Remove the state and the window observer.
  ash::wm::GetWindowState(window)->RemoveObserver(this);
  window->RemoveObserver(this);
  // Remove the window from the owners list.
  delete window_to_entry_[window];
  window_to_entry_.erase(window);
}

void MultiUserWindowManager::OnWindowVisibilityChanging(
    aura::Window* window, bool visible) {
  // This command gets called first and immediately when show or hide gets
  // called. We remember here the desired state for restoration IF we were
  // not ourselves issuing the call.
  // Note also that using the OnWindowVisibilityChanged callback cannot be
  // used for this.
  if (!suppress_visibility_changes_) {
    WindowToEntryMap::iterator it = window_to_entry_.find(window);
    // If the window is not owned by anyone it is shown on all desktops.
    if (it != window_to_entry_.end()) {
      // Remember what was asked for so that we can restore this when the users
      // desktop gets restored.
      it->second->set_show(visible);
    }
  }
}

void MultiUserWindowManager::OnWindowVisibilityChanged(
    aura::Window* window, bool visible) {
  // Don't allow to make the window visible if it shouldn't be.
  if (visible && !IsWindowOnDesktopOfUser(window, current_user_id_))
    SetWindowVisibility(window, false);
}

void MultiUserWindowManager::OnWindowShowTypeChanged(
    ash::wm::WindowState* window_state,
    ash::wm::WindowShowType old_type) {
  if (!window_state->IsMinimized())
    return;

  aura::Window* window = window_state->window();
  // If the window was shown on a different users desktop: move it back.
  const std::string& owner = GetWindowOwner(window);
  if (!IsWindowOnDesktopOfUser(window, owner))
    ShowWindowForUser(window, owner);
}

void MultiUserWindowManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_BROWSER_WINDOW_READY)
    AddBrowserWindow(content::Source<Browser>(source).ptr());
}

// static
MultiUserWindowManager* MultiUserWindowManager::CreateInstanceInternal(
    std::string active_user_id) {
  DCHECK(!g_instance);
  g_instance = new MultiUserWindowManager(active_user_id);
  return g_instance;
}

MultiUserWindowManager::MultiUserWindowManager(
    const std::string& current_user_id)
    : current_user_id_(current_user_id),
      suppress_visibility_changes_(false) {
  // Add a session state observer to be able to monitor session changes.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->session_state_delegate()->
        AddSessionStateObserver(this);

  // The BrowserListObserver would have been better to use then the old
  // notification system, but that observer fires before the window got created.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                 content::NotificationService::AllSources());

  // Add an app window observer & all already running apps.
  AddUser(current_user_id);
}

MultiUserWindowManager::~MultiUserWindowManager() {
  // Remove all window observers.
  WindowToEntryMap::iterator window_observer = window_to_entry_.begin();
  while (window_observer != window_to_entry_.end()) {
    OnWindowDestroyed(window_observer->first);
    window_observer = window_to_entry_.begin();
  }

  // Remove all app observers.
  UserIDToShellWindowObserver::iterator app_observer_iterator =
      user_id_to_app_observer_.begin();
  while (app_observer_iterator != user_id_to_app_observer_.end()) {
    Profile* profile = GetProfileFromUserID(app_observer_iterator->first);
    DCHECK(profile);
    apps::ShellWindowRegistry::Get(profile)->RemoveObserver(
        app_observer_iterator->second);
    delete app_observer_iterator->second;
    user_id_to_app_observer_.erase(app_observer_iterator);
    app_observer_iterator = user_id_to_app_observer_.begin();
  }

  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->session_state_delegate()->
        RemoveSessionStateObserver(this);
}

void MultiUserWindowManager::AddUser(const std::string& user_id) {
  if (user_id_to_app_observer_.find(user_id) != user_id_to_app_observer_.end())
    return;

  // Add an observer for all shell window changes.
  Profile* profile = GetProfileFromUserID(user_id);

  // In case of unit tests we might have no profile.
  if (!profile)
    return;

  user_id_to_app_observer_[user_id] = new AppObserver(user_id);
  apps::ShellWindowRegistry::Get(profile)->AddObserver(
      user_id_to_app_observer_[user_id]);

  // Account all existing application windows of this user accordingly.
  const apps::ShellWindowRegistry::ShellWindowList& shell_windows =
      apps::ShellWindowRegistry::Get(profile)->shell_windows();
  apps::ShellWindowRegistry::ShellWindowList::const_iterator it =
      shell_windows.begin();
  for (; it != shell_windows.end(); ++it)
    user_id_to_app_observer_[user_id]->OnShellWindowAdded(*it);

  // Account all existing browser windows of this user accordingly.
  BrowserList* browser_list = BrowserList::GetInstance(HOST_DESKTOP_TYPE_ASH);
  BrowserList::const_iterator browser_it = browser_list->begin();
  for (; browser_it != browser_list->end(); ++browser_it) {
    if ((*browser_it)->profile()->GetOriginalProfile() == profile)
      AddBrowserWindow(*browser_it);
  }
}

void MultiUserWindowManager::AddBrowserWindow(Browser* browser) {
  if (browser->window() && !browser->window()->GetNativeWindow())
    return;
  SetWindowOwner(browser->window()->GetNativeWindow(),
                 GetUserIDFromProfile(browser->profile()));
}

void MultiUserWindowManager::SetWindowVisibility(
    aura::Window* window, bool visible) {
  if (window->IsVisible() == visible)
    return;

  // To avoid that these commands are recorded as any other commands, we are
  // suppressing any window entry changes while this is going on.
  base::AutoReset<bool> suppressor(&suppress_visibility_changes_, true);

  if (visible)
    window->Show();
  else
    window->Hide();
}

Profile* MultiUserWindowManager::GetProfileFromUserID(
    const std::string& user_id) {
  // This can only happen for unit tests. If it happens we return NULL.
  if (!g_browser_process || !g_browser_process->profile_manager())
    return NULL;

  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();

  std::vector<Profile*>::iterator profile_iterator = profiles.begin();
  for (; profile_iterator != profiles.end(); ++profile_iterator) {
    if (GetUserIDFromProfile(*profile_iterator) == user_id)
      return *profile_iterator;
  }
  return NULL;
}

}  // namespace chrome
