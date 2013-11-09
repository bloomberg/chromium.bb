// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_H_

#include <map>
#include <string>

class Browser;
class Profile;

namespace aura {
class Window;
}

namespace chrome {

class MultiUserWindowManagerChromeOS;

// The MultiUserWindowManager manages windows from multiple users by presenting
// only user relevant windows to the current user. The manager is automatically
// determining the window ownership from browser and application windows and
// puts them onto the correct "desktop".
// Un-owned windows will be visible on all users desktop's and owned windows can
// only be presented on one desktop. If a window should get moved to another
// user's desktop |ShowWindowForUser| can be called.
// Windows which are neither a browser nor an app can be associated with a user
// through |SetWindowOwner|.
// This class will also switch the desktop upon user change.
// Note:
// - There is no need to "unregister" a window from an owner. The class will
//   clean automatically all references of that window upon destruction.
// - User changes will be tracked via observer. No need to call.
// - All child windows will be owned by the same owner as its parent.
class MultiUserWindowManager {
 public:
  // The multi profile mode in use.
  enum MultiProfileMode {
    MULTI_PROFILE_MODE_UNINITIALIZED,  // Not initialized yet.
    MULTI_PROFILE_MODE_OFF,            // Single user mode.
    MULTI_PROFILE_MODE_SEPARATED,      // Each user has his own desktop.
    MULTI_PROFILE_MODE_MIXED           // All users mix windows freely.
  };

  // Creates an instance of the MultiUserWindowManager.
  // Note: This function might fail if due to the desired mode the
  // MultiUserWindowManager is not required.
  static MultiUserWindowManager* CreateInstance();

  // Gets the instance of the object. If the multi profile mode is not enabled
  // this will return NULL.
  static MultiUserWindowManager* GetInstance();

  // Return the current multi profile mode operation. If CreateInstance was not
  // yet called (or was already destroyed), MULTI_PROFILE_MODE_UNINITIALIZED
  // will get returned.
  static MultiProfileMode GetMultiProfileMode();

  // Removes the instance.
  static void DeleteInstance();

  // A function to set an |instance| of a created MultiUserWinwdowManager object
  // with a given |mode| for test purposes.
  static void SetInstanceForTest(MultiUserWindowManager* instance,
                                 MultiProfileMode mode);

  // Assigns an owner to a passed window. Note that this window's parent should
  // be a direct child of the root window.
  // A user switch will automatically change the visibility - and - if the
  // current user is not the owner it will immediately hidden. If the window
  // had already be registered this function will run into a DCHECK violation.
  virtual void SetWindowOwner(
      aura::Window* window, const std::string& user_id) = 0;

  // See who owns this window. The return value is the user id or an empty
  // string if not assigned yet.
  virtual const std::string& GetWindowOwner(aura::Window* window) = 0;

  // Allows to show an owned window for another users. If the window is not
  // owned, this call will return immediately. (The FileManager for example
  // might be available for every user and not belong explicitly to one).
  // Note that a window can only be shown on one desktop at a time. Note that
  // when the window gets minimized, it will automatically fall back to the
  // owner's desktop.
  virtual void ShowWindowForUser(
      aura::Window* window, const std::string& user_id) = 0;

  // Returns true when windows are shared among users.
  virtual bool AreWindowsSharedAmongUsers() = 0;

  // A query call for a given window to see if it is on the given user's
  // desktop.
  virtual bool IsWindowOnDesktopOfUser(aura::Window* window,
                                       const std::string& user_id) = 0;

  // Get the user on which the window is currently shown. If an empty string is
  // passed back the window will be presented for every user.
  virtual const std::string& GetUserPresentingWindow(aura::Window* window) = 0;

  // Adds user to monitor starting and running V1/V2 application windows.
  // Returns immediately if the user (identified by a |profile|) is already
  // known to the manager. Note: This function is not implemented as a
  // SessionStateObserver to coordinate the timing of the addition with other
  // modules.
  virtual void AddUser(Profile* profile) = 0;

 protected:
  virtual ~MultiUserWindowManager() {}

 private:
  // Caching the current multi profile mode since the detection is expensive.
  static MultiProfileMode multi_user_mode_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_H_
