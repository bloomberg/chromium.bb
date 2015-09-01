// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification.h"

#include "chrome/browser/download/notification/download_notification_manager.h"

namespace {

///////////////////////////////////////////////////////////////////////////////
// NotificationWatcher class:
///////////////////////////////////////////////////////////////////////////////

class NotificationWatcher : public NotificationDelegate {
 public:
  explicit NotificationWatcher(DownloadNotification* item) : item_(item) {}

  // NotificationDelegate overrides:
  void Close(bool by_user) override {
    item_->OnNotificationClose();
  }

  void Click() override {
    item_->OnNotificationClick();
  }

  bool HasClickedListener() override {
    return item_->HasNotificationClickedListener();
  }

  void ButtonClick(int button_index) override {
    item_->OnNotificationButtonClick(button_index);
  }

  std::string id() const override {
    return item_->GetNotificationId();
  }

 private:
  ~NotificationWatcher() override {}

  DownloadNotification* item_;

  DISALLOW_COPY_AND_ASSIGN(NotificationWatcher);
};

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// DownloadNotification implementation:
///////////////////////////////////////////////////////////////////////////////

// static
const char DownloadNotification::kDownloadNotificationOrigin[] =
    "chrome://downloads";

DownloadNotification::DownloadNotification()
    : watcher_(new NotificationWatcher(this)) {}

DownloadNotification::~DownloadNotification() {}

bool DownloadNotification::HasNotificationClickedListener() {
  // True by default.
  return true;
}

NotificationDelegate* DownloadNotification::watcher() const {
  return watcher_.get();
}

void DownloadNotification::InvokeUnsafeForceNotificationFlush(
    message_center::MessageCenter* message_center, const std::string& id) {
  DCHECK(message_center);
  message_center->ForceNotificationFlush(id);
}
