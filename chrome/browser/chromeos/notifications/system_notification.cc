// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/system_notification.h"

#include "base/move.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/notifications/system_notification_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

namespace chromeos {

SystemNotification::SystemNotification(Profile* profile, std::string id,
                                       string16 title)
  : profile_(profile),
    collection_(static_cast<BalloonCollectionImpl*>(
      g_browser_process->notification_ui_manager()->balloon_collection())),
    delegate_(new Delegate(base::move(id))),
    title_(move(title)),
    visible_(false) {}

SystemNotification::~SystemNotification() {
  Hide();
}

void SystemNotification::Show(const string16& message) {
  Notification notify = SystemNotificationFactory::Create(GURL(),
      title_, message, delegate_.get());
  if (visible_) {
    collection_->UpdateNotification(notify);
  } else {
    collection_->AddSystemNotification(notify, profile_, true /* sticky */,
                                       false /* no controls */);
    visible_ = true;
  }
}

void SystemNotification::Hide() {
  if (visible_) {
    collection_->Remove(Notification(GURL(), GURL(), std::wstring(),
                                     delegate_.get()));

    visible_ = false;
  }
}

}  // namespace chromeos

