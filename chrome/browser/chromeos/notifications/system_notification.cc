// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/system_notification.h"

#include "base/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/notifications/system_notification_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "content/browser/webui/web_ui_util.h"

namespace chromeos {

void SystemNotification::Init(int icon_resource_id) {
  collection_ = static_cast<BalloonCollectionImpl*>(
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
      urgent_(false) {
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
      urgent_(false) {
  Init(icon_resource_id);
}

SystemNotification::~SystemNotification() {
}

void SystemNotification::Show(const string16& message,
                              bool urgent,
                              bool sticky) {
  Show(message, string16(), NULL, urgent, sticky);
}

void SystemNotification::Show(const string16& message,
                              const string16& link,
                              MessageCallback* callback,
                              bool urgent,
                              bool sticky) {
  Notification notify = SystemNotificationFactory::Create(icon_,
      title_, message, link, delegate_.get());
  if (visible_) {
    // Force showing a user hidden notification on an urgent transition.
    if (urgent && !urgent_) {
      collection_->UpdateAndShowNotification(notify);
    } else {
      collection_->UpdateNotification(notify);
    }
  } else {
    collection_->AddSystemNotification(notify, profile_,
                                       sticky,
                                       false /* no controls */);
    collection_->AddWebUIMessageCallback(notify, "link", callback);
  }
  visible_ = true;
  urgent_ = urgent;
}

void SystemNotification::Hide() {
  if (visible_) {
    collection_->RemoveById(delegate_->id());
    visible_ = false;
    urgent_ = false;
  }
}

}  // namespace chromeos
