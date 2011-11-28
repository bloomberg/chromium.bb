// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker_tester.h"

#include <gdk/gdkkeysyms.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/screen_lock_view.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screen_locker_views.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/root_view.h"
#include "views/controls/label.h"

namespace chromeos {

test::ScreenLockerTester* ScreenLocker::GetTester() {
  return new test::ScreenLockerTester();
}

namespace test {

bool ScreenLockerTester::IsLocked() {
  return ScreenLocker::screen_locker_ &&
      ScreenLocker::screen_locker_->locked_;
}

void ScreenLockerTester::InjectMockAuthenticator(
    const std::string& user, const std::string& password) {
  DCHECK(ScreenLocker::screen_locker_);
  ScreenLocker::screen_locker_->SetAuthenticator(
      new MockAuthenticator(ScreenLocker::screen_locker_, user, password));
}

void ScreenLockerTester::SetPassword(const std::string& password) {
  DCHECK(ScreenLocker::screen_locker_);
  views::Textfield* pass = GetPasswordField();
  pass->SetText(ASCIIToUTF16(password.c_str()));
}

std::string ScreenLockerTester::GetPassword() const {
  DCHECK(ScreenLocker::screen_locker_);
  views::Textfield* pass = GetPasswordField();
  return UTF16ToUTF8(pass->text());
}

void ScreenLockerTester::EnterPassword(const std::string& password) {
  SetPassword(password);
  views::Textfield* pass = GetPasswordField();
  GdkEvent* event = gdk_event_new(GDK_KEY_PRESS);
  event->key.keyval = GDK_Return;
  views::KeyEvent key_event(event);
  screen_locker_views()->screen_lock_view_->HandleKeyEvent(pass, key_event);
  gdk_event_free(event);
}

void ScreenLockerTester::EmulateWindowManagerReady() {
  DCHECK(ScreenLocker::screen_locker_);
  screen_locker_views()->OnWindowManagerReady();
}

views::Textfield* ScreenLockerTester::GetPasswordField() const {
  DCHECK(ScreenLocker::screen_locker_);
  return screen_locker_views()->screen_lock_view_->password_field_;
}

views::Widget* ScreenLockerTester::GetWidget() const {
  DCHECK(ScreenLocker::screen_locker_);
  return screen_locker_views()->lock_window_;
}

views::Widget* ScreenLockerTester::GetChildWidget() const {
  DCHECK(ScreenLocker::screen_locker_);
  return screen_locker_views()->lock_widget_;
}

ScreenLockerViews* ScreenLockerTester::screen_locker_views() const {
  DCHECK(ScreenLocker::screen_locker_);
  // TODO(flackr): Generalize testing infrastructure to work with WebUI.
  DCHECK(!ScreenLocker::UseWebUILockScreen());
  return static_cast<ScreenLockerViews*>(
      ScreenLocker::screen_locker_->delegate_.get());
}

}  // namespace test

}  // namespace chromeos
