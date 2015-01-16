// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/user_switch_animator_chromeos.h"

#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_notification_blocker_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "ui/wm/public/activation_client.h"

namespace chrome {

namespace {

// The minimal possible animation time for animations which should happen
// "instantly".
const int kMinimalAnimationTimeMS = 1;

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

}  // namespace

UserSwichAnimatorChromeOS::UserSwichAnimatorChromeOS(
    MultiUserWindowManagerChromeOS* owner,
    const std::string& new_user_id,
    int animation_speed_ms)
    : owner_(owner),
      new_user_id_(new_user_id),
      animation_speed_ms_(animation_speed_ms),
      animation_step_(ANIMATION_STEP_HIDE_OLD_USER),
      screen_cover_(GetScreenCover(NULL)) {
  AdvanceUserTransitionAnimation();

  if (!animation_speed_ms_) {
    FinalizeAnimation();
  } else {
    user_changed_animation_timer_.reset(new base::Timer(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(animation_speed_ms_),
        base::Bind(
            &UserSwichAnimatorChromeOS::AdvanceUserTransitionAnimation,
            base::Unretained(this)),
        true));
    user_changed_animation_timer_->Reset();
  }
}

UserSwichAnimatorChromeOS::~UserSwichAnimatorChromeOS() {
  FinalizeAnimation();
}

// static
bool UserSwichAnimatorChromeOS::CoversScreen(aura::Window* window) {
  // Full screen covers the screen naturally. Since a normal window can have the
  // same size as the work area, we only compare the bounds against the work
  // area.
  if (ash::wm::GetWindowState(window)->IsFullscreen())
    return true;
  gfx::Rect bounds = window->GetBoundsInRootWindow();
  gfx::Rect work_area = gfx::Screen::GetScreenFor(window)->
      GetDisplayNearestWindow(window).work_area();
  bounds.Intersect(work_area);
  return work_area == bounds;
}

void UserSwichAnimatorChromeOS::AdvanceUserTransitionAnimation() {
  DCHECK_NE(animation_step_, ANIMATION_STEP_ENDED);

  TransitionWallpaper(animation_step_);
  TransitionUserShelf(animation_step_);
  TransitionWindows(animation_step_);

  // Advance to the next step.
  switch (animation_step_) {
    case ANIMATION_STEP_HIDE_OLD_USER:
      animation_step_ = ANIMATION_STEP_SHOW_NEW_USER;
      break;
    case ANIMATION_STEP_SHOW_NEW_USER:
      animation_step_ = ANIMATION_STEP_FINALIZE;
      break;
    case ANIMATION_STEP_FINALIZE:
      user_changed_animation_timer_.reset();
      animation_step_ = ANIMATION_STEP_ENDED;
      break;
    case ANIMATION_STEP_ENDED:
      NOTREACHED();
      break;
  }
}

void UserSwichAnimatorChromeOS::CancelAnimation() {
  animation_step_ = ANIMATION_STEP_ENDED;
}

void UserSwichAnimatorChromeOS::FinalizeAnimation() {
  user_changed_animation_timer_.reset();
  while (ANIMATION_STEP_ENDED != animation_step_)
    AdvanceUserTransitionAnimation();
}

void UserSwichAnimatorChromeOS::TransitionWallpaper(
    AnimationStep animation_step) {
  // Handle the wallpaper switch.
  ash::UserWallpaperDelegate* wallpaper_delegate =
      ash::Shell::GetInstance()->user_wallpaper_delegate();
  if (animation_step == ANIMATION_STEP_HIDE_OLD_USER) {
    // Set the wallpaper cross dissolve animation duration to our complete
    // animation cycle for a fade in and fade out.
    int duration =
        NO_USER_COVERS_SCREEN == screen_cover_ ? (2 * animation_speed_ms_) : 0;
    wallpaper_delegate->SetAnimationDurationOverride(
        std::max(duration, kMinimalAnimationTimeMS));
    if (screen_cover_ != NEW_USER_COVERS_SCREEN) {
      chromeos::WallpaperManager::Get()->SetUserWallpaperNow(new_user_id_);
      wallpaper_user_id_ =
          (NO_USER_COVERS_SCREEN == screen_cover_ ? "->" : "") +
          new_user_id_;
    }
  } else if (animation_step == ANIMATION_STEP_FINALIZE) {
    // Revert the wallpaper cross dissolve animation duration back to the
    // default.
    if (screen_cover_ == NEW_USER_COVERS_SCREEN)
      chromeos::WallpaperManager::Get()->SetUserWallpaperNow(new_user_id_);

    // Coming here the wallpaper user id is the final result. No matter how we
    // got here.
    wallpaper_user_id_ = new_user_id_;
    wallpaper_delegate->SetAnimationDurationOverride(0);
  }
}

void UserSwichAnimatorChromeOS::TransitionUserShelf(
    AnimationStep animation_step) {
  ChromeLauncherController* chrome_launcher_controller =
      ChromeLauncherController::instance();
  // The shelf animation duration override.
  int duration_override = animation_speed_ms_;
  // Handle the shelf order of items. This is done once the old user is hidden.
  if (animation_step == ANIMATION_STEP_SHOW_NEW_USER) {
    // Some unit tests have no ChromeLauncherController.
    if (chrome_launcher_controller)
      chrome_launcher_controller->ActiveUserChanged(new_user_id_);
    // Hide the black rectangle on top of each shelf again.
    aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
    for (aura::Window::Windows::const_iterator iter = root_windows.begin();
         iter != root_windows.end(); ++iter) {
      ash::ShelfWidget* shelf =
          ash::RootWindowController::ForWindow(*iter)->shelf();
      shelf->HideShelfBehindBlackBar(false, duration_override);
    }
    // We kicked off the shelf animation above and the override can be
    // removed.
    duration_override = 0;
  }

  if (!animation_speed_ms_ || animation_step == ANIMATION_STEP_FINALIZE)
    return;

  // Note: The animation duration override will be set before the old user gets
  // hidden and reset after the animations for the new user got kicked off.
  ash::Shell::RootWindowControllerList controller =
      ash::Shell::GetInstance()->GetAllRootWindowControllers();
  for (ash::Shell::RootWindowControllerList::iterator iter = controller.begin();
       iter != controller.end(); ++iter) {
    (*iter)->GetShelfLayoutManager()->SetAnimationDurationOverride(
        duration_override);
  }

  if (animation_step != ANIMATION_STEP_HIDE_OLD_USER)
    return;

  // For each root window hide the shelf.
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  for (aura::Window::Windows::const_iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    // Hiding the shelf will cause a resize on a maximized window.
    // If the shelf is then shown for the following user in the same location,
    // the window gets resized again. Since each resize can cause a considerable
    // CPU usage and therefore effect jank, we should avoid hiding the shelf if
    // the start and end location are the same and cover the shelf instead with
    // a black rectangle on top.
    if (GetScreenCover(*iter) != NO_USER_COVERS_SCREEN &&
        (!chrome_launcher_controller ||
         !chrome_launcher_controller->ShelfBoundsChangesProbablyWithUser(
             *iter, new_user_id_))) {
      ash::ShelfWidget* shelf =
          ash::RootWindowController::ForWindow(*iter)->shelf();
      shelf->HideShelfBehindBlackBar(true, duration_override);
    } else {
      // This shelf change is only part of the animation and will be updated by
      // ChromeLauncherController::ActiveUserChanged() to the new users value.
      // Note that the user preference will not be changed.
      ash::Shell::GetInstance()->SetShelfAutoHideBehavior(
          ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN, *iter);
    }
  }
}

void UserSwichAnimatorChromeOS::TransitionWindows(
    AnimationStep animation_step) {
  // Disable the window position manager and the MRU window tracker temporarily.
  UserChangeActionDisabler disabler;

  if (animation_step == ANIMATION_STEP_HIDE_OLD_USER ||
      (animation_step == ANIMATION_STEP_FINALIZE &&
       screen_cover_ == BOTH_USERS_COVER_SCREEN)) {
    // We need to show/hide the windows in the same order as they were created
    // in their parent window(s) to keep the layer / window hierarchy in sync.
    // To achieve that we first collect all parent windows and then enumerate
    // all windows in those parent windows and show or hide them accordingly.

    // Create a list of all parent windows we have to check.
    std::set<aura::Window*> parent_list;
    for (MultiUserWindowManagerChromeOS::WindowToEntryMap::const_iterator it =
             owner_->window_to_entry().begin();
         it != owner_->window_to_entry().end(); ++it) {
      aura::Window* parent = it->first->parent();
      if (parent_list.find(parent) == parent_list.end())
        parent_list.insert(parent);
    }

    for (std::set<aura::Window*>::iterator it_parents = parent_list.begin();
         it_parents != parent_list.end(); ++it_parents) {
      const aura::Window::Windows window_list = (*it_parents)->children();
      // In case of |BOTH_USERS_COVER_SCREEN| the desktop might shine through
      // if all windows fade (in or out). To avoid this we only fade the topmost
      // covering window (in / out) and make / keep all other covering windows
      // visible while animating. |foreground_window_found| will get set when
      // the top fading window was found.
      bool foreground_window_found = false;
      // Covering windows which follow the fade direction will also fade - all
      // others will get immediately shown / kept shown until the animation is
      // finished.
      bool foreground_becomes_visible = false;
      for (aura::Window::Windows::const_reverse_iterator it_window =
               window_list.rbegin();
           it_window != window_list.rend(); ++it_window) {
        aura::Window* window = *it_window;
        MultiUserWindowManagerChromeOS::WindowToEntryMap::const_iterator
            it_map = owner_->window_to_entry().find(window);
        if (it_map != owner_->window_to_entry().end()) {
          bool should_be_visible =
              it_map->second->show_for_user() == new_user_id_ &&
              it_map->second->show();
          bool is_visible = window->IsVisible();
          ash::wm::WindowState* window_state = ash::wm::GetWindowState(window);
          if (it_map->second->owner() == new_user_id_ &&
              it_map->second->show_for_user() != new_user_id_ &&
              window_state->IsMinimized()) {
            // Pull back minimized visiting windows to the owners desktop.
            owner_->ShowWindowForUserIntern(window, new_user_id_);
            window_state->Unminimize();
          } else if (should_be_visible != is_visible) {
            bool animate = true;
            int duration = animation_step == ANIMATION_STEP_FINALIZE ?
                               0 : (2 * animation_speed_ms_);
            duration = std::max(kMinimalAnimationTimeMS, duration);
            if (animation_step != ANIMATION_STEP_FINALIZE &&
                screen_cover_ == BOTH_USERS_COVER_SCREEN &&
                CoversScreen(window)) {
              if (!foreground_window_found) {
                foreground_window_found = true;
                foreground_becomes_visible = should_be_visible;
              } else if (should_be_visible != foreground_becomes_visible) {
                // Covering windows behind the foreground window which are
                // inverting their visibility should immediately become visible
                // or stay visible until the animation is finished.
                duration = kMinimalAnimationTimeMS;
                if (!should_be_visible)
                  animate = false;
              }
            }
            if (animate)
              owner_->SetWindowVisibility(window, should_be_visible, duration);
          }
        }
      }
    }
  }

  // Activation and real switch are happening after the other user gets shown.
  if (animation_step == ANIMATION_STEP_SHOW_NEW_USER) {
    // Finally we need to restore the previously active window.
    ash::MruWindowTracker::WindowList mru_list =
        ash::Shell::GetInstance()->mru_window_tracker()->BuildMruWindowList();
    if (!mru_list.empty()) {
      aura::Window* window = mru_list[0];
      ash::wm::WindowState* window_state = ash::wm::GetWindowState(window);
      if (owner_->IsWindowOnDesktopOfUser(window, new_user_id_) &&
          !window_state->IsMinimized()) {
        aura::client::ActivationClient* client =
            aura::client::GetActivationClient(window->GetRootWindow());
        // Several unit tests come here without an activation client.
        if (client)
          client->ActivateWindow(window);
      }
    }

    // This is called directly here to make sure notification_blocker will see
    // the new window status.
    owner_->notification_blocker()->ActiveUserChanged(new_user_id_);
  }
}

UserSwichAnimatorChromeOS::TransitioningScreenCover
UserSwichAnimatorChromeOS::GetScreenCover(aura::Window* root_window) {
  TransitioningScreenCover cover = NO_USER_COVERS_SCREEN;
  for (MultiUserWindowManagerChromeOS::WindowToEntryMap::const_iterator it_map =
           owner_->window_to_entry().begin();
       it_map != owner_->window_to_entry().end();
       ++it_map) {
    aura::Window* window = it_map->first;
    if (root_window && window->GetRootWindow() != root_window)
      continue;
    if (window->IsVisible() && CoversScreen(window)) {
      if (cover == NEW_USER_COVERS_SCREEN)
        return BOTH_USERS_COVER_SCREEN;
      else
        cover = OLD_USER_COVERS_SCREEN;
    } else if (owner_->IsWindowOnDesktopOfUser(window, new_user_id_) &&
               CoversScreen(window)) {
      if (cover == OLD_USER_COVERS_SCREEN)
        return BOTH_USERS_COVER_SCREEN;
      else
        cover = NEW_USER_COVERS_SCREEN;
    }
  }
  return cover;
}

}  // namespace chrome
