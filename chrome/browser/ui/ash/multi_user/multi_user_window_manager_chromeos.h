// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CHROMEOS_H_

#include <map>
#include <memory>

#include "ash/multi_user/multi_user_window_manager_delegate_classic.h"
#include "ash/public/interfaces/multi_user_window_manager.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/ws/common/types.h"
#include "ui/aura/window_observer.h"

class AppObserver;
class Browser;

namespace ash {
class MultiUserWindowManager;
class MultiUserWindowManagerChromeOSTest;
}  // namespace ash

namespace content {
class BrowserContext;
}

namespace aura {
class Window;
}  // namespace aura

// This class is responsible for notifying Ash's MultiUserWindowManager of
// which aura::Windows are associated with which accounts. This class caches
// similar information to that of Ash's MultiUserWindowManager so that it can
// synchronously respond to requests for state (such as
// IsWindowOnDesktopOfUser()).
//
// TODO: don't use ash::MultiUserWindowManager directly, instead use it over
// a mojom. https://crbug.com/875111.
class MultiUserWindowManagerChromeOS
    : public MultiUserWindowManager,
      public ash::MultiUserWindowManagerDelegateClassic,
      public ash::mojom::MultiUserWindowManagerClient,
      public aura::WindowObserver,
      public content::NotificationObserver {
 public:
  // Create the manager and use |active_account_id| as the active user.
  explicit MultiUserWindowManagerChromeOS(const AccountId& active_account_id);
  ~MultiUserWindowManagerChromeOS() override;

  // Initializes the manager after its creation. Should only be called once.
  void Init();

  // MultiUserWindowManager overrides:
  void SetWindowOwner(aura::Window* window,
                      const AccountId& account_id) override;
  const AccountId& GetWindowOwner(aura::Window* window) const override;
  void ShowWindowForUser(aura::Window* window,
                         const AccountId& account_id) override;
  bool AreWindowsSharedAmongUsers() const override;
  void GetOwnersOfVisibleWindows(
      std::set<AccountId>* account_ids) const override;
  bool IsWindowOnDesktopOfUser(aura::Window* window,
                               const AccountId& account_id) const override;
  const AccountId& GetUserPresentingWindow(aura::Window* window) const override;
  void AddUser(content::BrowserContext* context) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // WindowObserver overrides:
  void OnWindowDestroyed(aura::Window* window) override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ash::MultiUserWindowManagerDelegateClassic overrides:
  void OnOwnerEntryChanged(aura::Window* window,
                           const AccountId& account_id,
                           bool was_minimized,
                           bool teleported) override;

  // Returns the current user for unit tests.
  const AccountId& GetCurrentUserForTest() const;

 private:
  friend class ash::MultiUserWindowManagerChromeOSTest;

  struct ClassicSupport;

  class WindowEntry {
   public:
    explicit WindowEntry(const AccountId& account_id)
        : owner_(account_id), show_for_user_(account_id) {}
    ~WindowEntry() {}

    // Returns the owner of this window. This cannot be changed.
    const AccountId& owner() const { return owner_; }

    // Returns the user for which this should be shown.
    const AccountId& show_for_user() const { return show_for_user_; }

    // Set the user which will display the window on the owned desktop. If
    // an empty user id gets passed the owner will be used.
    void set_show_for_user(const AccountId& account_id) {
      show_for_user_ = account_id.is_valid() ? account_id : owner_;
    }

   private:
    // The user id of the owner of this window.
    const AccountId owner_;

    // The user id of the user on which desktop the window gets shown.
    AccountId show_for_user_;

    DISALLOW_COPY_AND_ASSIGN(WindowEntry);
  };

  // ash::mojom::MultiUserWindowManagerClient:
  void OnWindowOwnerEntryChanged(ws::Id window_id,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override;
  void OnWillSwitchActiveAccount(const AccountId& account_id) override;
  void OnTransitionUserShelfToNewAccount() override;
  void OnDidSwitchActiveAccount() override;

  void FlushForTesting();

  using AccountIdToAppWindowObserver = std::map<AccountId, AppObserver*>;

  using WindowToEntryMap =
      std::map<aura::Window*, std::unique_ptr<WindowEntry>>;

  // Add a browser window to the system so that the owner can be remembered.
  void AddBrowserWindow(Browser* browser);

  std::unique_ptr<ClassicSupport> classic_support_;

  // A lookup to see to which user the given window belongs to, where and if it
  // should get shown.
  WindowToEntryMap window_to_entry_;

  // A list of all known users and their app window observers.
  AccountIdToAppWindowObserver account_id_to_app_observer_;

  // An observer list to be notified upon window owner changes.
  base::ObserverList<Observer>::Unchecked observers_;

  // The currently selected active user. It is used to find the proper
  // visibility state in various cases. The state is stored here instead of
  // being read from the user manager to be in sync while a switch occurs.
  AccountId current_account_id_;

  // The notification registrar to track the creation of browser windows.
  content::NotificationRegistrar registrar_;

  // This is *only* created in classic and single-process mash cases. In
  // multi-process mash Ash creates the MultiUserWindowManager.
  std::unique_ptr<ash::MultiUserWindowManager> ash_multi_user_window_manager_;

  // Only used for windows created for the window-service. For example,
  // Browser windows when running in mash.
  ash::mojom::MultiUserWindowManagerAssociatedPtr
      multi_user_window_manager_mojom_;

  mojo::AssociatedBinding<ash::mojom::MultiUserWindowManagerClient>
      client_binding_{this};

  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManagerChromeOS);
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CHROMEOS_H_
