// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/system_notification.h"

#include "base/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/notifications/balloon_collection_impl_aura.h"
#include "chrome/browser/chromeos/notifications/system_notification_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

void SystemNotification::Init(int icon_resource_id) {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  collection_ = static_cast<BalloonCollectionImplType*>(
      g_browser_process->notification_ui_manager()->balloon_collection());
  std::string url = web_ui_util::GetImageDataUrlFromResource(icon_resource_id);
  DCHECK(!url.empty());
  GURL tmp_gurl(url);
  icon_.Swap(&tmp_gurl);
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
      sticky_(false),
      urgent_(false),
      show_on_unlock_(false) {
  Init(icon_resource_id);
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
      sticky_(false),
      urgent_(false),
      show_on_unlock_(false) {
  Init(icon_resource_id);
}

SystemNotification::~SystemNotification() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void SystemNotification::UnlockScreen() {
  if (show_on_unlock_) {
    DCHECK(!visible_);
    Notification notify = SystemNotificationFactory::Create(
        icon_, title_, message_, link_, delegate_.get());
    ShowNotification(notify);
  }
}

void SystemNotification::Show(const string16& message,
                              bool urgent,
                              bool sticky) {
  Show(message, string16(), BalloonViewHost::MessageCallback(), urgent, sticky);
}

void SystemNotification::Show(const string16& message,
                              const string16& link,
                              const BalloonViewHost::MessageCallback& callback,
                              bool urgent,
                              bool sticky) {
  message_ = message;
  link_ = link;
  callback_ = callback;
  sticky_ = sticky;

  if (DBusThreadManager::Get()->GetPowerManagerClient()->GetIsScreenLocked()) {
    if (visible_ && urgent && !urgent_) {
      // Hide the notification so that we show/update it on unlock.
      Hide();
      urgent_ = true;
    }
    if (!visible_)
      show_on_unlock_ = true;
    return;
  }

  Notification notify = SystemNotificationFactory::Create(
      icon_, title_, message_, link_, delegate_.get());
  if (visible_) {
    // Force showing a user hidden notification on an urgent transition.
    if (urgent && !urgent_) {
      if (!collection_->UpdateAndShowNotification(notify))
        visible_ = false;  // re-show
    } else {
      collection_->UpdateNotification(notify);
    }
  }
  if (!visible_)
    ShowNotification(notify);
  urgent_ = urgent;
}

void SystemNotification::ShowNotification(const Notification& notify) {
  collection_->AddSystemNotification(notify, profile_, sticky_);
  collection_->AddWebUIMessageCallback(notify, "link", callback_);
  visible_ = true;
}

void SystemNotification::Hide() {
  if (visible_) {
    collection_->RemoveById(delegate_->id());
    visible_ = false;
    urgent_ = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
// SystemNotification::Delegate

SystemNotification::Delegate::Delegate(const std::string& id)
    : id_(id) {
}

std::string SystemNotification::Delegate::id() const {
  return id_;
}

}  // namespace chromeos
