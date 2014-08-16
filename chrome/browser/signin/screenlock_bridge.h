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

  // Class containing parameters describing the custom icon that should be
  // shown on a user's screen lock pod next to the input field.
  class UserPodCustomIconOptions {
   public:
    UserPodCustomIconOptions();
    ~UserPodCustomIconOptions();

    // Converts parameters to a dictionary values that can be sent to the
    // screenlock web UI.
    scoped_ptr<base::DictionaryValue> ToDictionaryValue() const;

    // Sets the icon as chrome://theme resource URL.
    void SetIconAsResourceURL(const std::string& url);

    // Sets the icon as a gfx::Image. The image will be converted to set of data
    // URLs for each icon representation. Use |SetIconAsResourceURL| instead of
    // this.
    // TODO(tbarzic): Remove this one once easy unlock app stops using
    // screenlockPrivate.showCustomIcon.
    void SetIconAsImage(const gfx::Image& image);

    // Sets the icon size. Has to be called if |SetIconAsResourceURL| was used
    // to set the icon. For animated icon, this should be set to a single frame
    // size, not the animation resource size.
    void SetSize(size_t icon_width, size_t icon_height);

    // If the icon is supposed to be animated, sets the animation parameters.
    // If set, it expects that the resource set using |SetIcon*| methods
    // contains horizontally arranged ordered list of animation frames.
    // Note that the icon size set in |SetSize| should be a single frame size.
    // |resource_width|: Total animation resource width.
    // |frame_length_ms|: Time for which a single animation frame is shown.
    void SetAnimation(size_t resource_width, size_t frame_length_ms);

    // Sets the icon opacity. The values should be in <0, 100] interval, which
    // will get scaled into <0, 1] interval. The default value is 100.
    void SetOpacity(size_t opacity);

    // Sets the icon tooltip. If |autoshow| is set the tooltip is automatically
    // shown with the icon.
    void SetTooltip(const base::string16& tooltip, bool autoshow);

    // If hardlock on click is set, clicking the icon in the screenlock will
    // go to state where password is required for unlock.
    void SetHardlockOnClick();

   private:
    std::string icon_resource_url_;
    scoped_ptr<gfx::Image> icon_image_;

    size_t width_;
    size_t height_;

    bool animation_set_;
    size_t animation_resource_width_;
    size_t animation_frame_length_ms_;

    // The opacity should be in <0, 100] range.
    size_t opacity_;

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
