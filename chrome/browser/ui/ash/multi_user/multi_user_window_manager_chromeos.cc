// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "ash/ash_switches.h"
#include "ash/multi_profile_uma.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_notification_blocker_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/user_switch_animator_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace {

// The animation time in milliseconds for a single window which is fading
// in / out.
const int kAnimationTimeMS = 100;

// The animation time in milliseconds for the fade in and / or out when
// switching users.
const int kUserFadeTimeMS = 110;

// The animation time in ms for a window which get teleported to another screen.
const int kTeleportAnimationTimeMS = 300;

// Checks if a given event is a user event.
bool IsUserEvent(const ui::Event* e) {
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
    if (IsUserEvent((*it)->GetHost()->dispatcher()->current_event()))
      return true;
  }
  return false;
}

// Records the type of window which was transferred to another desktop.
void RecordUMAForTransferredWindowType(aura::Window* window) {
  // We need to figure out what kind of window this is to record the transfer.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  ash::MultiProfileUMA::TeleportWindowType window_type =
      ash::MultiProfileUMA::TELEPORT_WINDOW_UNKNOWN;
  if (browser) {
    if (browser->profile()->IsOffTheRecord()) {
      window_type = ash::MultiProfileUMA::TELEPORT_WINDOW_INCOGNITO_BROWSER;
    } else if (browser->is_app()) {
      window_type = ash::MultiProfileUMA::TELEPORT_WINDOW_V1_APP;
    } else if (browser->is_type_popup()) {
      window_type = ash::MultiProfileUMA::TELEPORT_WINDOW_POPUP;
    } else {
      window_type = ash::MultiProfileUMA::TELEPORT_WINDOW_BROWSER;
    }
  } else {
    // Unit tests might come here without a profile manager.
    if (!g_browser_process->profile_manager())
      return;
    // If it is not a browser, it is probably be a V2 application. In that case
    // one of the AppWindowRegistry instances should know about it.
    apps::AppWindow* app_window = NULL;
    std::vector<Profile*> profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    for (std::vector<Profile*>::iterator it = profiles.begin();
         it != profiles.end() && app_window == NULL;
         it++) {
      app_window = apps::AppWindowRegistry::Get(*it)
                       ->GetAppWindowForNativeWindow(window);
    }
    if (app_window) {
      if (app_window->window_type() == apps::AppWindow::WINDOW_TYPE_PANEL ||
          app_window->window_type() == apps::AppWindow::WINDOW_TYPE_V1_PANEL) {
        window_type = ash::MultiProfileUMA::TELEPORT_WINDOW_PANEL;
      } else {
        window_type = ash::MultiProfileUMA::TELEPORT_WINDOW_V2_APP;
      }
    }
  }
  ash::MultiProfileUMA::RecordTeleportWindowType(window_type);
}

}  // namespace

namespace chrome {

// A class to temporarily change the animation properties for a window.
class AnimationSetter {
 public:
  AnimationSetter(aura::Window* window, int animation_time_in_ms)
      : window_(window),
        previous_animation_type_(
            wm::GetWindowVisibilityAnimationType(window_)),
        previous_animation_time_(
            wm::GetWindowVisibilityAnimationDuration(*window_)) {
    wm::SetWindowVisibilityAnimationType(
        window_,
        wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
    wm::SetWindowVisibilityAnimationDuration(
        window_,
        base::TimeDelta::FromMilliseconds(animation_time_in_ms));
  }

  ~AnimationSetter() {
    wm::SetWindowVisibilityAnimationType(window_,
                                                    previous_animation_type_);
    wm::SetWindowVisibilityAnimationDuration(
        window_,
        previous_animation_time_);
  }

 private:
  // The window which gets used.
  aura::Window* window_;

  // Previous animation type.
  const int previous_animation_type_;

  // Previous animation time.
  const base::TimeDelta previous_animation_time_;

  DISALLOW_COPY_AND_ASSIGN(AnimationSetter);
};

// This class keeps track of all applications which were started for a user.
// When an app gets created, the window will be tagged for that user. Note
// that the destruction does not need to be tracked here since the universal
// window observer will take care of that.
class AppObserver : public apps::AppWindowRegistry::Observer {
 public:
  explicit AppObserver(const std::string& user_id) : user_id_(user_id) {}
  virtual ~AppObserver() {}

  // AppWindowRegistry::Observer overrides:
  virtual void OnAppWindowAdded(apps::AppWindow* app_window) OVERRIDE {
    aura::Window* window = app_window->GetNativeWindow();
    DCHECK(window);
    MultiUserWindowManagerChromeOS::GetInstance()->SetWindowOwner(window,
                                                                  user_id_);
  }

 private:
  std::string user_id_;

  DISALLOW_COPY_AND_ASSIGN(AppObserver);
};

MultiUserWindowManagerChromeOS::MultiUserWindowManagerChromeOS(
    const std::string& current_user_id)
    : current_user_id_(current_user_id),
      notification_blocker_(new MultiUserNotificationBlockerChromeOS(
          message_center::MessageCenter::Get(), current_user_id)),
      suppress_visibility_changes_(false),
      animation_speed_(ANIMATION_SPEED_NORMAL) {
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
  // When the MultiUserWindowManager gets destroyed, ash::Shell is mostly gone.
  // As such we should not try to finalize any outstanding user animations.
  // Note that the destruction of the object can be done later.
  if (animation_.get())
    animation_->CancelAnimation();

  // Remove all window observers.
  WindowToEntryMap::iterator window = window_to_entry_.begin();
  while (window != window_to_entry_.end()) {
    OnWindowDestroyed(window->first);
    window = window_to_entry_.begin();
  }

  // Remove all app observers.
  UserIDToAppWindowObserver::iterator app_observer_iterator =
      user_id_to_app_observer_.begin();
  while (app_observer_iterator != user_id_to_app_observer_.end()) {
    Profile* profile = multi_user_util::GetProfileFromUserID(
        app_observer_iterator->first);
    DCHECK(profile);
    apps::AppWindowRegistry::Get(profile)
        ->RemoveObserver(app_observer_iterator->second);
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

  // Add observers to track state changes.
  window->AddObserver(this);
  wm::TransientWindowManager::Get(window)->AddObserver(this);

  // Check if this window was created due to a user interaction. If it was,
  // transfer it to the current user.
  if (IsProcessingUserEvent())
    window_to_entry_[window]->set_show_for_user(current_user_id_);

  // Add all transient children to our set of windows. Note that the function
  // will add the children but not the owner to the transient children map.
  AddTransientOwnerRecursive(window, window);

  // Notify entry adding.
  FOR_EACH_OBSERVER(Observer, observers_, OnOwnerEntryAdded(window));

  if (!IsWindowOnDesktopOfUser(window, current_user_id_))
    SetWindowVisibility(window, false, 0);
}

const std::string& MultiUserWindowManagerChromeOS::GetWindowOwner(
    aura::Window* window) const {
  WindowToEntryMap::const_iterator it = window_to_entry_.find(window);
  return it != window_to_entry_.end() ? it->second->owner()
                                      : base::EmptyString();
}

void MultiUserWindowManagerChromeOS::ShowWindowForUser(
    aura::Window* window,
    const std::string& user_id) {
  std::string previous_owner(GetUserPresentingWindow(window));
  if (!ShowWindowForUserIntern(window, user_id))
    return;
  // The window switched to a new desktop and we have to switch to that desktop,
  // but only when it was on the visible desktop and the the target is not the
  // visible desktop.
  if (user_id == current_user_id_ || previous_owner != current_user_id_)
    return;

  ash::Shell::GetInstance()->session_state_delegate()->SwitchActiveUser(
      user_id);
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
    std::set<std::string>* user_ids) const {
  for (WindowToEntryMap::const_iterator it = window_to_entry_.begin();
       it != window_to_entry_.end();
       ++it) {
    if (it->first->IsVisible())
      user_ids->insert(it->second->owner());
  }
}

bool MultiUserWindowManagerChromeOS::IsWindowOnDesktopOfUser(
    aura::Window* window,
    const std::string& user_id) const {
  const std::string& presenting_user = GetUserPresentingWindow(window);
  return presenting_user.empty() || presenting_user == user_id;
}

const std::string& MultiUserWindowManagerChromeOS::GetUserPresentingWindow(
    aura::Window* window) const {
  WindowToEntryMap::const_iterator it = window_to_entry_.find(window);
  // If the window is not owned by anyone it is shown on all desktops and we
  // return the empty string.
  if (it == window_to_entry_.end())
    return base::EmptyString();
  // Otherwise we ask the object for its desktop.
  return it->second->show_for_user();
}

void MultiUserWindowManagerChromeOS::AddUser(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  const std::string& user_id = multi_user_util::GetUserIDFromProfile(profile);
  if (user_id_to_app_observer_.find(user_id) != user_id_to_app_observer_.end())
    return;

  user_id_to_app_observer_[user_id] = new AppObserver(user_id);
  apps::AppWindowRegistry::Get(profile)
      ->AddObserver(user_id_to_app_observer_[user_id]);

  // Account all existing application windows of this user accordingly.
  const apps::AppWindowRegistry::AppWindowList& app_windows =
      apps::AppWindowRegistry::Get(profile)->app_windows();
  apps::AppWindowRegistry::AppWindowList::const_iterator it =
      app_windows.begin();
  for (; it != app_windows.end(); ++it)
    user_id_to_app_observer_[user_id]->OnAppWindowAdded(*it);

  // Account all existing browser windows of this user accordingly.
  BrowserList* browser_list = BrowserList::GetInstance(HOST_DESKTOP_TYPE_ASH);
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

void MultiUserWindowManagerChromeOS::ActiveUserChanged(
    const std::string& user_id) {
  DCHECK(user_id != current_user_id_);
  // This needs to be set before the animation starts.
  current_user_id_ = user_id;

  animation_.reset(
      new UserSwichAnimatorChromeOS(
          this, user_id, GetAdjustedAnimationTimeInMS(kUserFadeTimeMS)));
  // Call notifier here instead of observing ActiveUserChanged because
  // this must happen after MultiUserWindowManagerChromeOS is notified.
  ash::Shell::GetInstance()
      ->system_tray_notifier()
      ->NotifyMediaCaptureChanged();
}

void MultiUserWindowManagerChromeOS::OnWindowDestroyed(aura::Window* window) {
  if (GetWindowOwner(window).empty()) {
    // This must be a window in the transient chain - remove it and its
    // children from the owner.
    RemoveTransientOwnerRecursive(window);
    return;
  }
  wm::TransientWindowManager::Get(window)->RemoveObserver(this);
  // Remove the window from the owners list.
  delete window_to_entry_[window];
  window_to_entry_.erase(window);

  // Notify entry change.
  FOR_EACH_OBSERVER(Observer, observers_, OnOwnerEntryRemoved(window));
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
    SetWindowVisibility(window, false, 0);
    return;
  }
  aura::Window* owned_parent = GetOwningWindowInTransientChain(window);
  if (owned_parent && owned_parent != window && visible &&
      !IsWindowOnDesktopOfUser(owned_parent, current_user_id_))
    SetWindowVisibility(window, false, 0);
}

void MultiUserWindowManagerChromeOS::OnTransientChildAdded(
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

void MultiUserWindowManagerChromeOS::OnTransientChildRemoved(
    aura::Window* window,
    aura::Window* transient_window) {
  // Remove the transient child if the window itself is owned, or one of the
  // windows in its transient parents chain.
  if (!GetWindowOwner(window).empty() ||
      GetOwningWindowInTransientChain(window))
    RemoveTransientOwnerRecursive(transient_window);
}

void MultiUserWindowManagerChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == NOTIFICATION_BROWSER_WINDOW_READY)
    AddBrowserWindow(content::Source<Browser>(source).ptr());
}

void MultiUserWindowManagerChromeOS::SetAnimationSpeedForTest(
    MultiUserWindowManagerChromeOS::AnimationSpeed speed) {
  animation_speed_ = speed;
}

bool MultiUserWindowManagerChromeOS::IsAnimationRunningForTest() {
  return animation_.get() != NULL && !animation_->IsAnimationFinished();
}

const std::string& MultiUserWindowManagerChromeOS::GetCurrentUserForTest()
    const {
  return current_user_id_;
}

bool MultiUserWindowManagerChromeOS::ShowWindowForUserIntern(
    aura::Window* window,
    const std::string& user_id) {
  // If there is either no owner, or the owner is the current user, no action
  // is required.
  const std::string& owner = GetWindowOwner(window);
  if (owner.empty() ||
      (owner == user_id && IsWindowOnDesktopOfUser(window, user_id)))
    return false;

  bool minimized = ash::wm::GetWindowState(window)->IsMinimized();
  // Check that we are not trying to transfer ownership of a minimized window.
  if (user_id != owner && minimized)
    return false;

  if (minimized) {
    // If it is minimized it falls back to the original desktop.
    ash::MultiProfileUMA::RecordTeleportAction(
        ash::MultiProfileUMA::TELEPORT_WINDOW_RETURN_BY_MINIMIZE);
  } else {
    // If the window was transferred without getting minimized, we should record
    // the window type.
    RecordUMAForTransferredWindowType(window);
  }

  WindowToEntryMap::iterator it = window_to_entry_.find(window);
  it->second->set_show_for_user(user_id);

  // Show the window if the added user is the current one.
  if (user_id == current_user_id_) {
    // Only show the window if it should be shown according to its state.
    if (it->second->show())
      SetWindowVisibility(window, true, kTeleportAnimationTimeMS);
  } else {
    SetWindowVisibility(window, false, kTeleportAnimationTimeMS);
  }

  // Notify entry change.
  FOR_EACH_OBSERVER(Observer, observers_, OnOwnerEntryChanged(window));
  return true;
}

void MultiUserWindowManagerChromeOS::SetWindowVisibility(
    aura::Window* window, bool visible, int animation_time_in_ms) {
  if (window->IsVisible() == visible)
    return;

  // Hiding a system modal dialog should not be allowed. Instead we switch to
  // the user which is showing the system modal window.
  // Note that in some cases (e.g. unit test) windows might not have a root
  // window.
  if (!visible && window->GetRootWindow()) {
    // Get the system modal container for the window's root window.
    aura::Window* system_modal_container =
        window->GetRootWindow()->GetChildById(
            ash::kShellWindowId_SystemModalContainer);
    if (window->parent() == system_modal_container) {
      // The window is system modal and we need to find the parent which owns
      // it so that we can switch to the desktop accordingly.
      std::string user_id = GetUserPresentingWindow(window);
      if (user_id.empty()) {
        aura::Window* owning_window = GetOwningWindowInTransientChain(window);
        DCHECK(owning_window);
        user_id = GetUserPresentingWindow(owning_window);
        DCHECK(!user_id.empty());
      }
      ash::Shell::GetInstance()->session_state_delegate()->SwitchActiveUser(
          user_id);
      return;
    }
  }

  // To avoid that these commands are recorded as any other commands, we are
  // suppressing any window entry changes while this is going on.
  base::AutoReset<bool> suppressor(&suppress_visibility_changes_, true);

  if (visible) {
    ShowWithTransientChildrenRecursive(window, animation_time_in_ms);
  } else {
    if (window->HasFocus())
      window->Blur();
    SetWindowVisible(window, false, animation_time_in_ms);
  }
}

void MultiUserWindowManagerChromeOS::AddBrowserWindow(Browser* browser) {
  // A unit test (e.g. CrashRestoreComplexTest.RestoreSessionForThreeUsers) can
  // come here with no valid window.
  if (!browser->window() || !browser->window()->GetNativeWindow())
    return;
  SetWindowOwner(browser->window()->GetNativeWindow(),
                 multi_user_util::GetUserIDFromProfile(browser->profile()));
}

void MultiUserWindowManagerChromeOS::ShowWithTransientChildrenRecursive(
    aura::Window* window, int animation_time_in_ms) {
  aura::Window::Windows::const_iterator it =
      wm::GetTransientChildren(window).begin();
  for (; it != wm::GetTransientChildren(window).end(); ++it)
    ShowWithTransientChildrenRecursive(*it, animation_time_in_ms);

  // We show all children which were not explicitly hidden.
  TransientWindowToVisibility::iterator it2 =
      transient_window_to_visibility_.find(window);
  if (it2 == transient_window_to_visibility_.end() || it2->second)
    SetWindowVisible(window, true, animation_time_in_ms);
}

aura::Window* MultiUserWindowManagerChromeOS::GetOwningWindowInTransientChain(
    aura::Window* window) const {
  if (!GetWindowOwner(window).empty())
    return NULL;
  aura::Window* parent = wm::GetTransientParent(window);
  while (parent) {
    if (!GetWindowOwner(parent).empty())
      return parent;
    parent = wm::GetTransientParent(parent);
  }
  return NULL;
}

void MultiUserWindowManagerChromeOS::AddTransientOwnerRecursive(
    aura::Window* window,
    aura::Window* owned_parent) {
  // First add all child windows.
  aura::Window::Windows::const_iterator it =
      wm::GetTransientChildren(window).begin();
  for (; it != wm::GetTransientChildren(window).end(); ++it)
    AddTransientOwnerRecursive(*it, owned_parent);

  // If this window is the owned window, we do not have to handle it again.
  if (window == owned_parent)
    return;

  // Remember the current visibility.
  DCHECK(transient_window_to_visibility_.find(window) ==
             transient_window_to_visibility_.end());
  transient_window_to_visibility_[window] = window->IsVisible();

  // Add observers to track state changes.
  window->AddObserver(this);
  wm::TransientWindowManager::Get(window)->AddObserver(this);

  // Hide the window if it should not be shown. Note that this hide operation
  // will hide recursively this and all children - but we have already collected
  // their initial view state.
  if (!IsWindowOnDesktopOfUser(owned_parent, current_user_id_))
    SetWindowVisibility(window, false, kAnimationTimeMS);
}

void MultiUserWindowManagerChromeOS::RemoveTransientOwnerRecursive(
    aura::Window* window) {
  // First remove all child windows.
  aura::Window::Windows::const_iterator it =
      wm::GetTransientChildren(window).begin();
  for (; it != wm::GetTransientChildren(window).end(); ++it)
    RemoveTransientOwnerRecursive(*it);

  // Find from transient window storage the visibility for the given window,
  // set the visibility accordingly and delete the window from the map.
  TransientWindowToVisibility::iterator visibility_item =
      transient_window_to_visibility_.find(window);
  DCHECK(visibility_item != transient_window_to_visibility_.end());

  window->RemoveObserver(this);
  wm::TransientWindowManager::Get(window)->RemoveObserver(this);

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

void MultiUserWindowManagerChromeOS::SetWindowVisible(
    aura::Window* window,
    bool visible,
    int animation_time_in_ms) {
  // The MaximizeModeWindowManager will not handle invisible windows since they
  // are not user activatable. Since invisible windows are not being tracked,
  // we tell it to maximize / track this window now before it gets shown, to
  // reduce animation jank from multiple resizes.
  if (visible)
    ash::Shell::GetInstance()->maximize_mode_controller()->AddWindow(window);

  AnimationSetter animation_setter(
      window,
      GetAdjustedAnimationTimeInMS(animation_time_in_ms));

  if (visible)
    window->Show();
  else
    window->Hide();

  // Make sure that animations have no influence on the window state after the
  // call.
  DCHECK_EQ(visible, window->IsVisible());
}

int MultiUserWindowManagerChromeOS::GetAdjustedAnimationTimeInMS(
    int default_time_in_ms) const {
  return animation_speed_ == ANIMATION_SPEED_NORMAL ? default_time_in_ms :
      (animation_speed_ == ANIMATION_SPEED_FAST ? 10 : 0);
}

}  // namespace chrome
