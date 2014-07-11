// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SCREENLOCK_BRIDGE_H_
#define CHROME_BROWSER_SIGNIN_SCREENLOCK_BRIDGE_H_

#include <string>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace gfx {
class Image;
}

class Profile;

// ScreenlockBridge brings together the screenLockPrivate API and underlying
// support. On ChromeOS, it delegates calls to the ScreenLocker. On other
// platforms, it delegates calls to UserManagerUI (and friends).
class ScreenlockBridge {
 public:
  class Observer {
   public:
    // Invoked after the screen is locked.
    virtual void OnScreenDidLock() = 0;
    // Invoked after the screen lock is dismissed.
    virtual void OnScreenDidUnlock() = 0;
   protected:
    virtual ~Observer() {}
  };

  class LockHandler {
   public:
    // Supported authentication types. Keep in sync with the enum in
    // user_pod_row.js.
    enum AuthType {
      OFFLINE_PASSWORD = 0,
      ONLINE_SIGN_IN = 1,
      NUMERIC_PIN = 2,
      USER_CLICK = 3,
      EXPAND_THEN_USER_CLICK = 4,
    };

    // Displays |message| in a banner on the lock screen.
    virtual void ShowBannerMessage(const std::string& message) = 0;

    // Shows a custom icon in the user pod on the lock screen.
    virtual void ShowUserPodCustomIcon(const std::string& user_email,
                                       const gfx::Image& icon) = 0;

    // Hides the custom icon in user pod for a user.
    virtual void HideUserPodCustomIcon(const std::string& user_email) = 0;

    // (Re)enable lock screen UI.
    virtual void EnableInput() = 0;

    // Set the authentication type to be used on the lock screen.
    virtual void SetAuthType(const std::string& user_email,
                             AuthType auth_type,
                             const std::string& auth_value) = 0;

    // Returns the authentication type used for a user.
    virtual AuthType GetAuthType(const std::string& user_email) const = 0;

    // Unlock from easy unlock app for a user.
    virtual void Unlock(const std::string& user_email) = 0;

   protected:
    virtual ~LockHandler() {}
  };

  static ScreenlockBridge* Get();
  static std::string GetAuthenticatedUserEmail(Profile* profile);

  void SetLockHandler(LockHandler* lock_handler);

  bool IsLocked() const;
  void Lock(Profile* profile);
  void Unlock(Profile* profile);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  LockHandler* lock_handler() { return lock_handler_; }

 private:
  friend struct base::DefaultLazyInstanceTraits<ScreenlockBridge>;
  friend struct base::DefaultDeleter<ScreenlockBridge>;

  ScreenlockBridge();
  ~ScreenlockBridge();

  LockHandler* lock_handler_;  // Not owned
  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockBridge);
};

#endif  // CHROME_BROWSER_SIGNIN_SCREENLOCK_BRIDGE_H_
