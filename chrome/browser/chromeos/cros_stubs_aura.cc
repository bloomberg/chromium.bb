// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"
#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/notifications/balloon_collection.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// SystemNotification

void SystemNotification::Init(int icon_resource_id) {
  NOTIMPLEMENTED();
}

SystemNotification::SystemNotification(Profile* profile,
                                       NotificationDelegate* delegate,
                                       int icon_resource_id,
                                       const string16& title)
    : profile_(profile),
      collection_(NULL),
      delegate_(delegate),
      title_(title),
      visible_(false),
      urgent_(false) {
  NOTIMPLEMENTED();
}

SystemNotification::SystemNotification(Profile* profile,
                                       const std::string& id,
                                       int icon_resource_id,
                                       const string16& title)
    : profile_(profile),
      collection_(NULL),
      delegate_(new Delegate(id)),
      title_(title),
      visible_(false),
      urgent_(false) {
  NOTIMPLEMENTED();
}

SystemNotification::~SystemNotification() {
}

void SystemNotification::Show(const string16& message,
                              bool urgent,
                              bool sticky) {
  NOTIMPLEMENTED() << " " << message;
}

void SystemNotification::Show(const string16& message,
                              const string16& link,
                              const MessageCallback& callback,
                              bool urgent,
                              bool sticky) {
  NOTIMPLEMENTED();
}

void SystemNotification::Hide() {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// SystemNotification::Delegate

SystemNotification::Delegate::Delegate(const std::string& id)
    : id_(id) {
  NOTIMPLEMENTED();
}

std::string SystemNotification::Delegate::id() const {
  NOTIMPLEMENTED();
  return id_;
}

//////////////////////////////////////////////////////////////////////////////
// ScreenLocker

ScreenLocker::ScreenLocker(const chromeos::User& user) : user_(user) {
  NOTIMPLEMENTED();
}

ScreenLocker::~ScreenLocker() {
  NOTIMPLEMENTED();
}

void ScreenLocker::Init() {
  NOTIMPLEMENTED();
}

void ScreenLocker::OnLoginFailure(const LoginFailure& error) {
  NOTIMPLEMENTED();
}

void ScreenLocker::OnLoginSuccess(
    const std::string&,
    const std::string&,
    const GaiaAuthConsumer::ClientLoginResult&,
    bool,
    bool) {
  NOTIMPLEMENTED();
}

void ScreenLocker::Authenticate(const string16& password) {
  NOTIMPLEMENTED();
}

void ScreenLocker::ClearErrors() {
  NOTIMPLEMENTED();
}

void ScreenLocker::EnableInput() {
  NOTIMPLEMENTED();
}

void ScreenLocker::Signout() {
  NOTIMPLEMENTED();
}

void ScreenLocker::ShowCaptchaAndErrorMessage(const GURL& captcha_url,
                                              const string16& message) {
  NOTIMPLEMENTED();
}

void ScreenLocker::ShowErrorMessage(const string16& message,
                                    bool sign_out_only) {
  NOTIMPLEMENTED();
}

void ScreenLocker::SetLoginStatusConsumer(
    chromeos::LoginStatusConsumer* consumer) {
  NOTIMPLEMENTED();
}

// static
void ScreenLocker::Show() {
  NOTIMPLEMENTED();
}

// static
void ScreenLocker::Hide() {
  NOTIMPLEMENTED();
}

// static
void ScreenLocker::UnlockScreenFailed() {
  NOTIMPLEMENTED();
}

// static
void ScreenLocker::InitClass() {
  NOTIMPLEMENTED();
}

void ScreenLocker::ScreenLockReady() {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
