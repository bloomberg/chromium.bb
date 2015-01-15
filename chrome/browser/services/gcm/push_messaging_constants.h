// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_CONSTANTS_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_CONSTANTS_H_

namespace gcm {

extern const char kPushMessagingEndpoint[];

// The tag of the notification that will be automatically shown if a webapp
// receives a push message then fails to show a notification.
extern const char kPushMessagingForcedNotificationTag[];

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_CONSTANTS_H_
