// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_LAUNCH_ID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_LAUNCH_ID_H_

#include "chrome/browser/notifications/notification_handler.h"
#include "url/gurl.h"

// This class encapsulates the launch id strings that are passed back and forth
// between Chrome and the Windows Action Center and contain the necessary info
// to figure out which notification was activated in the Action Center.
class NotificationLaunchId {
 public:
  NotificationLaunchId();
  NotificationLaunchId(const NotificationLaunchId& other);

  NotificationLaunchId(NotificationHandler::Type notification_type,
                       const std::string& notification_id,
                       const std::string& profile_id,
                       bool incognito,
                       const GURL& origin_url);

  // A constructor used to parse an encoded string we get back from the Action
  // Center. Callers must use is_valid() to check if decoding was successful.
  explicit NotificationLaunchId(const std::string& encoded);

  ~NotificationLaunchId() = default;

  NotificationLaunchId& operator=(const NotificationLaunchId& other) = default;

  bool is_valid() const { return is_valid_; }

  std::string Serialize() const;

  void set_button_index(int index) { button_index_ = index; }

  NotificationHandler::Type notification_type() const {
    DCHECK(is_valid());
    return notification_type_;
  }
  const std::string& notification_id() const {
    DCHECK(is_valid());
    return notification_id_;
  }
  const std::string& profile_id() const {
    DCHECK(is_valid());
    return profile_id_;
  }
  bool incognito() const {
    DCHECK(is_valid());
    return incognito_;
  }
  const GURL& origin_url() const {
    DCHECK(is_valid());
    return origin_url_;
  }
  int button_index() const {
    DCHECK(is_valid());
    return button_index_;
  }

 private:
  NotificationHandler::Type notification_type_;
  std::string notification_id_;
  std::string profile_id_;
  bool incognito_ = false;
  GURL origin_url_;
  int button_index_ = -1;

  bool is_valid_ = false;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_LAUNCH_ID_H_
