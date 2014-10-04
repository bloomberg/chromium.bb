// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SCREENLOCK_BRIDGE_H_
#define CHROME_BROWSER_SIGNIN_SCREENLOCK_BRIDGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/values.h"


class Profile;

// ScreenlockBridge brings together the screenLockPrivate API and underlying
// support. On ChromeOS, it delegates calls to the ScreenLocker. On other
// platforms, it delegates calls to UserManagerUI (and friends).
// TODO(tbarzic): Rename ScreenlockBridge to SignInScreenBridge, as this is not
// used solely for the lock screen anymore.
class ScreenlockBridge {
 public:
  class Observer {
   public:
    // Invoked after the screen is locked.
    virtual void OnScreenDidLock() = 0;
    // Invoked after the screen lock is dismissed.
    virtual void OnScreenDidUnlock() = 0;
    // Invoked when the user focused on the lock screen changes.
    virtual void OnFocusedUserChanged(const std::string& user_id) = 0;
   protected:
    virtual ~Observer() {}
  };

  // User pod icons supported by lock screen / signin screen UI.
  enum UserPodCustomIcon {
    USER_POD_CUSTOM_ICON_NONE,
    USER_POD_CUSTOM_ICON_HARDLOCKED,
    USER_POD_CUSTOM_ICON_LOCKED,
    USER_POD_CUSTOM_ICON_UNLOCKED,
    USER_POD_CUSTOM_ICON_SPINNER
  };

  // Class containing parameters describing the custom icon that should be
  // shown on a user's screen lock pod next to the input field.
  class UserPodCustomIconOptions {
   public:
    UserPodCustomIconOptions();
    ~UserPodCustomIconOptions();

    // Converts parameters to a dictionary values that can be sent to the
    // screenlock web UI.
    scoped_ptr<base::DictionaryValue> ToDictionaryValue() const;

    // Sets the icon that should be shown in the UI.
    void SetIcon(UserPodCustomIcon icon);

    // Sets the icon tooltip. If |autoshow| is set the tooltip is automatically
    // shown with the icon.
    void SetTooltip(const base::string16& tooltip, bool autoshow);

    // If hardlock on click is set, clicking the icon in the screenlock will
    // go to state where password is required for unlock.
    void SetHardlockOnClick();

   private:
    UserPodCustomIcon icon_;

    base::string16 tooltip_;
    bool autoshow_tooltip_;

    bool hardlock_on_click_;

    DISALLOW_COPY_AND_ASSIGN(UserPodCustomIconOptions);
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
      FORCE_OFFLINE_PASSWORD = 5
    };

    // Displays |message| in a banner on the lock screen.
    virtual void ShowBannerMessage(const base::string16& message) = 0;

    // Shows a custom icon in the user pod on the lock screen.
    virtual void ShowUserPodCustomIcon(
        const std::string& user_email,
        const UserPodCustomIconOptions& icon) = 0;

    // Hides the custom icon in user pod for a user.
    virtual void HideUserPodCustomIcon(const std::string& user_email) = 0;

    // (Re)enable lock screen UI.
    virtual void EnableInput() = 0;

    // Set the authentication type to be used on the lock screen.
    virtual void SetAuthType(const std::string& user_email,
                             AuthType auth_type,
                             const base::string16& auth_value) = 0;

    // Returns the authentication type used for a user.
    virtual AuthType GetAuthType(const std::string& user_email) const = 0;

    // Unlock from easy unlock app for a user.
    virtual void Unlock(const std::string& user_email) = 0;

    // Attempts to login the user using an easy unlock key.
    virtual void AttemptEasySignin(const std::string& user_email,
                                   const std::string& secret,
                                   const std::string& key_label) = 0;

   protected:
    virtual ~LockHandler() {}
  };

  static ScreenlockBridge* Get();
  static std::string GetAuthenticatedUserEmail(Profile* profile);

  void SetLockHandler(LockHandler* lock_handler);
  void SetFocusedUser(const std::string& user_id);

  bool IsLocked() const;
  void Lock(Profile* profile);
  void Unlock(Profile* profile);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  LockHandler* lock_handler() { return lock_handler_; }

  std::string focused_user_id() const { return focused_user_id_; }

 private:
  friend struct base::DefaultLazyInstanceTraits<ScreenlockBridge>;
  friend struct base::DefaultDeleter<ScreenlockBridge>;

  ScreenlockBridge();
  ~ScreenlockBridge();

  LockHandler* lock_handler_;  // Not owned
  // The last focused user's id.
  std::string focused_user_id_;
  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockBridge);
};

#endif  // CHROME_BROWSER_SIGNIN_SCREENLOCK_BRIDGE_H_
