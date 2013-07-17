// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process.h"

#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/common/service_process_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ServiceProcessTest, DISABLED_Run) {
  base::MessageLoopForUI main_message_loop;
  ServiceProcess process;
  ServiceProcessState state;
  CommandLine command_line(CommandLine::NO_PROGRAM);
  EXPECT_TRUE(process.Initialize(&main_message_loop, command_line, &state));
  EXPECT_TRUE(process.Teardown());
}
