// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager.h"

#include "chrome/browser/notifications/balloon_notification_ui_manager.h"

#if defined(ENABLE_MESSAGE_CENTER)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "ui/message_center/message_center_util.h"
#endif

// static
bool NotificationUIManager::DelegatesToMessageCenter() {
// MessageCenterNotificationManager doesn't work quite well with Ash
// right now. Until it matures in ChromeOS, delegating to message center will
// be done by BalloonCollectionImplAsh through BalloonNotificationUIManager.
// TODO(mukai): remove this |&& !defined(USE_ASH)| when that's no problem.
#if defined(ENABLE_MESSAGE_CENTER) && !defined(USE_ASH)
  return message_center::IsRichNotificationEnabled();
#endif
  return false;
}

#if !defined(OS_MACOSX)
// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
#if defined(ENABLE_MESSAGE_CENTER)
  if (DelegatesToMessageCenter())
    return new MessageCenterNotificationManager(
        g_browser_process->message_center());
#endif
  BalloonNotificationUIManager* balloon_manager =
      new BalloonNotificationUIManager(local_state);
  balloon_manager->SetBalloonCollection(BalloonCollection::Create());
  return balloon_manager;
}
#endif
