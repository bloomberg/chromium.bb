// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/system_notification.h"
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

}  // namespace chromeos
