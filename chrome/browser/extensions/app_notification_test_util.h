// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_TEST_UTIL_H_

#include "chrome/browser/extensions/app_notification.h"

namespace extensions {
class AppNotificationManager;
}

namespace app_notification_test_util {

// Does a deep equality check of two AppNotificationList's, adding a gtest
// failure and logging at the first difference found.
void ExpectListsEqual(const extensions::AppNotificationList& one,
                      const extensions::AppNotificationList& two);

// Helper for inserting |count| dummy notifications with |prefix| in their
// title and body into |list|.
void AddNotifications(extensions::AppNotificationList* list,
                      const std::string& extension_id,
                      int count,
                      const std::string& prefix);

// Adds a copy of each item in |list| to |manager|.
bool AddCopiesFromList(extensions::AppNotificationManager* manager,
                       const extensions::AppNotificationList& list);

}  // namespace app_notification_test_util

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_TEST_UTIL_H_
