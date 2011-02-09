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
#include "views/controls/button/button.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/events/event.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

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
  views::KeyEvent key_event(&event->key);
  ScreenLocker::screen_locker_->screen_lock_view_->HandleKeyEvent(
      pass, key_event);
  gdk_event_free(event);
}

void ScreenLockerTester::EmulateWindowManagerReady() {
  DCHECK(ScreenLocker::screen_locker_);
  ScreenLocker::screen_locker_->OnWindowManagerReady();
}

views::Textfield* ScreenLockerTester::GetPasswordField() const {
  DCHECK(ScreenLocker::screen_locker_);
  return ScreenLocker::screen_locker_->screen_lock_view_->password_field_;
}

views::Widget* ScreenLockerTester::GetWidget() const {
  DCHECK(ScreenLocker::screen_locker_);
  return ScreenLocker::screen_locker_->lock_window_;
}

views::Widget* ScreenLockerTester::GetChildWidget() const {
  DCHECK(ScreenLocker::screen_locker_);
  return ScreenLocker::screen_locker_->lock_widget_;
}

}  // namespace test

}  // namespace chromeos
