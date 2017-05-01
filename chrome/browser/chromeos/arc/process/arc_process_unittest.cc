// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/process/arc_process.h"

#include <assert.h>
#include <list>

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// Tests that ArcProcess objects can be sorted by their priority (higher to
// lower). This is critical for the OOM handler to work correctly.
TEST(ArcProcess, TestSorting) {
  constexpr int64_t kNow = 1234567890;

  std::list<ArcProcess> processes;  // use list<> for emplace_front.
  processes.emplace_back(0, 0, "process 0", mojom::ProcessState::PERSISTENT,
                         false /* is_foreground */, kNow + 1);
  processes.emplace_front(1, 1, "process 1", mojom::ProcessState::PERSISTENT,
                          false, kNow);
  processes.emplace_back(2, 2, "process 2", mojom::ProcessState::LAST_ACTIVITY,
                         false, kNow);
  processes.emplace_front(3, 3, "process 3", mojom::ProcessState::LAST_ACTIVITY,
                          false, kNow + 1);
  processes.emplace_back(4, 4, "process 4", mojom::ProcessState::CACHED_EMPTY,
                         false, kNow + 1);
  processes.emplace_front(5, 5, "process 5", mojom::ProcessState::CACHED_EMPTY,
                          false, kNow);
  processes.sort();

  static_assert(
      mojom::ProcessState::PERSISTENT < mojom::ProcessState::LAST_ACTIVITY,
      "unexpected enum values");
  static_assert(
      mojom::ProcessState::LAST_ACTIVITY < mojom::ProcessState::CACHED_EMPTY,
      "unexpected enum values");

  std::list<ArcProcess>::const_iterator it = processes.begin();
  // 0 should have higher priority since its last_activity_time is more recent.
  EXPECT_EQ(0, it->pid());
  ++it;
  EXPECT_EQ(1, it->pid());
  ++it;
  // Same, 3 should have higher priority.
  EXPECT_EQ(3, it->pid());
  ++it;
  EXPECT_EQ(2, it->pid());
  ++it;
  // Same, 4 should have higher priority.
  EXPECT_EQ(4, it->pid());
  ++it;
  EXPECT_EQ(5, it->pid());
}

}  // namespace

}  // namespace arc
