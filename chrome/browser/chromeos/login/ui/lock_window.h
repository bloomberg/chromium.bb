// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_

#include "base/macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class View;
class Widget;
}

namespace chromeos {

// Shows the widget for the WebUI screen locker.
class LockWindow : public views::Widget, public views::WidgetDelegate {
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
  ~LockWindow() override;

  // Attempt to grab inputs on the webview, the actual view displaying the lock
  // screen WebView.
  void Grab();

  // Sets the observer class which is notified on lock window events.
  void set_observer(Observer* observer) { observer_ = observer; }

  // Sets the view which should be initially focused.
  void set_initially_focused_view(views::View* view) {
    initially_focused_view_ = view;
  }

  // views::WidgetDelegate:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

 private:
  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;

  // The observer's OnLockWindowReady method will be called when the lock
  // window has finished all initialization.
  Observer* observer_ = nullptr;

  // The view which should be initially focused.
  views::View* initially_focused_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LockWindow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOCK_WINDOW_H_
