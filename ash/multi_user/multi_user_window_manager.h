// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_H_
#define ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/public/interfaces/multi_user_window_manager.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "services/ws/common/types.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/transient_window_observer.h"

namespace ash {

class MultiUserWindowManagerDelegateClassic;
class UserSwitchAnimator;

// MultiUserWindowManager associates windows with users and ensures the
// appropriate set of windows are visible at the right time.
// MultiUserWindowManager must be explicitly told about the windows to manage.
// This is done by way of SetWindowOwner().
//
// Each window may be associated with two accounts. The owning account (the
// account supplied to SetWindowOwner()), and an account the window is shown
// with when the account is active. Typically the 'shown' account and 'owning'
// account are the same, but the user may choose to show a window from an other
// account, in which case the 'shown' account changes.
//
// MultiUserWindowManager makes use of the following client/delegate interfaces
// mojom::MultiUserWindowManagerClient: used for windows created by the window
// service, as well as major state changes (such as animation changing).
// MultiUserWindowManagerDelegateClassic: used for all other windows. See
// MultiUserWindowManagerDelegateClassic for details on what this means.
//
// Note:
// - aura::Window::Hide() is currently hiding the window and all owned transient
//   children. However aura::Window::Show() is only showing the window itself.
//   To address that, all transient children (and their children) are remembered
//   in |transient_window_to_visibility_| and monitored to keep track of the
//   visibility changes from the owning user. This way the visibility can be
//   changed back to its requested state upon showing by us - or when the window
//   gets detached from its current owning parent.
class ASH_EXPORT MultiUserWindowManager : public SessionObserver,
                                          public aura::WindowObserver,
                                          public ::wm::TransientWindowObserver,
                                          public TabletModeObserver {
 public:
  // The speed which should be used to perform animations.
  enum AnimationSpeed {
    ANIMATION_SPEED_NORMAL,   // The normal animation speed.
    ANIMATION_SPEED_FAST,     // Unit test speed which test animations.
    ANIMATION_SPEED_DISABLED  // Unit tests which do not require animations.
  };

  MultiUserWindowManager(
      mojom::MultiUserWindowManagerClient* client,
      MultiUserWindowManagerDelegateClassic* classic_delegate,
      const AccountId& account_id);
  ~MultiUserWindowManager() override;

  static MultiUserWindowManager* Get();

  // Resets the client. This is called when running in mash. In single-process
  // mash, the browser creates this class (with no client) and
  // MultiUserWindowManagerBridge sets the client (as the client is provided
  // over mojom). In multi-process mash, MultiUserWindowManagerBridge creates
  // this and sets the client. This function is only necessary until
  // multi-process mash is the default.
  void SetClient(mojom::MultiUserWindowManagerClient* client);

  // Associates a window with a particular account. This may result in hiding
  // |window|. This should *not* be called more than once with a different
  // account. If |show_for_current_user| is true, this sets the 'shown'
  // account to the current account. If |window_id| is valid, changes to
  // |window| are notified through MultiUserWindowManagerClient. If |window_id|
  // is empty, MultiUserWindowManagerDelegateClassic is used.
  void SetWindowOwner(aura::Window* window,
                      const AccountId& account_id,
                      bool show_for_current_user,
                      base::Optional<ws::Id> window_id = base::nullopt);

  // Sets the 'shown' account for a window. See class description for details on
  // what the 'shown' account is. This function may trigger changing the active
  // user. When the window is minimized, the 'shown' account is reset to the
  // 'owning' account.
  void ShowWindowForUser(aura::Window* window, const AccountId& account_id);

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // WindowObserver overrides:
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // TransientWindowObserver overrides:
  void OnTransientChildAdded(aura::Window* window,
                             aura::Window* transient) override;
  void OnTransientChildRemoved(aura::Window* window,
                               aura::Window* transient) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;

  // Disable any animations for unit tests.
  void SetAnimationSpeedForTest(AnimationSpeed speed);

  // Returns true when a user switch animation is running. For unit tests.
  bool IsAnimationRunningForTest();

  // Returns the current user for unit tests.
  const AccountId& GetCurrentUserForTest() const;

 private:
  friend class MultiUserWindowManagerClientImplTest;
  friend class UserSwitchAnimator;

  class WindowEntry {
   public:
    WindowEntry(const AccountId& account_id, base::Optional<ws::Id> window_id);
    ~WindowEntry();

    // Returns the owner of this window. This cannot be changed.
    const AccountId& owner() const { return owner_; }

    // Returns the user for which this should be shown.
    const AccountId& show_for_user() const { return show_for_user_; }

    // Returns if the window should be shown for the "show user" or not.
    bool show() const { return show_; }

    // Set the user which will display the window on the owned desktop. If
    // an empty user id gets passed the owner will be used.
    void set_show_for_user(const AccountId& account_id) {
      show_for_user_ = account_id.is_valid() ? account_id : owner_;
    }

    // Sets if the window gets shown for the active user or not.
    void set_show(bool show) { show_ = show; }

    // True if this window was created by the window service.
    bool from_window_service() const { return from_window_service_; }

    // Unsets the |window_id|. This does not effect whether the window is
    // from the window-service, only the stored id. Resetting the id happens
    // when the client changes. This is necessary as the id is generally unique
    // to a client.
    void reset_window_id() { window_id_.reset(); }
    const base::Optional<ws::Id> window_id() const { return window_id_; }

   private:
    // The user id of the owner of this window.
    const AccountId owner_;

    // The user id of the user on which desktop the window gets shown.
    AccountId show_for_user_;

    // True if the window should be visible for the user which shows the window.
    bool show_ = true;

    // The id assigned to the window by the WindowService.
    base::Optional<ws::Id> window_id_;

    const bool from_window_service_;

    DISALLOW_COPY_AND_ASSIGN(WindowEntry);
  };

  using TransientWindowToVisibility = base::flat_map<aura::Window*, bool>;

  using WindowToEntryMap =
      std::map<aura::Window*, std::unique_ptr<WindowEntry>>;

  const AccountId& GetWindowOwner(aura::Window* window) const;

  // Returns true if at least one window's 'owner' account differs from its
  // 'shown' account. In other words, a window from one account is shown with
  // windows from another account.
  bool AreWindowsSharedAmongUsers() const;

  // Returns true if the 'shown' owner of |window| is |account_id|.
  bool IsWindowOnDesktopOfUser(aura::Window* window,
                               const AccountId& account_id) const;

  // Returns the 'shown' owner.
  const AccountId& GetUserPresentingWindow(aura::Window* window) const;

  // Show a window for a user without switching the user.
  // Returns true when the window moved to a new desktop.
  bool ShowWindowForUserIntern(aura::Window* window,
                               const AccountId& account_id);

  // Show / hide the given window. Note: By not doing this within the functions,
  // this allows to either switching to different ways to show/hide and / or to
  // distinguish state changes performed by this class vs. state changes
  // performed by the others. Note furthermore that system modal dialogs will
  // not get hidden. We will switch instead to the owners desktop.
  // The |animation_time| is the time the animation should take, an empty value
  // switches instantly.
  void SetWindowVisibility(aura::Window* window,
                           bool visible,
                           base::TimeDelta animation_time = base::TimeDelta());

  const WindowToEntryMap& window_to_entry() { return window_to_entry_; }

  // Show the window and its transient children. However - if a transient child
  // was turned invisible by some other operation, it will stay invisible.
  // |animation_time| is the amount of time to animate.
  void ShowWithTransientChildrenRecursive(aura::Window* window,
                                          base::TimeDelta animation_time);

  // Find the first owned window in the chain.
  // Returns NULL when the window itself is owned.
  aura::Window* GetOwningWindowInTransientChain(aura::Window* window) const;

  // A |window| and its children were attached as transient children to an
  // |owning_parent| and need to be registered. Note that the |owning_parent|
  // itself will not be registered, but its children will.
  void AddTransientOwnerRecursive(aura::Window* window,
                                  aura::Window* owning_parent);

  // A window and its children were removed from its parent and can be
  // unregistered.
  void RemoveTransientOwnerRecursive(aura::Window* window);

  // Animate a |window| to be |visible| over a time of |animation_time|.
  void SetWindowVisible(aura::Window* window,
                        bool visible,
                        base::TimeDelta aimation_time);

  // Returns the time for an animation.
  base::TimeDelta GetAdjustedAnimationTime(base::TimeDelta default_time) const;

  mojom::MultiUserWindowManagerClient* client_;

  MultiUserWindowManagerDelegateClassic* classic_delegate_;

  // A lookup to see to which user the given window belongs to, where and if it
  // should get shown.
  WindowToEntryMap window_to_entry_;

  // A map which remembers for owned transient windows their own visibility.
  TransientWindowToVisibility transient_window_to_visibility_;

  // The currently selected active user. It is used to find the proper
  // visibility state in various cases. The state is stored here instead of
  // being read from the user manager to be in sync while a switch occurs.
  AccountId current_account_id_;

  // Suppress changes to the visibility flag while we are changing it ourselves.
  bool suppress_visibility_changes_ = false;

  // The speed which is used to perform any animations.
  AnimationSpeed animation_speed_ = ANIMATION_SPEED_NORMAL;

  // The animation between users.
  std::unique_ptr<UserSwitchAnimator> animation_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManager);
};

}  // namespace ash

#endif  // ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_H_
