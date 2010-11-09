// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MOCK_NOTIFICATION_OBSERVER_H_
#define CHROME_TEST_MOCK_NOTIFICATION_OBSERVER_H_
#pragma once

#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockNotificationObserver : public NotificationObserver {
 public:
  MOCK_METHOD3(Observe, void(NotificationType,
                             const NotificationSource&,
                             const NotificationDetails&));
};

#endif  // CHROME_TEST_MOCK_NOTIFICATION_OBSERVER_H_
