// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/balloon_collection_impl_aura.h"

#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/ui/views/notifications/balloon_view.h"
#include "chrome/browser/ui/views/notifications/balloon_view_host.h"

namespace chromeos {

BalloonCollectionImplAura::BalloonCollectionImplAura() {
}

BalloonCollectionImplAura::~BalloonCollectionImplAura() {
}

bool BalloonCollectionImplAura::AddWebUIMessageCallback(
    const Notification& notification,
    const std::string& message,
    const BalloonViewHost::MessageCallback& callback) {
  Balloon* balloon = base().FindBalloon(notification);
  if (!balloon)
    return false;

  BalloonViewHost* host =
      static_cast<chromeos::BalloonViewHost*>(balloon->view()->GetHost());
  return host->AddWebUIMessageCallback(message, callback);
}

void BalloonCollectionImplAura::AddSystemNotification(
    const Notification& notification,
    Profile* profile,
    bool sticky) {
  system_notifications_.insert(notification.notification_id());

  // Add balloons to the front of the stack. This ensures that system
  // notifications will always be displayed. NOTE: This has the side effect
  // that system notifications are displayed in inverse order, with the most
  // recent notification always at the front of the list.
  AddImpl(notification, profile, true /* add to front*/);
}

bool BalloonCollectionImplAura::UpdateNotification(
    const Notification& notification) {
  Balloon* balloon = base().FindBalloon(notification);
  if (!balloon)
    return false;
  balloon->Update(notification);
  return true;
}

bool BalloonCollectionImplAura::UpdateAndShowNotification(
    const Notification& notification) {
  return UpdateNotification(notification);
}

Balloon* BalloonCollectionImplAura::MakeBalloon(
    const Notification& notification, Profile* profile) {
  Balloon* balloon = new Balloon(notification, profile, this);
  ::BalloonViewImpl* balloon_view = new ::BalloonViewImpl(this);
  if (system_notifications_.find(notification.notification_id()) !=
      system_notifications_.end())
    balloon_view->set_enable_web_ui(true);
  balloon->set_view(balloon_view);
  gfx::Size size(layout().min_balloon_width(), layout().min_balloon_height());
  balloon->set_content_size(size);
  return balloon;
}

}  // namespace chromeos

// static
BalloonCollection* BalloonCollection::Create() {
  return new chromeos::BalloonCollectionImplAura();
}
