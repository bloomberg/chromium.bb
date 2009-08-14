// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_LOG_UNITTEST_H_
#define NET_BASE_LOAD_LOG_UNITTEST_H_

#include "net/base/load_log.h"

namespace net {

// Create a timestamp with internal value of |t| milliseconds from the epoch.
base::TimeTicks MakeTime(int t);

// Call gtest's EXPECT_* to verify that |log| contains the specified entry
// at index |i|.
void ExpectLogContains(const LoadLog* log,
                       size_t i,
                       base::TimeTicks expected_time,
                       LoadLog::EventType expected_event,
                       LoadLog::EventPhase expected_phase);

// Same as above, but without an expectation for the timestamp.
void ExpectLogContains(const LoadLog* log,
                       size_t i,
                       LoadLog::EventType expected_event,
                       LoadLog::EventPhase expected_phase);

}  // namespace net

#endif  // NET_BASE_LOAD_LOG_UNITTEST_H_
