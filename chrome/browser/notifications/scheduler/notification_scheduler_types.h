// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_TYPES_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_TYPES_H_

namespace notifications {

// The type of a list of clients using the notification scheduler system.
enum class SchedulerClientType {
  PLACE_HOLDER,
};

// The type of user feedback from a displayed notification.
enum class UserFeedback {
  // Unknown feedback from the user.
  kUnknown = 0,
  // The user taps the helpful button, potentially a strong indicator of user's
  // positive preference on the notification.
  kHelpful = 1,
  // The user taps the unhelpful button, potentially a strong indicator of
  // user's negative preference on the notification.
  kNotHelpful = 2,
  // The user clicks the body of the notification.
  kBodyClick = 3,
  // The user has no interaction with the notification for a while.
  kIgnore = 4,
  kMaxValue = kIgnore
};

// The user impression result of a particular notification.
enum class ImpressionResult {
  // Unknown user impression.
  kUnknown = 0,
  // Positive user impression that the user may like the notification.
  kPositive = 1,
  // Positive user impression that the user may dislike the notification.
  kNegative = 2,
  // The feedback is netural to the user.
  kNetural = 3,
  kMaxValue = kNetural
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_SCHEDULER_TYPES_H_
