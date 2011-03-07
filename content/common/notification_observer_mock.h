// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NOTIFICATION_OBSERVER_MOCK_H_
#define CONTENT_COMMON_NOTIFICATION_OBSERVER_MOCK_H_
#pragma once

#include "content/common/notification_observer.h"
#include "content/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

class NotificationDetails;
class NotificationSource;

class NotificationObserverMock : public NotificationObserver {
 public:
  NotificationObserverMock();
  virtual ~NotificationObserverMock();

  MOCK_METHOD3(Observe, void(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details));
};

#endif  // CONTENT_COMMON_NOTIFICATION_OBSERVER_MOCK_H_
