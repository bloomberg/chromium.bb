// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NOTIFICATION_OBSERVER_MOCK_H_
#define CHROME_COMMON_NOTIFICATION_OBSERVER_MOCK_H_

#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

class NotificationObserverMock : public NotificationObserver {
 public:
  NotificationObserverMock() {}
  virtual ~NotificationObserverMock() {}

  MOCK_METHOD3(Observe, void(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details));
};

#endif  // CHROME_COMMON_NOTIFICATION_OBSERVER_MOCK_H_
