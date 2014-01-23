// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/welcome_delegate.h"
#include "chrome/browser/profiles/profile.h"

namespace notifier {

WelcomeDelegate::WelcomeDelegate(const std::string& notification_id,
                                 Profile* profile,
                                 const message_center::NotifierId notifier_id)
    : notification_id_(notification_id),
      profile_(profile),
      notifier_id_(notifier_id) {
  DCHECK_EQ(message_center::NotifierId::SYNCED_NOTIFICATION_SERVICE,
            notifier_id.type);
}

WelcomeDelegate::~WelcomeDelegate() {}

void WelcomeDelegate::Display() {}

void WelcomeDelegate::Error() {}

void WelcomeDelegate::Close(bool by_user) {}

void WelcomeDelegate::Click() {}

void WelcomeDelegate::ButtonClick(int button_index) {
  // TODO(dewittj): Implement disable-on-click.
  NOTIMPLEMENTED();
}

std::string WelcomeDelegate::id() const { return notification_id_; }

content::RenderViewHost* WelcomeDelegate::GetRenderViewHost() const {
  return NULL;
}

}  // namespace notifier
