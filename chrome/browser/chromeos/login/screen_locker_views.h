// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_VIEWS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_VIEWS_H_
#pragma once

#include "chrome/browser/chromeos/login/captcha_view.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/screen_locker_delegate.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/base/gtk/gtk_signal.h"
#endif

namespace chromeos {

class BackgroundView;
class InputEventObserver;
class LockerInputEventObserver;
class MessageBubble;
class MouseEventRelay;
class ScreenLocker;
class ScreenLockView;

namespace test {
class ScreenLockerTester;
}

// This version of ScreenLockerDelegate displays a views interface. This class
// shows a BackgroundView and a Signout button as well as creating a
// ScreenLockView to allow the user to log in.
class ScreenLockerViews : public ScreenLockerDelegate,
                          public CaptchaView::Delegate,
                          public ui::AcceleratorTarget,
                          public views::Widget::Observer {
 public:
  // Interface that helps switching from ScreenLockView to CaptchaView.
  class ScreenLockViewContainer {
   public:
    virtual void SetScreenLockView(views::View* screen_lock_view) = 0;

   protected:
    virtual ~ScreenLockViewContainer();
  };

  explicit ScreenLockerViews(ScreenLocker* screen_locker);

  // Called when the all inputs are grabbed.
  void OnGrabInputs();

  // Starts screen saver.
  virtual void StartScreenSaver();

  // Stops screen saver.
  virtual void StopScreenSaver();

  // Returns a view that has given view |id|, or null if it doesn't exist.
  views::View* GetViewByID(int id);

  // ScreenLockerDelegate implementation:
  virtual void LockScreen(bool unlock_on_input) OVERRIDE;
  virtual void ScreenLockReady() OVERRIDE;
  virtual void OnAuthenticate() OVERRIDE;
  virtual void SetInputEnabled(bool enabled) OVERRIDE;
  virtual void SetSignoutEnabled(bool enabled) OVERRIDE;
  virtual void ShowErrorMessage(const string16& message,
                                bool sign_out_only) OVERRIDE;
  virtual void ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                          const string16& message) OVERRIDE;
  virtual void ClearErrors() OVERRIDE;

  // views::Widget::Observer implementation:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;

  // CaptchaView::Delegate implementation:
  virtual void OnCaptchaEntered(const std::string& captcha) OVERRIDE;

 private:
  friend class LockerInputEventObserver;
  friend class test::ScreenLockerTester;

  virtual ~ScreenLockerViews();

  // Called when the window manager is ready to handle locked state.
  void OnWindowManagerReady();

  // Shows error_info_ bubble with the |message| and |arrow_location| specified.
  // Assumes that UI controls were locked before that.
  void ShowErrorBubble(const string16& message,
                       views::BubbleBorder::ArrowLocation arrow_location);

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Event handler for client-event.
  CHROMEGTK_CALLBACK_1(ScreenLockerViews, void, OnClientEvent, GdkEventClient*);

  // The screen locker window.
  views::Widget* lock_window_;

  // Child widget to grab the keyboard/mouse input.
  views::Widget* lock_widget_;

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
  string16 postponed_error_message_;

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

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerViews);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCKER_VIEWS_H_
