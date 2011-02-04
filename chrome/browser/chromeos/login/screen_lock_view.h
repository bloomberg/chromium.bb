// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
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
class ScreenLockView : public ThrobberHostView,
                       public views::Textfield::Controller,
                       public NotificationObserver,
                       public UserView::Delegate {
 public:
  explicit ScreenLockView(ScreenLocker* screen_locker);
  virtual ~ScreenLockView();

  void Init();

  // Clears and sets the focus to the password field.
  void ClearAndSetFocusToPassword();

  // Enable/Disable signout button.
  void SetSignoutEnabled(bool enabled);

  // Returns the bounds of the password field in ScreenLocker's coordinate.
  gfx::Rect GetPasswordBoundsRelativeTo(const views::View* view);

  // views::View implementation:
  virtual void SetEnabled(bool enabled);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // views::Textfield::Controller implementation:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents);
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& keystroke);

  // UserView::Delegate implementation:
  virtual void OnSignout();
  virtual bool IsUserSelected() const { return true; }

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

  // ScreenLocker is owned by itself.
  ScreenLocker* screen_locker_;

  NotificationRegistrar registrar_;

  // User's picture, signout button and password field.
  views::View* main_;

  // Username that overlays on top of user's picture.
  views::View* username_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
