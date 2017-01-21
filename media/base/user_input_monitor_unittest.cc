// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <memory>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "media/base/keyboard_event_counter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPoint.h"

#if defined(OS_LINUX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

namespace media {

TEST(UserInputMonitorTest, CreatePlatformSpecific) {
#if defined(OS_LINUX)
  base::MessageLoopForIO message_loop;
  base::FileDescriptorWatcher file_descriptor_watcher(&message_loop);
#else
  base::MessageLoopForUI message_loop;
#endif  // defined(OS_LINUX)

  base::RunLoop run_loop;
  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      message_loop.task_runner(), message_loop.task_runner());

  if (!monitor)
    return;

  MockMouseListener listener;
  // Ignore any callbacks.
  EXPECT_CALL(listener, OnMouseMoved(testing::_)).Times(testing::AnyNumber());

  monitor->EnableKeyPressMonitoring();
  monitor->DisableKeyPressMonitoring();

  monitor.reset();
  run_loop.RunUntilIdle();
}

}  // namespace media
