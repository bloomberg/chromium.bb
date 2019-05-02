// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_H_

#include <map>
#include <set>
#include <string>

class AccountId;

namespace content {
class BrowserContext;
}

namespace aura {
class Window;
}

// MultiUserWindowManagerClient acts as a client of ash's
// MultiUserWindowManager. In addition to acting as a client,
// MultiUserWindowManagerClient forwards calls to ash::MultiUserWindowManager
// and maintains a cache so that it can synchronously provide information about
// Windows.
//
// See ash::MultiUserWindowManager for more details on the API provided here.
//
// TODO(sky): this name is confusing. Maybe it should be
// ChromeMultiUserWindowManager.
class MultiUserWindowManagerClient {
 public:
  // Observer to notify of any window owner changes.
  class Observer {
   public:
    // Invoked when the new window is created and the manager start to track its
    // owner.
    virtual void OnOwnerEntryAdded(aura::Window* window) {}
    // Invoked when the owner of the window tracked by the manager is changed.
    virtual void OnOwnerEntryChanged(aura::Window* window) {}
    // Invoked when the user switch animation is finished.
    virtual void OnUserSwitchAnimationFinished() {}

   protected:
    virtual ~Observer() {}
  };

  // Creates an instance of the MultiUserWindowManagerClient.
  static MultiUserWindowManagerClient* CreateInstance();

  // Gets the instance of the object.
  static MultiUserWindowManagerClient* GetInstance();

  // Whether or not the window's title should show the avatar. On chromeos,
  // this is true when the owner of the window is different from the owner of
  // the desktop.
  static bool ShouldShowAvatar(aura::Window* window);

  // Removes the instance.
  static void DeleteInstance();

  // Sets the singleton instance to |instance| and enables multi-user-mode.
  static void SetInstanceForTest(MultiUserWindowManagerClient* instance);

  // Assigns an owner to a passed window. Note that this window's parent should
  // be a direct child of the root window.
  // A user switch will automatically change the visibility - and - if the
  // current user is not the owner it will immediately hidden. If the window
  // had already be registered this function will run into a DCHECK violation.
  virtual void SetWindowOwner(aura::Window* window,
                              const AccountId& account_id) = 0;

  // See who owns this window. The return value is the user account id or an
  // empty AccountId if not assigned yet.
  virtual const AccountId& GetWindowOwner(const aura::Window* window) const = 0;

  // Allows to show an owned window for another users. If the window is not
  // owned, this call will return immediately. (The FileManager for example
  // might be available for every user and not belong explicitly to one).
  // Note that a window can only be shown on one desktop at a time. Note that
  // when the window gets minimized, it will automatically fall back to the
  // owner's desktop.
  virtual void ShowWindowForUser(aura::Window* window,
                                 const AccountId& account_id) = 0;

  // Returns true when windows are shared among users.
  virtual bool AreWindowsSharedAmongUsers() const = 0;

  // Get the owners for the visible windows and set them to |account_ids|.
  virtual void GetOwnersOfVisibleWindows(
      std::set<AccountId>* account_ids) const = 0;

  // A query call for a given window to see if it is on the given user's
  // desktop.
  virtual bool IsWindowOnDesktopOfUser(aura::Window* window,
                                       const AccountId& account_id) const = 0;

  // Get the user on which the window is currently shown. If an empty string is
  // passed back the window will be presented for every user.
  virtual const AccountId& GetUserPresentingWindow(
      const aura::Window* window) const = 0;

  // Adds user to monitor starting and running V1/V2 application windows.
  // Returns immediately if the user (identified by a |profile|) is already
  // known to the manager. Note: This function is not implemented as a
  // SessionStateObserver to coordinate the timing of the addition with other
  // modules.
  virtual void AddUser(content::BrowserContext* profile) = 0;

  // Manages observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  virtual ~MultiUserWindowManagerClient() {}
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_H_
