// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_listener.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

using MockCallback =
    testing::MockFunction<void(MemoryPressureListener::MemoryPressureLevel)>;

TEST(MemoryPressureListenerTest, NotifyMemoryPressure) {
  MessageLoopForUI message_loop;
  MockCallback callback;
  scoped_ptr<MemoryPressureListener> listener(new MemoryPressureListener(
      Bind(&MockCallback::Call, Unretained(&callback))));

  // Memory pressure notifications are not suppressed by default.
  EXPECT_FALSE(MemoryPressureListener::AreNotificationsSuppressed());
  EXPECT_CALL(callback,
              Call(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE))
      .Times(1);
  MemoryPressureListener::NotifyMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  message_loop.RunUntilIdle();

  // Enable suppressing memory pressure notifications.
  MemoryPressureListener::SetNotificationsSuppressed(true);
  EXPECT_TRUE(MemoryPressureListener::AreNotificationsSuppressed());
  EXPECT_CALL(callback, Call(testing::_)).Times(0);
  MemoryPressureListener::NotifyMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  message_loop.RunUntilIdle();

  // Disable suppressing memory pressure notifications.
  MemoryPressureListener::SetNotificationsSuppressed(false);
  EXPECT_FALSE(MemoryPressureListener::AreNotificationsSuppressed());
  EXPECT_CALL(callback,
              Call(MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL))
      .Times(1);
  MemoryPressureListener::NotifyMemoryPressure(
      MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  message_loop.RunUntilIdle();

  listener.reset();
}

}  // namespace base
