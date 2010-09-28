// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/controls/button/button.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"

namespace views {
class ImageView;
}  // namespace views

namespace chromeos {

class ScreenLocker;
class UserView;

namespace test {
class ScreenLockerTester;
}  // namespace test

// ScreenLockView creates view components necessary to authenticate
// a user to unlock the screen.
class ScreenLockView : public views::View,
                       public views::ButtonListener,
                       public views::Textfield::Controller,
                       public NotificationObserver,
                       public UserView::Delegate {
 public:
  explicit ScreenLockView(ScreenLocker* screen_locker);
  virtual ~ScreenLockView() {}

  void Init();

  // Clears and sets the focus to the password field.
  void ClearAndSetFocusToPassword();

  // Enable/Disable signout button.
  void SetSignoutEnabled(bool enabled);

  // Returns the bounds of the password field in ScreenLocker's coordinate.
  gfx::Rect GetPasswordBoundsRelativeTo(const views::View* view);

  // views::View implementation:
  virtual void SetEnabled(bool enabled);

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // views::ButtonListener implmeentation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::Textfield::Controller implementation:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);

  // UserView::Delegate implementation:
  virtual void OnSignout();

 protected:
  // views::View implementation:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  friend class test::ScreenLockerTester;

  UserView* user_view_;

  // For editing the password.
  views::Textfield* password_field_;
  views::Button* unlock_button_;

  // ScreenLocker is owned by itself.
  ScreenLocker* screen_locker_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
