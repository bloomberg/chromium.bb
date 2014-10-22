// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class NotificationUIManager;

// The content::NotificationObserver observes system status change and sends
// events to NotificationUIManager.
class NotificationSystemObserver : public content::NotificationObserver {
 public:
  explicit NotificationSystemObserver(NotificationUIManager* ui_manager);
  ~NotificationSystemObserver() override;

 protected:
  // content::NotificationObserver override.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // Registrar for the other kind of notifications (event signaling).
  content::NotificationRegistrar registrar_;
  NotificationUIManager* ui_manager_;

  DISALLOW_COPY_AND_ASSIGN(NotificationSystemObserver);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_
