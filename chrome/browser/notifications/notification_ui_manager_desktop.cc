// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager.h"

#include <memory>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"

// static
NotificationUIManager* NotificationUIManager::Create() {
  return new MessageCenterNotificationManager(
      g_browser_process->message_center());
}
