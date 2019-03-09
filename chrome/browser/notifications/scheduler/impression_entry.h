// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_ENTRY_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_ENTRY_H_

#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_types.h"

namespace notifications {

// Contains data about the impression of a certain notification from the user's
// perspective.
struct ImpressionEntry {
  ImpressionEntry();
  ~ImpressionEntry();

  // Creation timestamp.
  base::Time create_time;

  // The user action that generates the impression result.
  UserFeedback user_feedback;

  // The impression result.
  ImpressionResult impression;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_IMPRESSION_ENTRY_H_
