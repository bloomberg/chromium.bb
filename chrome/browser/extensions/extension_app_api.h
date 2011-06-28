// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

#include <map>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"

struct AppNotification {
  AppNotification();
  ~AppNotification();

  std::string extension_id;
  std::string title;
  std::string body;
  GURL linkUrl;
  std::string linkText;
  SkBitmap icon;
};

// A list of AppNotification's.
typedef std::vector<linked_ptr<AppNotification> > AppNotificationList;

// This class keeps track of notifications for installed apps.
class AppNotificationManager : public NotificationObserver {
 public:
  AppNotificationManager();
  virtual ~AppNotificationManager();

  // Takes ownership of |notification|.
  void Add(AppNotification* item);

  const AppNotificationList* GetAll(const std::string& extension_id);

  // Returns the most recently added notification for |extension_id| if there
  // are any, or NULL otherwise.
  const AppNotification* GetLast(const std::string& extension_id);

  // Clears all notifications for |extension_id|.
  void ClearAll(const std::string& extension_id);

  // Implementing NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  // Maps extension id to a list of notifications for that extension.
  typedef std::map<std::string, AppNotificationList> NotificationMap;

  NotificationRegistrar registrar_;
  NotificationMap notifications_;

  DISALLOW_COPY_AND_ASSIGN(AppNotificationManager);
};

class AppNotifyFunction : public SyncExtensionFunction {
  virtual ~AppNotifyFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.app.notify");
};

class AppClearAllNotificationsFunction : public SyncExtensionFunction {
  virtual ~AppClearAllNotificationsFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.app.clearAllNotifications");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_API_H__
