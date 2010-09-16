// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_timeouts.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/test/test_switches.h"

namespace {

void InitializeTimeout(const char* switch_name, int* value) {
  DCHECK(value);
  if (CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    std::string string_value(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switch_name));
    int timeout;
    base::StringToInt(string_value, &timeout);
    *value = std::max(*value, timeout);
  }
}

}  // namespace

// static
bool TestTimeouts::initialized_ = false;

// The timeout values should increase in the order they appear in this block.
// static
int TestTimeouts::action_timeout_ms_ = 2000;
int TestTimeouts::action_max_timeout_ms_ = 15000;
int TestTimeouts::large_test_timeout_ms_ = 3 * 60 * 1000;
int TestTimeouts::huge_test_timeout_ms_ = 10 * 60 * 1000;

// static
int TestTimeouts::sleep_timeout_ms_ = 2000;

// static
int TestTimeouts::command_execution_timeout_ms_ = 25000;

// static
int TestTimeouts::wait_for_terminate_timeout_ms_ = 15000;

// static
int TestTimeouts::live_operation_timeout_ms_ = 30000;

// static
void TestTimeouts::Initialize() {
  if (initialized_) {
    NOTREACHED();
    return;
  }
  initialized_ = true;

  // The timeout values should be increasing in the right order, and while
  // initially they are, overrides may not respect this. Move the checks to
  // before the point at which they get overridden while all build scripts
  // are updated.
  // TODO(robertshield): Move these checks back once the bots are updated.
  CHECK(action_timeout_ms_ <= action_max_timeout_ms_);
  CHECK(action_max_timeout_ms_ <= large_test_timeout_ms_);
  CHECK(large_test_timeout_ms_ <= huge_test_timeout_ms_);

  InitializeTimeout(switches::kUiTestActionTimeout, &action_timeout_ms_);
  InitializeTimeout(switches::kUiTestActionMaxTimeout, &action_max_timeout_ms_);
  InitializeTimeout(switches::kTestLargeTimeout, &large_test_timeout_ms_);
  InitializeTimeout(switches::kUiTestTimeout, &huge_test_timeout_ms_);

  InitializeTimeout(switches::kUiTestSleepTimeout, &sleep_timeout_ms_);
  InitializeTimeout(switches::kUiTestCommandExecutionTimeout,
                    &command_execution_timeout_ms_);
  InitializeTimeout(switches::kUiTestTerminateTimeout,
                    &wait_for_terminate_timeout_ms_);

  InitializeTimeout(switches::kLiveOperationTimeout,
                    &live_operation_timeout_ms_);
}
