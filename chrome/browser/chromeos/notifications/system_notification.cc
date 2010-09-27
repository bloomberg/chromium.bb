// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/system_notification.h"

#include "app/resource_bundle.h"
#include "base/base64.h"
#include "base/move.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/notifications/system_notification_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

namespace chromeos {

SystemNotification::SystemNotification(Profile* profile, std::string id,
                                       int icon_resource_id, string16 title)
  : profile_(profile),
    collection_(static_cast<BalloonCollectionImpl*>(
      g_browser_process->notification_ui_manager()->balloon_collection())),
    delegate_(new Delegate(base::move(id))),
    title_(move(title)),
    visible_(false),
    urgent_(false) {
  // Load resource icon and covert to base64 encoded data url
  scoped_refptr<RefCountedMemory> raw_icon(ResourceBundle::GetSharedInstance().
      LoadDataResourceBytes(icon_resource_id));
  std::string str_gurl;
  std::copy(raw_icon->front(), raw_icon->front() + raw_icon->size(),
            std::back_inserter(str_gurl));
  base::Base64Encode(str_gurl, &str_gurl);
  str_gurl.insert(0, "data:image/png;base64,");
  GURL tmp_gurl(str_gurl);
  icon_.Swap(&tmp_gurl);
}

SystemNotification::~SystemNotification() {
  Hide();
}

void SystemNotification::Show(const string16& message,
                              bool urgent,
                              bool sticky) {
  Notification notify = SystemNotificationFactory::Create(icon_,
      title_, message, delegate_.get());
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
  }
  visible_ = true;
  urgent_ = urgent;
}

void SystemNotification::Hide() {
  if (visible_) {
    collection_->Remove(Notification(GURL(), GURL(), string16(), string16(),
                                     delegate_.get()));

    visible_ = false;
    urgent_ = false;
  }
}

}  // namespace chromeos
