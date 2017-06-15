// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/repeated_notification_observer.h"

#include "base/auto_reset.h"
#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

RepeatedNotificationObserver::RepeatedNotificationObserver(int type, int count)
    : num_outstanding_(count), running_(false) {
  registrar_.Add(this, type, NotificationService::AllSources());
}

void RepeatedNotificationObserver::Observe(int type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  ASSERT_GT(num_outstanding_, 0);
  if (!--num_outstanding_ && running_)
    run_loop_.QuitWhenIdle();
}

void RepeatedNotificationObserver::Wait() {
  if (num_outstanding_ <= 0)
    return;

  {
    DCHECK(!running_);
    base::AutoReset<bool> auto_reset(&running_, true);
    run_loop_.Run();
  }
}

}  // namespace content
