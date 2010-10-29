// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
#pragma once

#include <string>

#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "views/accelerator.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
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
                     public views::AcceleratorTarget {
 public:
  explicit ScreenLocker(const UserManager::User& user);

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

  // Authenticates the user with given |password| and authenticator.
  void Authenticate(const string16& password);

  // Close message bubble to clear error messages.
  void ClearErrors();

  // (Re)enable input field.
  void EnableInput();

  // Exit the chrome, which will sign out the current session.
  void Signout();

  // Called when the all inputs are grabbed.
  void OnGrabInputs();

  // Returns the user to authenticate.
  const UserManager::User& user() const {
    return user_;
  }

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
  // The time when the authenticaton is started.
  base::Time authentication_start_time_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_H_
