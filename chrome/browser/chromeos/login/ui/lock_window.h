// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_

#include "base/basictypes.h"

namespace views {
class View;
class Widget;
}

namespace chromeos {

// This is the interface which lock windows used for the WebUI screen locker
// implement.
class LockWindow {
 public:
  // This class provides an interface for the lock window to notify an observer
  // about its status.
  class Observer {
   public:
    // This method will be called when the lock window has finished all
    // initialization.
    virtual void OnLockWindowReady() = 0;
  };

  LockWindow();

  // Attempt to grab inputs on the webview, the actual view displaying the lock
  // screen WebView.
  virtual void Grab() = 0;

  // Returns the actual widget for the lock window.
  virtual views::Widget* GetWidget() = 0;

  // Sets the observer class which is notified on lock window events.
  void set_observer(Observer* observer) {
    observer_ = observer;
  }

  // Sets the view which should be initially focused.
  void set_initially_focused_view(views::View* view) {
    initially_focused_view_ = view;
  }

  // Creates an instance of the platform specific lock window.
  static LockWindow* Create();

 protected:
  // The observer's OnLockWindowReady method will be called when the lock
  // window has finished all initialization.
  Observer* observer_;

  // The view which should be initially focused.
  views::View* initially_focused_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LockWindow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_
