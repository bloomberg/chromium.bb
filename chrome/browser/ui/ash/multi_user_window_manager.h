// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_WINDOW_MANAGER_H_

#include <map>
#include <string>

#include "ash/session_state_observer.h"
#include "ash/wm/window_state_observer.h"
#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/aura/window_observer.h"

class Browser;
class Profile;

namespace aura {
class Window;
class WindowObserver;
}

namespace ash {
namespace test {
class MultiUserWindowManagerTest;
}
}

namespace chrome {

class AppObserver;

// The MultiUserWindowManager allows to show only windows of a particular user
// when the user is active. To achieve this all windows need to be registered
// to a user and upon user switch windows which belong to that (or no) user
// get shown while the others get hidden. In addition, each window visibility
// change gets inspected and - if incorrectly called - stays hidden.
// Browser as well as shell (app) window creations get automatically tracked
// and associated with the user in question.
// Other windows can explicitly made into a user specific window by calling
// |SetWindowOwner|. If a window should get shown on another user's desktop,
// the function |ShowWindowForUser| can be invoked.
// Note:
// - There is no need to "unregister" a window from an owner. The class will
//   clean automatically all references of that window upon destruction.
// - User changes will be tracked via observer. No need to call.
// - All child windows will be owned by the same owner as its parent.
class MultiUserWindowManager : public ash::SessionStateObserver,
                               public aura::WindowObserver,
                               public content::NotificationObserver,
                               public ash::wm::WindowStateObserver {
 public:
  // Gets the instance of the object. If the multi profile mode is not enabled
  // this will return NULL. If it wasn't created yet, it will create an instance
  // using the currently active user as the active user - otherwise it will
  // return the previously created instance.
  static MultiUserWindowManager* GetInstance();

  // Removes the instance.
  static void DeleteInstance();

  // Assigns an owner to a passed window. Note that this window's parent should
  // be a direct child of the root window.
  // A user switch will automatically change the visibility - and - if the
  // current user is not the owner it will immediately hidden. If the window
  // had already be registered this function will run into a DCHECK violation.
  void SetWindowOwner(aura::Window* window, const std::string& user_id);

  // See who owns this window. The return value is the user id or an empty
  // string if not assigned yet.
  const std::string& GetWindowOwner(aura::Window* window);

  // Allows to show an owned window for another users. If the window is not
  // owned, this call will return immediately. (The FileManager for example
  // might be available for every user and not belong explicitly to one).
  // Note that a window can only be shown on one desktop at a time. Note that
  // when the window gets minimized, it will automatically fall back to the
  // owner's desktop.
  void ShowWindowForUser(aura::Window* window, const std::string& user_id);

  // Returns true when windows are shared among users.
  bool AreWindowsSharedAmongUsers();

  // A query call for a given window to see if it is on the given user's
  // desktop.
  bool IsWindowOnDesktopOfUser(aura::Window* window,
                               const std::string& user_id);

  // Get the user on which the window is currently shown. If an empty string is
  // passed back the window will be presented for every user.
  const std::string& GetUserPresentingWindow(aura::Window* window);

  // SessionStateObserver overrides:
  virtual void ActiveUserChanged(const std::string& user_id) OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;
  virtual void OnWindowVisibilityChanging(aura::Window* window,
                                          bool visible) OVERRIDE;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;

  // Window .. overrides:
  virtual void OnWindowShowTypeChanged(
      ash::wm::WindowState* state,
      ash::wm::WindowShowType old_type) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) OVERRIDE;

 protected:
  friend class ash::test::MultiUserWindowManagerTest;

  // Create an instance for and pass the |active_user_id| to it.
  static MultiUserWindowManager* CreateInstanceInternal(
      std::string active_user_id);

 private:
  class WindowEntry {
   public:
    explicit WindowEntry(const std::string& user_id)
        : owner_(user_id),
          show_for_user_(user_id),
          show_(true) {}
    virtual ~WindowEntry() {}

    // Returns the owner of this window. This cannot be changed.
    const std::string& owner() const { return owner_; }

    // Returns the user for which this should be shown.
    const std::string& show_for_user() const { return show_for_user_; }

    // Returns if the window should be shown for the "show user" or not.
    bool show() const { return show_; }

    // Set the user which will display the window on the owned desktop. If
    // an empty user id gets passed the owner will be used.
    void set_show_for_user(const std::string& user_id) {
      show_for_user_ = user_id.empty() ? owner_ : user_id;
    }

    // Sets if the window gets shown for the active user or not.
    void set_show(bool show) { show_ = show; }

   private:
    // The user id of the owner of this window.
    const std::string owner_;

    // The user id of the user on which desktop the window gets shown.
    std::string show_for_user_;

    // True if the window should be visible for the user which shows the window.
    bool show_;

    DISALLOW_COPY_AND_ASSIGN(WindowEntry);
  };

  typedef std::map<aura::Window*, WindowEntry*> WindowToEntryMap;
  typedef std::map<std::string, AppObserver*> UserIDToShellWindowObserver;

  // Create the manager and use |active_user_id| as the active user.
  explicit MultiUserWindowManager(const std::string& active_user_id);
  virtual ~MultiUserWindowManager();

  // Adds user to monitor for application windows. Returns immediately if the
  // user is already known to the system. Otherwise it will make sure that
  // application windows from that user will get properly tracked.
  void AddUser(const std::string& user_id);

  // Add a browser window to the system so that the owner can be remembered.
  void AddBrowserWindow(Browser* browser);

  // Show / hide the given window. Note: By not doing this within the functions,
  // this allows to either switching to different ways to show/hide and / or to
  // distinguish state changes performed by this class vs. state changes
  // performed by the others.
  void SetWindowVisibility(aura::Window* window, bool visible);

  // Get a profile for a given user id.
  Profile* GetProfileFromUserID(const std::string& user_id);

  // A lookup to see to which user the given window belongs to, where and if it
  // should get shown.
  WindowToEntryMap window_to_entry_;

  // A list of all known users and their shell window observers.
  UserIDToShellWindowObserver user_id_to_app_observer_;

  // The currently selected active user. It is used to find the proper
  // visibility state in various cases. The state is stored here instead of
  // being read from the user manager to be in sync while a switch occurs.
  std::string current_user_id_;

  // The notification registrar to track the creation of browser windows.
  content::NotificationRegistrar registrar_;

  // Suppress changes to the visibility flag while we are changing it ourselves.
  bool suppress_visibility_changes_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManager);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_WINDOW_MANAGER_H_
