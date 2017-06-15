// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_REPEATED_NOTIFICATION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_REPEATED_NOTIFICATION_OBSERVER_H_

#include "base/run_loop.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {

// RepeatedNotificationObserver allows code to wait until specified number
// of notifications of particular type are posted.
class RepeatedNotificationObserver : public NotificationObserver {
 public:
  RepeatedNotificationObserver(int type, int count);

  // NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // Wait until |count| events of |type| (both specified in constructor) happen.
  void Wait();

 private:
  int num_outstanding_;
  NotificationRegistrar registrar_;
  bool running_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(RepeatedNotificationObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_REPEATED_NOTIFICATION_OBSERVER_H_
