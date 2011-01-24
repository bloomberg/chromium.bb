// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/chromeos/login/captcha_view.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "views/accelerator.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class View;
class WidgetGtk;
}  // namespace views

namespace chromeos {

class Authenticator;
class BackgroundView;
class InputEventObserver;
class LockerInputEventObserver;
class MessageBubble;
class MouseEventRelay;
class ScreenLockView;
class LoginFailure;

namespace test {
class ScreenLockerTester;
}  // namespace test

// ScreenLocker creates a background view as well as ScreenLockView to
// authenticate the user. ScreenLocker manages its life cycle and will
// delete itself when it's unlocked.
class ScreenLocker : public LoginStatusConsumer,
                     public MessageBubbleDelegate,
                     public CaptchaView::Delegate,
                     public views::AcceleratorTarget {
 public:
  // Interface that helps switching from ScreenLockView to CaptchaView.
  class ScreenLockViewContainer {
   public:
    virtual void SetScreenLockView(views::View* screen_lock_view) = 0;

   protected:
    virtual ~ScreenLockViewContainer() {}
  };

  explicit ScreenLocker(const UserManager::User& user);

  // Returns the default instance if it has been created.
  static ScreenLocker* default_screen_locker() {
    return screen_locker_;
  }

  // Initialize and show the screen locker.
  void Init();

  // LoginStatusConsumer implements:
  virtual void OnLoginFailure(const chromeos::LoginFailure& error);
  virtual void OnLoginSuccess(const std::string& username,
                              const std::string& password,
                              const GaiaAuthConsumer::ClientLoginResult& result,
                              bool pending_requests);

  // Overridden from views::InfoBubbleDelegate.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape() { return true; }
  virtual bool FadeInOnShow() { return false; }
  virtual void OnHelpLinkActivated() {}

  // CaptchaView::Delegate implementation:
  virtual void OnCaptchaEntered(const std::string& captcha);

  // Authenticates the user with given |password| and authenticator.
  void Authenticate(const string16& password);

  // Close message bubble to clear error messages.
  void ClearErrors();

  // (Re)enable input field.
  void EnableInput();

  // Exit the chrome, which will sign out the current session.
  void Signout();

  // Present user a CAPTCHA challenge with image from |captcha_url|,
  // After that shows error bubble with |message|.
  void ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                  const std::wstring& message);

  // Disables all UI needed and shows error bubble with |message|.
  // If |sign_out_only| is true then all other input except "Sign Out"
  // button is blocked.
  void ShowErrorMessage(const std::wstring& message, bool sign_out_only);

  // Called when the all inputs are grabbed.
  void OnGrabInputs();

  // Returns the user to authenticate.
  const UserManager::User& user() const {
    return user_;
  }

  // Returns a view that has given view |id|, or null if it doesn't exist.
  views::View* GetViewByID(int id);

  // Initialize ScreenLocker class. It will listen to
  // LOGIN_USER_CHANGED notification so that the screen locker accepts
  // lock event only after a user is logged in.
  static void InitClass();

  // Show the screen locker.
  static void Show();

  // Hide the screen locker.
  static void Hide();

  // Notifies that PowerManager rejected UnlockScreen request.
  static void UnlockScreenFailed();

  // Returns the tester
  static test::ScreenLockerTester* GetTester();

 private:
  friend class DeleteTask<ScreenLocker>;
  friend class test::ScreenLockerTester;
  friend class LockerInputEventObserver;

  virtual ~ScreenLocker();

  // Sets the authenticator.
  void SetAuthenticator(Authenticator* authenticator);

  // Called when the screen lock is ready.
  void ScreenLockReady();

  // Called when the window manager is ready to handle locked state.
  void OnWindowManagerReady();

  // Shows error_info_ bubble with the |message| and |arrow_location| specified.
  // Assumes that UI controls were locked before that.
  void ShowErrorBubble(const std::wstring& message,
                       BubbleBorder::ArrowLocation arrow_location);

  // Stops screen saver.
  void StopScreenSaver();

  // Starts screen saver.
  void StartScreenSaver();

  // Overridden from AcceleratorTarget:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // Event handler for client-event.
  CHROMEGTK_CALLBACK_1(ScreenLocker, void, OnClientEvent, GdkEventClient*);

  // The screen locker window.
  views::WidgetGtk* lock_window_;

  // TYPE_CHILD widget to grab the keyboard/mouse input.
  views::WidgetGtk* lock_widget_;

  // A view that accepts password.
  ScreenLockView* screen_lock_view_;

  // A view that can display html page as background.
  BackgroundView* background_view_;

  // View used to present CAPTCHA challenge input.
  CaptchaView* captcha_view_;

  // Containers that hold currently visible view.
  // Initially it's ScreenLockView instance.
  // When CAPTCHA input dialog is presented it's swapped to CaptchaView
  // instance, then back after CAPTCHA input is done.
  ScreenLockViewContainer* grab_container_;
  ScreenLockViewContainer* background_container_;

  // View that's not owned by grab_container_ - either ScreenLockView or
  // CaptchaView instance. Keep that under scoped_ptr so that it's deleted.
  scoped_ptr<views::View> secondary_view_;

  // Postponed error message to be shown after CAPTCHA input is done.
  std::wstring postponed_error_message_;

  // Logged in user.
  UserManager::User user_;

  // Used to authenticate the user to unlock.
  scoped_refptr<Authenticator> authenticator_;

  // ScreenLocker grabs all keyboard and mouse events on its
  // gdk window and never let other gdk_window to handle inputs.
  // This MouseEventRelay object is used to forward events to
  // the message bubble's gdk_window so that close button works.
  scoped_ptr<MouseEventRelay> mouse_event_relay_;

  // A message loop observer to detect user's keyboard/mouse event.
  // Used when |unlock_on_input_| is true.
  scoped_ptr<InputEventObserver> input_event_observer_;

  // A message loop observer to detect user's keyboard/mouse event.
  // Used when to show the screen locker upon such an event.
  scoped_ptr<LockerInputEventObserver> locker_input_event_observer_;

  // An info bubble to display login failure message.
  MessageBubble* error_info_;

  // True if the screen locker's window has been drawn.
  bool drawn_;

  // True if both mouse input and keyboard input are grabbed.
  bool input_grabbed_;

  // Unlock the screen when it detects key/mouse event without asking
  // password. True when chrome is in BWSI or auto login mode.
  bool unlock_on_input_;

  // True if the screen is locked, or false otherwise.  This changes
  // from false to true, but will never change from true to
  // false. Instead, ScreenLocker object gets deleted when unlocked.
  bool locked_;

  // Reference to the single instance of the screen locker object.
  // This is used to make sure there is only one screen locker instance.
  static ScreenLocker* screen_locker_;

  // The time when the screen locker object is created.
  base::Time start_time_;
  // The time when the authentication is started.
  base::Time authentication_start_time_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
