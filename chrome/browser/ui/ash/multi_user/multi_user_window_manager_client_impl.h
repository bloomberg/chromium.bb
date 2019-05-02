// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_IMPL_H_

#include <map>
#include <memory>

#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_client.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class AppObserver;
class Browser;

namespace ash {
class MultiUserWindowManager;
class MultiUserWindowManagerClientImplTest;
}  // namespace ash

namespace content {
class BrowserContext;
}

namespace aura {
class Window;
}  // namespace aura

// Real implementation of MultiUserWindowManagerClient, see it for details.
class MultiUserWindowManagerClientImpl
    : public MultiUserWindowManagerClient,
      public ash::MultiUserWindowManagerDelegate,
      public content::NotificationObserver {
 public:
  // Create the manager and use |active_account_id| as the active user.
  explicit MultiUserWindowManagerClientImpl(const AccountId& active_account_id);
  ~MultiUserWindowManagerClientImpl() override;

  // Initializes the manager after its creation. Should only be called once.
  void Init();

  // MultiUserWindowManagerClient overrides:
  void SetWindowOwner(aura::Window* window,
                      const AccountId& account_id) override;
  const AccountId& GetWindowOwner(const aura::Window* window) const override;
  void ShowWindowForUser(aura::Window* window,
                         const AccountId& account_id) override;
  bool AreWindowsSharedAmongUsers() const override;
  void GetOwnersOfVisibleWindows(
      std::set<AccountId>* account_ids) const override;
  bool IsWindowOnDesktopOfUser(aura::Window* window,
                               const AccountId& account_id) const override;
  const AccountId& GetUserPresentingWindow(
      const aura::Window* window) const override;
  void AddUser(content::BrowserContext* context) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns the current user for unit tests.
  const AccountId& GetCurrentUserForTest() const;

 private:
  friend class ash::MultiUserWindowManagerClientImplTest;
  friend class MultiUserWindowManagerClientImplTestHelper;

  // ash::MultiUserWindowManagerDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override;
  void OnTransitionUserShelfToNewAccount() override;
  void OnDidSwitchActiveAccount() override;

  // Add a browser window to the system so that the owner can be remembered.
  void AddBrowserWindow(Browser* browser);

  // The single instance of MultiUserWindowManagerClientImpl, tracked solely for
  // tests.
  static MultiUserWindowManagerClientImpl* instance_;

  using AccountIdToAppWindowObserver = std::map<AccountId, AppObserver*>;

  // A list of all known users and their app window observers.
  AccountIdToAppWindowObserver account_id_to_app_observer_;

  // An observer list to be notified upon window owner changes.
  base::ObserverList<Observer>::Unchecked observers_;

  // The notification registrar to track the creation of browser windows.
  content::NotificationRegistrar registrar_;

  std::unique_ptr<ash::MultiUserWindowManager> ash_multi_user_window_manager_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManagerClientImpl);
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_IMPL_H_
