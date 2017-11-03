// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/strings/string16.h"

class GURL;
class Profile;

// Interface that enables the different kind of notifications to process
// operations coming from the user or decisions made by the underlying
// notification type.
class NotificationHandler {
 public:
  virtual ~NotificationHandler() {}

  // Called after displaying a toast in case the caller needs some processing.
  virtual void OnShow(Profile* profile, const std::string& notification_id) {}

  // Process notification close events.
  virtual void OnClose(Profile* profile,
                       const GURL& origin,
                       const std::string& notification_id,
                       bool by_user) {}

  // Process cliks to a notification or its buttons, depending on
  // |action_index|.
  virtual void OnClick(Profile* profile,
                       const GURL& origin,
                       const std::string& notification_id,
                       const base::Optional<int>& action_index,
                       const base::Optional<base::string16>& reply) = 0;

  // Open notification settings.
  virtual void OpenSettings(Profile* profile) {}
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_HANDLER_H_
