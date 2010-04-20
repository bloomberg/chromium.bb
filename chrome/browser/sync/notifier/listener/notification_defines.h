// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_

#include <string>

struct NotificationData {
  std::string service_url;
  std::string service_specific_data;
};

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_NOTIFICATION_DEFINES_H_

