// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/notifications/non_persistent_notification_handler.h"

// Handler for notifications shown by extensions. Will be created and owned by
// the NotificationDisplayService.
class ExtensionNotificationHandler : public NonPersistentNotificationHandler {
 public:
  ExtensionNotificationHandler();
  ~ExtensionNotificationHandler() override;

  // NotificationHandler implementation.
  void OpenSettings(Profile* profile) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionNotificationHandler);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_HANDLER_H_
