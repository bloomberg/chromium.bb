// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/user_view.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace chromeos {

class ScreenLocker;
class UserView;

namespace test {
class ScreenLockerTester;
}  // namespace test

// ScreenLockView creates view components necessary to authenticate
// a user to unlock the screen.
class ScreenLockView : public ThrobberHostView,
                       public views::TextfieldController,
                       public content::NotificationObserver,
                       public UserView::Delegate {
 public:
  explicit ScreenLockView(ScreenLocker* screen_locker);
  virtual ~ScreenLockView();

  void Init();

  // Clears and sets the focus to the password field.
  void ClearAndSetFocusToPassword();

  // Enable/Disable signout button.
  void SetSignoutEnabled(bool enabled);

  // views::View:
  virtual void SetEnabled(bool enabled);
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& keystroke) OVERRIDE;

  // UserView::Delegate:
  virtual void OnSignout() OVERRIDE;
  virtual bool IsUserSelected() const OVERRIDE;

  views::Textfield* password_field() { return password_field_; }

 private:
  friend class test::ScreenLockerTester;

  UserView* user_view_;

  // For editing the password.
  views::Textfield* password_field_;

  // ScreenLocker is owned by itself.
  ScreenLocker* screen_locker_;

  content::NotificationRegistrar registrar_;

  // User's picture, signout button and password field.
  views::View* main_;

  // Username that overlays on top of user's picture.
  views::View* username_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREEN_LOCK_VIEW_H_
