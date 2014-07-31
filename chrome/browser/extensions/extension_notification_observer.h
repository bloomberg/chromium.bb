// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_NOTIFICATION_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_NOTIFICATION_OBSERVER_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Records LOADED, INSTALLED, and UNLOADED notifications for extensions with
// specific IDs. Use in tests only.
class ExtensionNotificationObserver : public content::NotificationObserver {
 public:
  ExtensionNotificationObserver(content::NotificationSource source,
                                const std::set<std::string>& extension_ids);

  virtual ~ExtensionNotificationObserver();

  // Each of these methods returns a testing::AssertionSuccess if exactly those
  // notifications occurred for any extensions in |extension_ids|, and no more,
  // since the last time any of these methods were called.
  testing::AssertionResult CheckNotifications() WARN_UNUSED_RESULT;
  testing::AssertionResult CheckNotifications(extensions::NotificationType type)
      WARN_UNUSED_RESULT;
  testing::AssertionResult CheckNotifications(extensions::NotificationType t1,
                                              extensions::NotificationType t2)
      WARN_UNUSED_RESULT;
  testing::AssertionResult CheckNotifications(extensions::NotificationType t1,
                                              extensions::NotificationType t2,
                                              extensions::NotificationType t3)
      WARN_UNUSED_RESULT;
  testing::AssertionResult CheckNotifications(extensions::NotificationType t1,
                                              extensions::NotificationType t2,
                                              extensions::NotificationType t3,
                                              extensions::NotificationType t4,
                                              extensions::NotificationType t5,
                                              extensions::NotificationType t6)
      WARN_UNUSED_RESULT;

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Checks then clears notifications for our extensions.
  testing::AssertionResult CheckNotifications(
      const std::vector<extensions::NotificationType>& types);

  const std::set<std::string> extension_ids_;
  std::vector<extensions::NotificationType> notifications_;
  content::NotificationRegistrar registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_NOTIFICATION_OBSERVER_H_
