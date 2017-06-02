// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_

#include <memory>
#include <string>

namespace base {
class NullableString16;
}

class Profile;

// Interface that enables the different kind of notifications to process
// operations coming from the user or decisions made by the underlying
// notification type.
class NotificationHandler {
 public:
  virtual ~NotificationHandler() {}

  virtual void OnShow(Profile* profile, const std::string& notification_id) = 0;

  // Process notification close events.
  virtual void OnClose(Profile* profile,
                       const std::string& origin,
                       const std::string& notification_id,
                       bool by_user) = 0;

  // Process cliks to a notification or its buttons, depending on
  // |action_index|.
  virtual void OnClick(Profile* profile,
                       const std::string& origin,
                       const std::string& notification_id,
                       int action_index,
                       const base::NullableString16& reply) = 0;

  // Open notification settings.
  virtual void OpenSettings(Profile* profile) = 0;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_
