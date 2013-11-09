// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "ash/ash_switches.h"
#include "ash/multi_profile_uma.h"
#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"

namespace {

// Checks if a given event is a user event.
bool IsUserEvent(ui::Event* e) {
  if (e) {
    ui::EventType type = e->type();
    if (type != ui::ET_CANCEL_MODE &&
        type != ui::ET_UMA_DATA &&
        type != ui::ET_UNKNOWN)
      return true;
  }
  return false;
}

// Test if we are currently processing a user event which might lead to a
// browser / app creation.
bool IsProcessingUserEvent() {
  // When there is a nested message loop (e.g. active menu or drag and drop
  // operation) - we are in a nested loop and can ignore this.
  // Note: Unit tests might not have a message loop.
  base::MessageLoop* message_loop = base::MessageLoop::current();
  if (message_loop && message_loop->is_running() && message_loop->IsNested())
    return false;

  // TODO(skuhne): "Open link in new window" will come here after the menu got
  // closed, executing the command from the nested menu loop. However at that
  // time there is no active event processed. A solution for that need to be
  // found past M-32. A global event handler filter (pre and post) might fix
  // that problem in conjunction with a depth counter - but - for the menu
  // execution we come here after the loop was finished (so it's not nested
  // anymore) and the root window should therefore still have the event which
  // lead to the menu invocation, but it is not. By fixing that problem this
  // would "magically work".
  aura::Window::Windows root_window_list = ash::Shell::GetAllRootWindows();
  for (aura::Window::Windows::iterator it = root_window_list.begin();
       it != root_window_list.end();
       ++it) {
    if (IsUserEvent((*it)->GetDispatcher()->current_event()))
      return true;
  }
  return false;
}

}  // namespace

namespace chrome {

// logic while the user gets switched.
class UserChangeActionDisabler {
 public:
  UserChangeActionDisabler() {
    ash::WindowPositioner::DisableAutoPositioning(true);
    ash::Shell::GetInstance()->mru_window_tracker()->SetIgnoreActivations(true);
  }

  ~UserChangeActionDisabler() {
    ash::WindowPositioner::DisableAutoPositioning(false);
    ash::Shell::GetInstance()->mru_window_tracker()->SetIgnoreActivations(
        false);
  }
 private:

  DISALLOW_COPY_AND_ASSIGN(UserChangeActionDisabler);
};

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
    MultiUserWindowManagerChromeOS::GetInstance()->SetWindowOwner(window,
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

MultiUserWindowManagerChromeOS::MultiUserWindowManagerChromeOS(
    const std::string& current_user_id)
    : current_user_id_(current_user_id),
      suppress_visibility_changes_(false) {
  // Add a session state observer to be able to monitor session changes.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->session_state_delegate()->
        AddSessionStateObserver(this);

  // The BrowserListObserver would have been better to use then the old
  // notification system, but that observer fires before the window got created.
  registrar_.Add(this, NOTIFICATION_BROWSER_WINDOW_READY,
                 content::NotificationService::AllSources());

  // Add an app window observer & all already running apps.
  Profile* profile = multi_user_util::GetProfileFromUserID(current_user_id);
  if (profile)
    AddUser(profile);
}

MultiUserWindowManagerChromeOS::~MultiUserWindowManagerChromeOS() {
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
    Profile* profile = multi_user_util::GetProfileFromUserID(
        app_observer_iterator->first);
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

void MultiUserWindowManagerChromeOS::SetWindowOwner(
    aura::Window* window,
    const std::string& user_id) {
  // Make sure the window is valid and there was no owner yet.
  DCHECK(window);
  DCHECK(!user_id.empty());
  if (GetWindowOwner(window) == user_id)
    return;
  DCHECK(GetWindowOwner(window).empty());
  window_to_entry_[window] = new WindowEntry(user_id);

  // Remember the initial visibility of the window.
  window_to_entry_[window]->set_show(window->IsVisible());

  // Set the window and the state observer.
  window->AddObserver(this);
  ash::wm::GetWindowState(window)->AddObserver(this);

  // Check if this window was created due to a user interaction. If it was,
  // transfer it to the current user.
  if (IsProcessingUserEvent())
    window_to_entry_[window]->set_show_for_user(current_user_id_);

  // Add all transient children to our set of windows. Note that the function
  // will add the children but not the owner to the transient children map.
  AddTransientOwnerRecursive(window, window);

  if (!IsWindowOnDesktopOfUser(window, current_user_id_))
    SetWindowVisibility(window, false);
}

const std::string& MultiUserWindowManagerChromeOS::GetWindowOwner(
    aura::Window* window) {
  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  return it != window_to_entry_.end() ? it->second->owner() : EmptyString();
}

void MultiUserWindowManagerChromeOS::ShowWindowForUser(
    aura::Window* window,
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

  ash::MultiProfileUMA::RecordTeleportAction(
      ash::MultiProfileUMA::TELEPORT_WINDOW_RETURN_BY_MINIMIZE);

  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  it->second->set_show_for_user(user_id);

  // Show the window if the added user is the current one.
  if (user_id == current_user_id_) {
    // Only show the window if it should be shown according to its state.
    if (it->second->show())
      SetWindowVisibility(window, true);
  } else {
    SetWindowVisibility(window, false);
  }
}

bool MultiUserWindowManagerChromeOS::AreWindowsSharedAmongUsers() {
  WindowToEntryMap::iterator it = window_to_entry_.begin();
  for (; it != window_to_entry_.end(); ++it) {
    if (it->second->owner() != it->second->show_for_user())
      return true;
  }
  return false;
}

bool MultiUserWindowManagerChromeOS::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const std::string& user_id) {
  const std::string& presenting_user = GetUserPresentingWindow(window);
  return presenting_user.empty() || presenting_user == user_id;
}

const std::string& MultiUserWindowManagerChromeOS::GetUserPresentingWindow(
    aura::Window* window) {
  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  // If the window is not owned by anyone it is shown on all desktops and we
  // return the empty string.
  if (it == window_to_entry_.end())
    return EmptyString();
  // Otherwise we ask the object for its desktop.
  return it->second->show_for_user();
}

void MultiUserWindowManagerChromeOS::AddUser(Profile* profile) {
  const std::string& user_id = multi_user_util::GetUserIDFromProfile(profile);
  if (user_id_to_app_observer_.find(user_id) != user_id_to_app_observer_.end())
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

void MultiUserWindowManagerChromeOS::ActiveUserChanged(
    const std::string& user_id) {
  DCHECK(user_id != current_user_id_);
  std::string old_user = current_user_id_;
  current_user_id_ = user_id;
  // Disable the window position manager and the MRU window tracker temporarily.
  scoped_ptr<UserChangeActionDisabler> disabler(new UserChangeActionDisabler);

  // We need to show/hide the windows in the same order as they were created in
  // their parent window(s) to keep the layer / window hierarchy in sync. To
  // achieve that we first collect all parent windows and then enumerate all
  // windows in those parent windows and show or hide them accordingly.

  // Create a list of all parent windows we have to check and their parents.
  std::set<aura::Window*> parent_list;
  for (WindowToEntryMap::iterator it = window_to_entry_.begin();
       it != window_to_entry_.end(); ++it) {
    aura::Window* parent = it->first->parent();
    if (parent_list.find(parent) == parent_list.end())
      parent_list.insert(parent);
  }

  // Traverse the found parent windows to handle their child windows in order of
  // their appearance.
  for (std::set<aura::Window*>::iterator it_parents = parent_list.begin();
       it_parents != parent_list.end(); ++it_parents) {
    const aura::Window::Windows window_list = (*it_parents)->children();
    for (aura::Window::Windows::const_iterator it_window = window_list.begin();
         it_window != window_list.end(); ++it_window) {
      aura::Window* window = *it_window;
      WindowToEntryMap::iterator it_map = window_to_entry_.find(window);
      if (it_map != window_to_entry_.end()) {
        bool should_be_visible = it_map->second->show_for_user() == user_id &&
                                 it_map->second->show();
        bool is_visible = window->IsVisible();
        if (should_be_visible != is_visible)
          SetWindowVisibility(window, should_be_visible);
      }
    }
  }

  // Finally we need to restore the previously active window.
  ash::MruWindowTracker::WindowList mru_list =
      ash::Shell::GetInstance()->mru_window_tracker()->BuildMruWindowList();
  if (mru_list.size()) {
    aura::Window* window = mru_list[0];
    ash::wm::WindowState* window_state = ash::wm::GetWindowState(window);
    if (IsWindowOnDesktopOfUser(window, user_id) &&
        !window_state->IsMinimized()) {
      aura::client::ActivationClient* client =
          aura::client::GetActivationClient(window->GetRootWindow());
      // Several unit tests come here without an activation client.
      if (client)
        client->ActivateWindow(window);
    }
  }
}

void MultiUserWindowManagerChromeOS::OnWindowDestroyed(aura::Window* window) {
  if (GetWindowOwner(window).empty()) {
    // This must be a window in the transient chain - remove it and its
    // children from the owner.
    RemoveTransientOwnerRecursive(window);
    return;
  }
  // Remove the state and the window observer.
  ash::wm::GetWindowState(window)->RemoveObserver(this);
  window->RemoveObserver(this);
  // Remove the window from the owners list.
  delete window_to_entry_[window];
  window_to_entry_.erase(window);
}

void MultiUserWindowManagerChromeOS::OnWindowVisibilityChanging(
    aura::Window* window, bool visible) {
  // This command gets called first and immediately when show or hide gets
  // called. We remember here the desired state for restoration IF we were
  // not ourselves issuing the call.
  // Note also that using the OnWindowVisibilityChanged callback cannot be
  // used for this.
  if (suppress_visibility_changes_)
    return;

  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  // If the window is not owned by anyone it is shown on all desktops.
  if (it != window_to_entry_.end()) {
    // Remember what was asked for so that we can restore this when the user's
    // desktop gets restored.
    it->second->set_show(visible);
  } else {
    TransientWindowToVisibility::iterator it =
        transient_window_to_visibility_.find(window);
    if (it != transient_window_to_visibility_.end())
      it->second = visible;
  }
}

void MultiUserWindowManagerChromeOS::OnWindowVisibilityChanged(
    aura::Window* window, bool visible) {
  if (suppress_visibility_changes_)
    return;

  // Don't allow to make the window visible if it shouldn't be.
  if (visible && !IsWindowOnDesktopOfUser(window, current_user_id_)) {
    SetWindowVisibility(window, false);
    return;
  }
  aura::Window* owned_parent = GetOwningWindowInTransientChain(window);
  if (owned_parent && owned_parent != window && visible &&
      !IsWindowOnDesktopOfUser(owned_parent, current_user_id_))
    SetWindowVisibility(window, false);
}

void MultiUserWindowManagerChromeOS::OnAddTransientChild(
    aura::Window* window,
    aura::Window* transient_window) {
  if (!GetWindowOwner(window).empty()) {
    AddTransientOwnerRecursive(transient_window, window);
    return;
  }
  aura::Window* owned_parent =
      GetOwningWindowInTransientChain(transient_window);
  if (!owned_parent)
    return;

  AddTransientOwnerRecursive(transient_window, owned_parent);
}

void MultiUserWindowManagerChromeOS::OnRemoveTransientChild(
    aura::Window* window,
    aura::Window* transient_window) {
  // Remove the transient child if the window itself is owned, or one of the
  // windows in its transient parents chain.
  if (!GetWindowOwner(window).empty() ||
      GetOwningWindowInTransientChain(window))
    RemoveTransientOwnerRecursive(transient_window);
}

void MultiUserWindowManagerChromeOS::OnWindowShowTypeChanged(
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

void MultiUserWindowManagerChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == NOTIFICATION_BROWSER_WINDOW_READY)
    AddBrowserWindow(content::Source<Browser>(source).ptr());
}

void MultiUserWindowManagerChromeOS::AddBrowserWindow(Browser* browser) {
  // A unit test (e.g. CrashRestoreComplexTest.RestoreSessionForThreeUsers) can
  // come here with no valid window.
  if (!browser->window() || !browser->window()->GetNativeWindow())
    return;
  SetWindowOwner(browser->window()->GetNativeWindow(),
                 multi_user_util::GetUserIDFromProfile(browser->profile()));
}

void MultiUserWindowManagerChromeOS::SetWindowVisibility(
    aura::Window* window, bool visible) {
  if (window->IsVisible() == visible)
    return;

  // To avoid that these commands are recorded as any other commands, we are
  // suppressing any window entry changes while this is going on.
  base::AutoReset<bool> suppressor(&suppress_visibility_changes_, true);

  if (visible) {
    ShowWithTransientChildrenRecursive(window);
  } else {
    if (window->HasFocus())
      window->Blur();
    window->Hide();
  }
}

void MultiUserWindowManagerChromeOS::ShowWithTransientChildrenRecursive(
    aura::Window* window) {
  aura::Window::Windows::const_iterator it =
      window->transient_children().begin();
  for (; it !=  window->transient_children().end(); ++it)
    ShowWithTransientChildrenRecursive(*it);

  // We show all children which were not explicitly hidden.
  TransientWindowToVisibility::iterator it2 =
      transient_window_to_visibility_.find(window);
  if (it2 == transient_window_to_visibility_.end() || it2->second)
    window->Show();
}

aura::Window* MultiUserWindowManagerChromeOS::GetOwningWindowInTransientChain(
    aura::Window* window) {
  if (!GetWindowOwner(window).empty())
    return NULL;
  aura::Window* parent = window->transient_parent();
  while (parent) {
    if (!GetWindowOwner(parent).empty())
      return parent;
    parent = parent->transient_parent();
  }
  return NULL;
}

void MultiUserWindowManagerChromeOS::AddTransientOwnerRecursive(
    aura::Window* window,
    aura::Window* owned_parent) {
  // First add all child windows.
  aura::Window::Windows::const_iterator it =
      window->transient_children().begin();
  for (; it !=  window->transient_children().end(); ++it)
    AddTransientOwnerRecursive(*it, owned_parent);

  // If this window is the owned window, we do not have to handle it again.
  if (window == owned_parent)
    return;

  // Remember the current visibility.
  DCHECK(transient_window_to_visibility_.find(window) ==
             transient_window_to_visibility_.end());
  transient_window_to_visibility_[window] = window->IsVisible();

  // Add a window observer to make sure that we catch status changes.
  window->AddObserver(this);

  // Hide the window if it should not be shown. Note that this hide operation
  // will hide recursively this and all children - but we have already collected
  // their initial view state.
  if (!IsWindowOnDesktopOfUser(owned_parent, current_user_id_))
    SetWindowVisibility(window, false);
}

void MultiUserWindowManagerChromeOS::RemoveTransientOwnerRecursive(
    aura::Window* window) {
  // First remove all child windows.
  aura::Window::Windows::const_iterator it =
      window->transient_children().begin();
  for (; it !=  window->transient_children().end(); ++it)
    RemoveTransientOwnerRecursive(*it);

  // Find from transient window storage the visibility for the given window,
  // set the visibility accordingly and delete the window from the map.
  TransientWindowToVisibility::iterator visibility_item =
      transient_window_to_visibility_.find(window);
  DCHECK(visibility_item != transient_window_to_visibility_.end());

  // Remove the window observer.
  window->RemoveObserver(this);

  bool unowned_view_state = visibility_item->second;
  transient_window_to_visibility_.erase(visibility_item);
  if (unowned_view_state && !window->IsVisible()) {
    // To prevent these commands from being recorded as any other commands, we
    // are suppressing any window entry changes while this is going on.
    // Instead of calling SetWindowVisible, only show gets called here since all
    // dependents have been shown previously already.
    base::AutoReset<bool> suppressor(&suppress_visibility_changes_, true);
    window->Show();
  }
}

}  // namespace chrome
