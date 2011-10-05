// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_TEST_UTIL_H_
#pragma once

#include "chrome/browser/extensions/app_notification.h"

class AppNotificationManager;

namespace app_notification_test_util {

// Does a deep equality check of two AppNotificationList's, adding a gtest
// failure and logging at the first difference found.
void ExpectListsEqual(const AppNotificationList& one,
                      const AppNotificationList& two);

// Helper for inserting |count| dummy notifications with |prefix| in their
// title and body into |list|.
void AddNotifications(AppNotificationList* list,
                      int count,
                      std::string prefix);

// Copy the contents of |source| into a new object.
AppNotification* CopyAppNotification(const AppNotification& source);

// Adds a copy of each item in |list| to |manager| using |extension_id| as the
// extension id.
void AddCopiesFromList(AppNotificationManager* manager,
                       const std::string& extension_id,
                       const AppNotificationList& list);

}  // namespace app_notification_test_util

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_TEST_UTIL_H_
